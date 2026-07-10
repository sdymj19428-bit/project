import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
# from geometry_msgs.msg import PoseStamped  # 사람 위치 메시지 주석 처리
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import pyrealsense2 as rs
import numpy as np
import cv2
import time
from ultralytics import YOLO

class HumanFollowerTilted(Node):
    def __init__(self):
        super().__init__('human_follower_tilted')

        # 1. 퍼블리셔 설정
        self.cmd_pub = self.create_publisher(Twist, '/cmd_vel', 10)
        # self.pose_pub = self.create_publisher(PoseStamped, '/worker/pose', 10)  # 위치 퍼블리셔 주석 처리
        self.image_pub = self.create_publisher(Image, '/worker/debug_image', 10)
        self.bridge = CvBridge()

        # 2. 카메라 하드웨어 리셋 및 초기화
        ctx = rs.context()
        devices = ctx.query_devices()
        if len(devices) > 0:
            self.get_logger().info('RealSense 카메라 리셋 중...')
            devices[0].hardware_reset()
            time.sleep(5.0)  # 리셋 후 재연결 대기

        # 3. YOLOv5nu 및 리얼센스 파이프라인 설정
        self.model = YOLO('yolov5nu.pt')
        self.pipeline = rs.pipeline()
        config = rs.config()
        config.enable_stream(rs.stream.color, 424, 240, rs.format.bgr8, 15)
        config.enable_stream(rs.stream.depth, 424, 240, rs.format.z16, 15)

        profile = self.pipeline.start(config)
        self.align = rs.align(rs.stream.color)
        self.intrinsics = profile.get_stream(rs.stream.color).as_video_stream_profile().get_intrinsics()

        # 4. 카메라 설치 오프셋 및 각도
        self.cam_offset_x = 0.05  # 중심(바퀴축)에서 앞단 카메라 렌즈까지 거리
        self.cam_offset_z = 0.14  # 지면에서 14cm 높이
        self.tilt_angle = np.radians(45) # 45도 상향 각도

        self.create_timer(0.1, self.main_loop)
        self.get_logger().info('45도 경사 보정 추종 시스템 가동 시작! (위치 발행 제외)')

    def main_loop(self):
        try:
            frames = self.pipeline.wait_for_frames(1000)
            aligned_frames = self.align.process(frames)
            color_frame = aligned_frames.get_color_frame()
            depth_frame = aligned_frames.get_depth_frame()

            if not color_frame or not depth_frame: return
            
            img = np.asanyarray(color_frame.get_data())
            debug_img = img.copy()

            # YOLOv5nu 추론
            results = self.model(img, classes=0, conf=0.6, imgsz=320, verbose=False)
            
            target_person = None
            best_box = None
            max_area = 0

            # 타겟 선정 필터링
            for r in results:
                for box in r.boxes:
                    x1, y1, x2, y2 = map(int, box.xyxy[0].cpu().numpy())
                    w, h = x2 - x1, y2 - y1
                    area = w * h
                    
                    cv2.rectangle(debug_img, (x1, y1), (x2, y2), (0, 255, 0), 1)

                    if area > 2000 and (h/w) > 0.8 and area > max_area:
                        max_area = area
                        target_person = (int((x1+x2)/2), int((y1+y2)/2))
                        best_box = (x1, y1, x2, y2)

            # 타겟 시각화 및 영상 발행
            if best_box:
                cv2.rectangle(debug_img, (best_box[0], best_box[1]), (best_box[2], best_box[3]), (0, 0, 255), 3)
            img_msg = self.bridge.cv2_to_imgmsg(debug_img, encoding="bgr8")
            self.image_pub.publish(img_msg)

            cmd_msg = Twist()
            if target_person:
                u, v = target_person
                dist = depth_frame.get_distance(u, v)

                if dist > 0.1:
                    # 카메라 좌표계 기준 대각선 점
                    point = rs.rs2_deproject_pixel_to_point(self.intrinsics, [u, v], dist)
                    c_x, c_y, c_z = point

                    # 45도 회전 보정
                    cos_a = np.cos(self.tilt_angle)
                    sin_a = np.sin(self.tilt_angle)
                    true_forward = c_z * cos_a - c_y * sin_a
                    
                    # 수평 거리 기준 로봇과 사람의 실제 거리
                    ros_x = true_forward + self.cam_offset_x

                    # Pose 발행 부분 주석 처리
                    # true_upward = c_z * sin_a + c_y * cos_a
                    # ros_y = -c_x
                    # ros_z = -true_upward + self.cam_offset_z
                    # self.publish_pose(ros_x, ros_y, ros_z)

                    # --- 제어 로직 ---
                    # 1. 회전 제어
                    error_angular = u - 212
                    if abs(error_angular) > 20:
                        target_ang_vel = -float(error_angular) / 250.0 
                        cmd_msg.angular.z = max(-0.4, min(0.4, target_ang_vel))

                    # 2. 전진 제어 (1.0m 유지, 후진 기능 없음)
                    error_linear = ros_x - 1.0
                    if error_linear > 0.08:
                        cmd_msg.linear.x = min(error_linear * 0.4, 0.2)
                    else:
                        cmd_msg.linear.x = 0.0

                    self.get_logger().info(f"추종 중 - 타겟 거리: {ros_x:.2f}m")
            
            self.cmd_pub.publish(cmd_msg)

        except Exception as e:
            self.get_logger().warn(f'프레임 처리 오류: {e}')

    # 위치 토픽 발행 함수 전체 주석 처리
    # def publish_pose(self, x, y, z):
    #     msg = PoseStamped()
    #     msg.header.stamp = self.get_clock().now().to_msg()
    #     msg.header.frame_id = 'base_link'
    #     msg.pose.position.x = float(x)
    #     msg.pose.position.y = float(y)
    #     msg.pose.position.z = float(z)
    #     msg.pose.orientation.w = 1.0
    #     self.pose_pub.publish(msg)

    def stop(self):
        self.pipeline.stop()

def main():
    rclpy.init()
    node = HumanFollowerTilted()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.stop()
        node.destroy_node()
        if rclpy.ok(): rclpy.shutdown()

if __name__ == '__main__':
    main()
