#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <array>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <std_msgs/msg/bool.hpp>
#include <QDialog>
#include <QSqlQuery>
#include <QProcess>
#include <cmath>
#include "dbclient.h"
#include  "voice.h"

//added for voice
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(rclcpp::Node::SharedPtr node, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateRobotPositions();
    void checkBattery();
    void spinOnce();
    void resizeEvent(QResizeEvent *event);
    void handleVoiceStatus(QString status);
    void handleVoiceResult(VoiceResult result);

private:
    void setupUI();
    void setupMap();
    void setupRobotCard(QVBoxLayout *layout, int idx);
    int getRobotIdFromText(QString text);
    void sendStopToRobot(int robotNum);
    void sendDanceToRobot(int robotNum);

    //마이크 피드백
    QWidget *overlayWidget_;
    QLabel *micIconLabel_;       // 마이크 아이콘 (애니메이션 대상)
    QLabel *voiceStatusText_;    // 상태 메시지
    QPropertyAnimation *pulseAnim_; // 커졌다 작아졌다 하는 애니메이션

    //robot control func
    void sendRobotToLocation(int robotNum, const double loc[4]);
    int getNearestRobotId(const double loc[6]);

//    작업자
//    QGraphicsEllipseItem *workerItem_ = nullptr;
//    double workerX_ = 250;
//    double workerY_ = 250;
//    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr worker_sub_;

    // 로봇 이름
    const std::array<QString, 4> robotNames_ = {"알렉사", "철수", "길동", "영희"};

    // ROS2
    rclcpp::Node::SharedPtr node_;
    std::array<rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr, 4> odom_subs_;
    std::array<rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr, 4> battery_subs_;
    std::array<rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr, 4> status_subs_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr nav_cmd_robot2_pub;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr nav_cmd_robot3_pub;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr nav_cmd_robot4_pub;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stop_robot2_pub;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stop_robot3_pub;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stop_robot4_pub;


    // 지도
    QGraphicsView  *mapView_;
    QGraphicsScene *mapScene_;
    QGraphicsEllipseItem *robotItems_[4];
    QGraphicsTextItem    *robotTexts_[4] = {nullptr, nullptr, nullptr, nullptr};
    QGraphicsTextItem *workerText_ = nullptr;

    // 로봇 카드 라벨
    QLabel *labelBattery_[4];
    QLabel *labelPos_[4];
    QLabel *labelStatus_[4];
    QLabel *labelVoiceStatus_;

    // DB
    DbClient *pDb_;

    //voice asistant
    Voice *pVoice;

    // 타이머
    QTimer *timer_;
    QTimer *spinTimer_;

    //Voice
    Voice *voice_;

    // 로봇 데이터
    //로봇의 시작 위치 값으로 초기화
    double robotX_[4]   = {-0.02389465644955635*59 + 250, 2.1129746437072754 *59 + 250, 2.660083293914795 *59 + 250, 1.408046841621399*59 + 250};
    double robotY_[4]   = {-0.02604793570935726*-74.76+307.76, -1.4665216207504272*-74.76+307.76, -1.3506938219070435*-74.76+307.76, -1.4875789880752563*-74.76+307.76};
    double battery_[4]  = {100, 100, 100, 100};
    bool   robotOk_[3]  = {true, true, true};
    double raw_robotX[4] = {0,0,0,0};
    double raw_robotY[4] = {0,0,0,0};
    double master_z = 0;
    double master_w = 0;

    double area_A[4] = {3.0909345149993896,3.8802568912506104,-0.7,0.7};
    double area_B[4] = {1.6932272911071777,3.885867118835449,-0.7,0.7};
    double area_C[4] = {0.3228745460510254,3.8954710960388184,-0.7,0.7};
    double home_1[4] = {-0.02389465644955635,-0.02604793570935726,1,0};
    double home_2[4] = {2.1129746437072754, -1.4665216207504272, 0.7, 0.7};
    double home_3[4] = {2.660083293914795,-1.3506938219070435,0.7,0.7};
    double home_4[4] = {1.408046841621399,-1.4875789880752563,0.7,0.7};

    int selectedRobotId_ = 0;

};
#endif
