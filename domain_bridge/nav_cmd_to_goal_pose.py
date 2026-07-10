import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient

from geometry_msgs.msg import PoseStamped, Twist  # Twist 추가
from nav2_msgs.action import NavigateToPose
from std_msgs.msg import Bool

class NavCmdToGoalBridge(Node):
    def __init__(self):
        super().__init__('nav_cmd_to_goal_bridge')
        
        self.is_available = True
        self._goal_handle = None
        self.is_dancing = False
        self.dance_step = 0
        
        # 1. Action Client & Publishers
        self.action_client = ActionClient(self, NavigateToPose, 'navigate_to_pose')
        self.status_publisher = self.create_publisher(Bool, 'robot_action_ok', 10)
        self.cmd_vel_publisher = self.create_publisher(Twist, 'cmd_vel', 10)
        
        # 2. Subscribers
        self.subscription = self.create_subscription(
            PoseStamped, 'nav_cmd', self.nav_cmd_callback, 10
        )
        # stop 토픽을 Bool 타입으로 구독
        self.stop_subscription = self.create_subscription(
            Bool, 'stop', self.stop_callback, 10
        )
        
        # 3. Timers
        self.status_timer = self.create_timer(0.1, self.publish_status)
        self.dance_timer = None # 춤출 때만 활성화할 타이머
        
        self.get_logger().info('춤추는 내비게이션 브릿지 노드가 준비되었습니다.')

    def publish_status(self):
        msg = Bool()
        msg.data = self.is_available and not self.is_dancing
        self.status_publisher.publish(msg)

    def stop_callback(self, msg):
        # 수신된 데이터가 False일 때만 춤을 춤
        if msg.data is False:
            self.get_logger().info('🛑 Stop(False) 수신! 작업을 취소하고 춤을 시작합니다. 💃')
            
            # 진행 중인 내비게이션 취소
            if self._goal_handle is not None:
                self._goal_handle.cancel_goal_async()
            
            # 춤 시작
            self.start_dance()
        else:
            self.get_logger().info('Stop(True) 수신: 단순히 정지합니다.')
            if self._goal_handle is not None:
                self._goal_handle.cancel_goal_async()

    def start_dance(self):
        if not self.is_dancing:
            self.is_dancing = True
            self.dance_step = 0
            # 0.2초 간격으로 춤 동작 수행 (총 3초간)
            if self.dance_timer is not None:
                self.dance_timer.cancel()
            self.dance_timer = self.create_timer(0.2, self.dance_routine)

    def dance_routine(self):
        twist = Twist()
        # 제자리에서 좌우로 흔들거리는 동작
        if self.dance_step < 15: # 약 3초 동안
            if (self.dance_step // 2) % 2 == 0:
                twist.angular.z = 1.5  # 왼쪽 회전
            else:
                twist.angular.z = -1.5 # 오른쪽 회전
            self.cmd_vel_publisher.publish(twist)
            self.dance_step += 1
        else:
            # 춤 종료
            twist.angular.z = 0.0
            self.cmd_vel_publisher.publish(twist)
            self.dance_timer.cancel()
            self.is_dancing = False
            self.get_logger().info('춤이 끝났습니다. 다시 명령을 대기합니다.')

    def nav_cmd_callback(self, msg):
        if self.is_dancing:
            self.get_logger().warn('춤추는 중에는 새로운 명령을 수행할 수 없습니다.')
            return

        self.get_logger().info(f'목표 위치 수신: x={msg.pose.position.x:.2f}')
        
        if not self.action_client.wait_for_server(timeout_sec=2.0):
            return

        goal_msg = NavigateToPose.Goal()
        goal_msg.pose = msg
        self._send_goal_future = self.action_client.send_goal_async(goal_msg)
        self._send_goal_future.add_done_callback(self.goal_response_callback)

    def goal_response_callback(self, future):
        goal_handle = future.result()
        if not goal_handle.accepted:
            return
        self.is_available = False
        self._goal_handle = goal_handle
        self._get_result_future = goal_handle.get_result_async()
        self._get_result_future.add_done_callback(self.get_result_callback)

    def get_result_callback(self, future):
        self._goal_handle = None
        self.is_available = True

def main(args=None):
    rclpy.init(args=args)
    node = NavCmdToGoalBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
