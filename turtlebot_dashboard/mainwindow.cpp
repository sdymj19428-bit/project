#include "mainwindow.h"
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <std_msgs/msg/bool.hpp>
#include <QSqlQuery>

MainWindow::MainWindow(rclcpp::Node::SharedPtr node, QWidget *parent)
    : QMainWindow(parent), node_(node)
{
    setupUI();
    pDb_ = new DbClient(this);

    spinTimer_ = new QTimer(this);
    connect(spinTimer_, &QTimer::timeout, this, &MainWindow::spinOnce);
    spinTimer_->start(30);

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MainWindow::updateRobotPositions);
    connect(timer_, &QTimer::timeout, this, &MainWindow::checkBattery);
    timer_->start(300);

    const std::array<std::string, 4> pose_topics = {
        "/robot1/amcl_pose", "/robot2/amcl_pose",
        "/robot3/amcl_pose", "/robot4/amcl_pose"
    };

    for (int i = 0; i < 4; i++) {
        odom_subs_[i] = node_->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
            pose_topics[i], 10,
            [this, i](const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
                QMetaObject::invokeMethod(this, [this, i, msg]() {
                    raw_robotX[i] = msg->pose.pose.position.x;
                    raw_robotY[i] = msg->pose.pose.position.y;
                    master_z = msg->pose.pose.orientation.z;
                    master_w = msg->pose.pose.orientation.w;

                    robotX_[i] =  raw_robotX[i] * 59 + 250;
                    robotY_[i] = raw_robotY[i] *-74.76+307.76;

                    // robot_status_log INSERT
                    pDb_->insertRobotStatus(
                        i + 1,
                        msg->pose.pose.position.x,
                        msg->pose.pose.position.y,
                        battery_[i],
                        robotOk_[i] ? "working" : "idle"
                    );
                }, Qt::QueuedConnection);
            }
        );
    }

    const std::array<std::string, 4> battery_topics = {
        "/robot1/battery_state", "/robot2/battery_state",
        "/robot3/battery_state", "/robot4/battery_state"
    };
    for (int i = 0; i < 4; i++) {
        battery_subs_[i] = node_->create_subscription<sensor_msgs::msg::BatteryState>(
            battery_topics[i], 10,
            [this, i](const sensor_msgs::msg::BatteryState::SharedPtr msg) {
                QMetaObject::invokeMethod(this, [this, i, msg]() {
                    double pct = msg->percentage;
                    if (pct >= 0.0 && pct <= 1.0) pct *= 100.0;
                    battery_[i] = pct;
                }, Qt::QueuedConnection);
            }
        );
    }

    const std::array<std::string, 4> status_topics = {
        "/robot1/status", "/robot2/status",
        "/robot3/status", "/robot4/status"
    };
    for (int i = 0; i < 4; i++) {
        status_subs_[i] = node_->create_subscription<std_msgs::msg::Bool>(
            status_topics[i], 10,
            [this, i](const std_msgs::msg::Bool::SharedPtr msg) {
                QMetaObject::invokeMethod(this, [this, i, msg]() {
                    robotOk_[i] = msg->data;
                }, Qt::QueuedConnection);
            }
        );
    }

    //voice generate and start
    pVoice = new Voice(this);
    pVoice->startSystem("/home/prolee/voice_project/alexa_system.py");

    connect(pVoice, SIGNAL(statusChanged(QString)), this, SLOT(handleVoiceStatus(QString)));

    connect(pVoice, SIGNAL(voiceResultReceived(VoiceResult)), this, SLOT(handleVoiceResult(VoiceResult)));

    //robot control
    nav_cmd_robot2_pub = node_->create_publisher<geometry_msgs::msg::PoseStamped>(
        "/robot2/nav_cmd", 10  // 토픽 이름과 QoS 설정
        );
    nav_cmd_robot3_pub = node_->create_publisher<geometry_msgs::msg::PoseStamped>(
        "/robot3/nav_cmd", 10  // 토픽 이름과 QoS 설정
        );
    nav_cmd_robot4_pub = node_->create_publisher<geometry_msgs::msg::PoseStamped>(
        "/robot4/nav_cmd", 10  // 토픽 이름과 QoS 설정
        );

    stop_robot2_pub = node_->create_publisher<std_msgs::msg::Bool>(
        "/robot2/stop", 10  // 토픽 이름과 QoS 설정
        );
    stop_robot3_pub = node_->create_publisher<std_msgs::msg::Bool>(
        "/robot3/stop", 10  // 토픽 이름과 QoS 설정
        );
    stop_robot4_pub = node_->create_publisher<std_msgs::msg::Bool>(
        "/robot4/stop", 10  // 토픽 이름과 QoS 설정
        );

//    worker_sub_ = node_->create_subscription<geometry_msgs::msg::PoseStamped>(
//        "/worker/pose", 10,
//        [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
//            QMetaObject::invokeMethod(this, [this, msg]() {
//                if (msg->pose.position.x == -1.0) return;

//                double x = msg->pose.position.x;
//                double y = msg->pose.position.y;
//                double oz = msg->pose.orientation.z;
//                double ow = msg->pose.orientation.w;
//                double yaw = 2.0 * atan2(oz, ow);  // 방향각 (라디안)

//                // yaw 방향으로 약간 보정
//                double offset = 0.01;  // 보정 거리 (조절 가능)
//                double correctedX = x + offset * cos(yaw);
//                double correctedY = y + offset * sin(yaw);

//                workerX_ = robotX_[0] + correctedX * 59;
//                workerY_ = robotY_[0] + correctedY * -74.76;

//                qDebug() << "작업자 위치:" << workerX_ << workerY_ << "방향:" << yaw;
//            }, Qt::QueuedConnection);
//        }
//    );
}

MainWindow::~MainWindow()
{
    delete pDb_;
}

void MainWindow::spinOnce()
{
    rclcpp::spin_some(node_);
}

void MainWindow::updateRobotPositions()
{
    for (int i = 0; i < 4; i++) {
        if (!robotItems_[i] || !robotTexts_[i]) continue;
        robotItems_[i]->setPos(robotX_[i], robotY_[i]);
        robotTexts_[i]->setPos(robotX_[i] + 18, robotY_[i]);
        labelPos_[i]->setText(
            QString("위치  X:%1  Y:%2")
            .arg(robotX_[i], 0, 'f', 1)
            .arg(robotY_[i], 0, 'f', 1)
        );
        if (robotOk_[i]) {
            labelStatus_[i]->setText("● 작업가능");
            labelStatus_[i]->setStyleSheet("color: #a6e3a1; font-size: 10px; font-weight: bold;");
        }
        else {
            labelStatus_[i]->setText("● 작업불가");
            labelStatus_[i]->setStyleSheet("color: #f38ba8; font-size: 10px; font-weight: bold;");
        }
    }
    // 작업자 위치 업데이트
//    if (workerItem_) {
//        workerItem_->setPos(workerX_, workerY_);
//    }
//    if (workerText_) {
//        workerText_->setPos(workerX_ + 18, workerY_);
//    }
}

void MainWindow::checkBattery()
{
    for (int i = 0; i < 4; i++) {
        labelBattery_[i]->setText(
            QString("배터리  %1%").arg(battery_[i], 0, 'f', 1)
        );
        if (battery_[i] < 20.0) {
            labelBattery_[i]->setStyleSheet("color: #f38ba8; font-size: 10px; font-weight: bold;");
            QMessageBox::warning(this, "배터리 경고",
                QString("%1 배터리 부족! (%2%)")
                .arg(robotNames_[i]).arg(battery_[i], 0, 'f', 1));
            pDb_->insertAlert(i+1, "warning", "low_battery",
                QString("배터리 %1% 이하").arg(battery_[i]));
        } else {
            labelBattery_[i]->setStyleSheet("color: #a6e3a1; font-size: 10px;");
        }
    }
//    작업자 위치 업데이트
//    if (workerItem_) {
//        workerItem_->setPos(workerX_, workerY_);
//    }
}

void MainWindow::setupUI()
{
    setWindowTitle("TurtleBot Dashboard");

    auto *central = new QWidget(this);
    auto *hLayout = new QHBoxLayout(central);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);

    setupMap();
    hLayout->addWidget(mapView_, 3);

    auto *sideWidget = new QWidget(central);
    sideWidget->setFixedWidth(220);
    sideWidget->setStyleSheet("background-color: #1e1e2e;");
    auto *sideLayout = new QVBoxLayout(sideWidget);
    sideLayout->setContentsMargins(8, 8, 8, 8);
    sideLayout->setSpacing(6);

    // 타이틀 + DB버튼 + X버튼
    auto *topLayout = new QHBoxLayout();
    auto *title = new QLabel("🤖 TurtleBot Dashboard", sideWidget);
    title->setStyleSheet("color: #cdd6f4; font-size: 11px; font-weight: bold;");
    topLayout->addWidget(title);

    auto *btnDb = new QPushButton("📊", sideWidget);
    btnDb->setFixedSize(24, 24);
    btnDb->setStyleSheet(
        "QPushButton { background-color: #313244; color: #89b4fa; "
        "border: 1px solid #45475a; border-radius: 4px; font-size: 12px; }"
        "QPushButton:pressed { background-color: #585b70; }"
    );
    connect(btnDb, &QPushButton::clicked, this, [this]() {
        auto *dialog = new QDialog(this);
        dialog->setWindowTitle("📊 DB");
        dialog->showFullScreen();
        dialog->setStyleSheet("background-color: #1e1e2e; color: #cdd6f4;");

        auto *mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(16, 16, 16, 16);
        mainLayout->setSpacing(10);

        // 상단: 타이틀 + 초기화 + 닫기
        auto *headerLayout = new QHBoxLayout();
        auto *titleLabel = new QLabel("📊 DB 현황", dialog);
        titleLabel->setStyleSheet("color: #89b4fa; font-size: 16px; font-weight: bold;");
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();

        auto *resetBtn = new QPushButton("🗑️ 초기화", dialog);
        resetBtn->setFixedSize(80, 30);
        resetBtn->setStyleSheet(
            "QPushButton { background-color: #313244; color: #f38ba8; "
            "border: 1px solid #f38ba8; border-radius: 4px; font-size: 12px; }"
            "QPushButton:pressed { background-color: #585b70; }"
        );
        connect(resetBtn, &QPushButton::clicked, this, [this, dialog]() {
            auto reply = QMessageBox::warning(
                dialog, "DB 초기화",
                "모든 기록이 삭제됩니다. 계속할까요?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::Yes) {
                QSqlQuery q;
                q.exec("SET FOREIGN_KEY_CHECKS = 0");
                q.exec("TRUNCATE TABLE alert_log");
                q.exec("TRUNCATE TABLE robot_action_log");
                q.exec("TRUNCATE TABLE robot_status_log");
                q.exec("TRUNCATE TABLE voice_command_log");
                q.exec("TRUNCATE TABLE picking_event");
                q.exec("TRUNCATE TABLE sensor_data");
                q.exec("SET FOREIGN_KEY_CHECKS = 1");
                QMessageBox::information(dialog, "완료", "DB 초기화 완료!");
                dialog->close();
            }
        });
        headerLayout->addWidget(resetBtn);

        auto *closeBtn = new QPushButton("✕ 닫기", dialog);
        closeBtn->setFixedSize(80, 30);
        closeBtn->setStyleSheet(
            "QPushButton { background-color: #313244; color: #f38ba8; "
            "border: 1px solid #45475a; border-radius: 4px; font-size: 12px; }"
            "QPushButton:pressed { background-color: #585b70; }"
        );
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);
        headerLayout->addWidget(closeBtn);
        mainLayout->addLayout(headerLayout);

        // 중단: 경고이력 + 동작기록 + 음성명령
        auto *midLayout = new QHBoxLayout();
        midLayout->setSpacing(10);

        // 경고 이력
        auto *alertFrame = new QFrame(dialog);
        alertFrame->setStyleSheet("QFrame { background-color: #313244; border-radius: 8px; }");
        auto *alertLayout = new QVBoxLayout(alertFrame);
        auto *alertTitle = new QLabel("⚠️ 경고 이력", alertFrame);
        alertTitle->setStyleSheet("color: #f9e2af; font-size: 12px; font-weight: bold; border: none;");
        alertLayout->addWidget(alertTitle);

        auto alertList = pDb_->selectAlerts();
        if (alertList.isEmpty()) {
            auto *noAlert = new QLabel("경고 없음", alertFrame);
            noAlert->setStyleSheet("color: #a6e3a1; font-size: 11px; border: none;");
            alertLayout->addWidget(noAlert);
        } else {
            for (auto &row : alertList) {
                QString resolved = row["resolved"] == "1" ? "[해결됨]" : "[미해결]";
                auto *item = new QLabel(
                    QString("%1  %2\n%3  %4")
                    .arg(resolved)
                    .arg(row["robot_name"])
                    .arg(row["alert_type"])
                    .arg(row["triggered_at"].left(16)), alertFrame);
                item->setStyleSheet("color: #f38ba8; font-size: 10px; border: none; padding: 4px;");
                alertLayout->addWidget(item);
            }
        }
        alertLayout->addStretch();
        midLayout->addWidget(alertFrame);

        // 동작 기록
        auto *actionFrame = new QFrame(dialog);
        actionFrame->setStyleSheet("QFrame { background-color: #313244; border-radius: 8px; }");
        auto *actionLayout = new QVBoxLayout(actionFrame);
        auto *actionTitle = new QLabel("🤖 동작 기록", actionFrame);
        actionTitle->setStyleSheet("color: #89b4fa; font-size: 12px; font-weight: bold; border: none;");
        actionLayout->addWidget(actionTitle);

        auto statusList = pDb_->selectRobotActions();
        if (statusList.isEmpty()) {
            auto *noAction = new QLabel("동작 기록 없음", actionFrame);
            noAction->setStyleSheet("color: #cdd6f4; font-size: 11px; border: none;");
            actionLayout->addWidget(noAction);
        } else {
            for (auto &row : statusList) {
                auto *item = new QLabel(
                    QString("%1  %2  → %3  %4")
                    .arg(row["robot_name"])
                    .arg(row["action_type"])
                    .arg(row["location_to"])
                    .arg(row["result"]), actionFrame);
                item->setStyleSheet("color: #cdd6f4; font-size: 10px; border: none; padding: 4px;");
                actionLayout->addWidget(item);
            }
        }
        actionLayout->addStretch();
        midLayout->addWidget(actionFrame);

        // 음성 명령 이력
        auto *voiceFrame = new QFrame(dialog);
        voiceFrame->setStyleSheet("QFrame { background-color: #313244; border-radius: 8px; }");
        auto *voiceLayout = new QVBoxLayout(voiceFrame);
        auto *voiceTitle = new QLabel("🎙️ 음성 명령 이력", voiceFrame);
        voiceTitle->setStyleSheet("color: #cba6f7; font-size: 12px; font-weight: bold; border: none;");
        voiceLayout->addWidget(voiceTitle);

        auto voiceList = pDb_->selectVoiceCommands();
        if (voiceList.isEmpty()) {
            auto *noVoice = new QLabel("명령 없음", voiceFrame);
            noVoice->setStyleSheet("color: #cdd6f4; font-size: 11px; border: none;");
            voiceLayout->addWidget(noVoice);
        } else {
            for (auto &row : voiceList) {
                QString success = row["success"] == "1" ? "✅" : "❌";
                auto *item = new QLabel(
                    QString("%1  %2  %3  %4")
                    .arg(row["parsed_action"])
                    .arg(row["parsed_target"])
                    .arg(success)
                    .arg(row["issued_at"].left(16)), voiceFrame);
                item->setStyleSheet("color: #cba6f7; font-size: 10px; border: none; padding: 4px;");
                voiceLayout->addWidget(item);
            }
        }
        voiceLayout->addStretch();
        midLayout->addWidget(voiceFrame);
        mainLayout->addLayout(midLayout);

        // 하단: 통계 요약
        auto *statsFrame = new QFrame(dialog);
        statsFrame->setStyleSheet("QFrame { background-color: #313244; border-radius: 8px; }");
        auto *statsLayout = new QVBoxLayout(statsFrame);

        auto *statsTitle = new QLabel("📈 통계 요약", statsFrame);
        statsTitle->setStyleSheet("color: #a6e3a1; font-size: 12px; font-weight: bold; border: none;");
        statsLayout->addWidget(statsTitle);

        // 평균 배터리
        auto *batteryTitle = new QLabel("🔋 평균 배터리", statsFrame);
        batteryTitle->setStyleSheet("color: #89b4fa; font-size: 11px; border: none;");
        statsLayout->addWidget(batteryTitle);

        QColor colors[] = {QColor("#89b4fa"), QColor("#a6e3a1"),
                           QColor("#f9e2af"), QColor("#f38ba8")};
        auto *batteryRow = new QHBoxLayout();
        for (int i = 0; i < 4; i++) {
            auto *bl = new QLabel(
                QString("%1\n%2%")
                .arg(robotNames_[i])
                .arg(battery_[i], 0, 'f', 1), statsFrame);
            bl->setStyleSheet(
                QString("color: %1; font-size: 11px; border: none; font-weight: bold;")
                .arg(colors[i].name()));
            bl->setAlignment(Qt::AlignCenter);
            batteryRow->addWidget(bl);
        }
        statsLayout->addLayout(batteryRow);

        // 가장 활동적인 로봇
        auto *activeTitle = new QLabel("🏆 가장 활동적인 로봇", statsFrame);
        activeTitle->setStyleSheet("color: #89b4fa; font-size: 11px; border: none;");
        statsLayout->addWidget(activeTitle);

        QSqlQuery countQuery;
        countQuery.exec("SELECT COUNT(*) FROM robot_action_log");
        countQuery.next();
        int totalCount = countQuery.value(0).toInt();

        if (totalCount == 0) {
            auto *noData = new QLabel("없음", statsFrame);
            noData->setStyleSheet("color: #6c7086; font-size: 11px; border: none;");
            statsLayout->addWidget(noData);
        } else {
            QSqlQuery activeQuery;
            activeQuery.exec(
                "SELECT r.robot_name, COUNT(*) as cnt "
                "FROM robot_action_log a "
                "JOIN robot r ON a.robot_id = r.robot_id "
                "GROUP BY a.robot_id ORDER BY cnt DESC LIMIT 1"
            );
            if (activeQuery.next()) {
                auto *activeLabel = new QLabel(
                    QString("%1 (동작 %2회)")
                    .arg(activeQuery.value("robot_name").toString())
                    .arg(activeQuery.value("cnt").toInt()), statsFrame);
                activeLabel->setStyleSheet("color: #f9e2af; font-size: 11px; border: none; font-weight: bold;");
                statsLayout->addWidget(activeLabel);
            }
        }

        mainLayout->addWidget(statsFrame);
        dialog->exec();
    });
    topLayout->addWidget(btnDb);

    auto *btnClose = new QPushButton("✕", sideWidget);
    btnClose->setFixedSize(24, 24);
    btnClose->setStyleSheet(
        "QPushButton { background-color: #313244; color: #f38ba8; "
        "border: 1px solid #45475a; border-radius: 4px; font-size: 12px; }"
        "QPushButton:pressed { background-color: #585b70; }"
    );
    connect(btnClose, &QPushButton::clicked, this, &MainWindow::close);
    topLayout->addWidget(btnClose);
    sideLayout->addLayout(topLayout);

    auto *line = new QFrame(sideWidget);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: #313244;");
    sideLayout->addWidget(line);

    labelVoiceStatus_ = new QLabel("🎙️ 대기중", sideWidget);
    labelVoiceStatus_->setStyleSheet("color: #cba6f7; font-size: 11px; border: 1px solid #45475a; border-radius: 4px; padding: 4px;");
    sideLayout->addWidget(labelVoiceStatus_);

    QColor robotColors[] = {
        QColor("#89b4fa"),
        QColor("#a6e3a1"),
        QColor("#f9e2af"),
        QColor("#f38ba8")
    };

    for (int i = 0; i < 4; i++) {
        auto *card = new QFrame(sideWidget);
        card->setStyleSheet(
            QString("QFrame { background-color: #313244; border: 1px solid %1; border-radius: 6px; }")
            .arg(robotColors[i].name())
        );
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 6, 8, 6);
        cardLayout->setSpacing(3);

        auto *nameLabel = new QLabel(robotNames_[i], card);
        nameLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold; border: none;")
            .arg(robotColors[i].name())
        );
        cardLayout->addWidget(nameLabel);

        labelBattery_[i] = new QLabel("배터리  100%", card);
        labelBattery_[i]->setStyleSheet("color: #a6e3a1; font-size: 10px; border: none;");
        cardLayout->addWidget(labelBattery_[i]);

        labelPos_[i] = new QLabel("위치  X:0  Y:0", card);
        labelPos_[i]->setStyleSheet("color: #cdd6f4; font-size: 10px; border: none;");
        cardLayout->addWidget(labelPos_[i]);

        labelStatus_[i] = new QLabel("● 작업가능", card);
        labelStatus_[i]->setStyleSheet("color: #a6e3a1; font-size: 10px; font-weight: bold; border: none;");
        cardLayout->addWidget(labelStatus_[i]);

        sideLayout->addWidget(card);
    }

    sideLayout->addStretch();
    hLayout->addWidget(sideWidget);
    setCentralWidget(central);

    //--------------------------------------------------------------------

    // 오버레이 위젯 생성
    overlayWidget_ = new QWidget(this);
    overlayWidget_->setStyleSheet("background-color: rgba(0, 0, 0, 190);"); // 약간 더 어둡게
    overlayWidget_->hide();

    auto *overlayLayout = new QVBoxLayout(overlayWidget_);
    overlayLayout->setAlignment(Qt::AlignCenter);

    // 1. 마이크 아이콘 레이블
    micIconLabel_ = new QLabel("🎙️", overlayWidget_);
    micIconLabel_->setAlignment(Qt::AlignCenter);
    micIconLabel_->setStyleSheet("font-size: 80px; background: transparent; color: #f38ba8;");

    // 2. 상태 텍스트 레이블
    voiceStatusText_ = new QLabel("명령을 기다리고 있습니다...", overlayWidget_);
    voiceStatusText_->setAlignment(Qt::AlignCenter);
    voiceStatusText_->setStyleSheet("color: #89b4fa; font-size: 18px; font-weight: bold; margin-top: 20px;");

    overlayLayout->addWidget(micIconLabel_);
    overlayLayout->addWidget(voiceStatusText_);

    // 3. 애니메이션 설정 (아이콘의 투명도를 조절해 "숨쉬는" 효과 부여)
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(micIconLabel_);
    micIconLabel_->setGraphicsEffect(eff);

    pulseAnim_ = new QPropertyAnimation(eff, "opacity", this);
    pulseAnim_->setDuration(800);        // 0.8초 동안
    pulseAnim_->setStartValue(1.0);      // 완전히 보임
    pulseAnim_->setEndValue(0.3);        // 흐릿해짐
    pulseAnim_->setEasingCurve(QEasingCurve::InOutQuad);
    pulseAnim_->setLoopCount(-1);        // 무한 반복

    //------------------------------------------------------
}

void MainWindow::setupMap()
{
    mapScene_ = new QGraphicsScene(this);
    mapScene_->setBackgroundBrush(QColor("#181825"));

    QPixmap mapPixmap(":/map/map/map.pgm");
    mapPixmap = mapPixmap.scaled(580, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    auto *mapItem = mapScene_->addPixmap(mapPixmap);
    mapItem->setPos((580 - mapPixmap.width()) / 2, (480 - mapPixmap.height()) / 2);
    mapScene_->setSceneRect(0, 0, 580, 480);

    mapView_ = new QGraphicsView(mapScene_, this);
    mapView_->setRenderHint(QPainter::Antialiasing);
    mapView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mapView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QColor robotColors[] = {
        QColor("#89b4fa"),
        QColor("#a6e3a1"),
        QColor("#f9e2af"),
        QColor("#f38ba8")
    };

    for (int i = 0; i < 4; i++) {
        robotItems_[i] = mapScene_->addEllipse(0, 0, 16, 16,
            QPen(robotColors[i], 2),
            QBrush(robotColors[i].darker(150)));
        robotItems_[i]->setPos(robotX_[i], robotY_[i]);

        robotTexts_[i] = mapScene_->addText(robotNames_[i]);
        robotTexts_[i]->setDefaultTextColor(robotColors[i]);
        robotTexts_[i]->setPos(robotX_[i] + 18, robotY_[i]);
    }

    // 작업자 (노란색)
//    auto *workerItem = mapScene_->addEllipse(0, 0, 18, 18,
//        QPen(QColor("#FFB347"), 2),
//        QBrush(QColor("#FFB347")));  // 밝은 노란색
//    workerItem->setPos(workerX_, workerY_);
//    workerItem_ = workerItem;

//    workerText_ = mapScene_->addText("작업자");
//    workerText_->setDefaultTextColor(QColor("#FFB347"));
//    workerText_->setPos(workerX_ + 20, workerY_);
}

void MainWindow::setupRobotCard(QVBoxLayout *layout, int idx)
{
    // setupUI에서 처리
}

//robot control func

int MainWindow::getRobotIdFromText(QString text) {
    if (text.contains("철수")) return 2;
    if (text.contains("길동")) return 3;
    if (text.contains("영희")) return 4;
    return -1; // 이름 없음
}

// 특정 목적지(loc)에서 가장 가까운 '작업 가능'한 로봇 찾기
int MainWindow::getNearestRobotId(const double loc[6]) {
    int nearestId = -1;
    double minDest = 1e9;

    for (int i = 1; i < 4; i++) { // robot2, 3, 4 (인덱스 1, 2, 3)
        if (!robotOk_[i]) continue; // 작업 불가능한 로봇 제외

        // 유클리드 거리 계산 (ROS 좌표 기준)
        // 현재 로봇 위치는 robotX_[i] (UI 좌표일 경우 변환 필요, 여기선 개념적 계산)
        double dx = raw_robotX[i] - loc[0];
        double dy = raw_robotY[i] - loc[1];
        double dist = std::sqrt(dx*dx + dy*dy);

        if (dist < minDest) {
            minDest = dist;
            nearestId = i + 1; // ID는 2, 3, 4
        }
    }
    return nearestId;
}

void MainWindow::sendStopToRobot(int robotNum)
{
    // 1. Bool 메시지 생성 및 데이터 설정
    auto stopMsg = std_msgs::msg::Bool();
    stopMsg.data = true; // 정지 신호 활성화

    // 2. 로봇 번호에 맞는 퍼블리셔로 발행
    switch (robotNum) {
    case 2:
        if (stop_robot2_pub) stop_robot2_pub->publish(stopMsg);
        break;
    case 3:
        if (stop_robot3_pub) stop_robot3_pub->publish(stopMsg);
        break;
    case 4:
        if (stop_robot4_pub) stop_robot4_pub->publish(stopMsg);
        break;
    default:
        if (stop_robot2_pub) stop_robot2_pub->publish(stopMsg);
        if (stop_robot3_pub) stop_robot3_pub->publish(stopMsg);
        if (stop_robot4_pub) stop_robot4_pub->publish(stopMsg);
        qDebug() << "모든 로봇에게 정지 신호를 보냈습니다.";
        return;
    }

    qDebug() << "Robot" << robotNum << "에게 정지 명령을 보냈습니다.";
}

void MainWindow::sendDanceToRobot(int robotNum)
{
    // 1. Bool 메시지 생성 및 데이터 설정 (false = 춤 시작)
    auto danceMsg = std_msgs::msg::Bool();
    danceMsg.data = false;

    // 2. 로봇 번호에 따른 분기 처리
    switch (robotNum) {
    case 2:
        if (stop_robot2_pub) stop_robot2_pub->publish(danceMsg);
        break;
    case 3:
        if (stop_robot3_pub) stop_robot3_pub->publish(danceMsg);
        break;
    case 4:
        if (stop_robot4_pub) stop_robot4_pub->publish(danceMsg);
        break;
    default:
        // 이름을 안 불렀으면 다 같이 파티 타임!
        if (stop_robot2_pub) stop_robot2_pub->publish(danceMsg);
        if (stop_robot3_pub) stop_robot3_pub->publish(danceMsg);
        if (stop_robot4_pub) stop_robot4_pub->publish(danceMsg);
        qDebug() << "모든 로봇이 춤을 춥니다! 💃🕺";
        return;
    }

    qDebug() << "Robot" << robotNum << "이(가) 춤을 추기 시작합니다!";
}

void MainWindow::handleVoiceResult(VoiceResult result)
{
    QString intent = result.intent;
    QString text = result.text;
    int targetId = getRobotIdFromText(text);

    qDebug() << "Voice Command -> Intent:" << intent << " Robot:" << targetId;

    // 1. 특정 로봇 지정 이동 (Select Intents)
    if (intent == "go_A_select" && targetId != -1) {
        sendRobotToLocation(targetId, area_A);
    }
    else if (intent == "go_B_select" && targetId != -1) {
        sendRobotToLocation(targetId, area_B);
    }
    else if (intent == "go_C_select" && targetId != -1) {
        sendRobotToLocation(targetId, area_C);
    }

    if (intent == "come_here") {
        double master_pose[4] = {raw_robotX[0], raw_robotY[0], master_z, master_w};
        sendRobotToLocation(targetId, master_pose);
    }

    // 2. 가장 가까운 로봇 자동 배정 (Auto Intents)
    else if (intent == "go_A_auto") {
        int autoId = getNearestRobotId(area_A);
        if (autoId != -1) sendRobotToLocation(autoId, area_A);
    }
    else if (intent == "go_B_auto") {
        int autoId = getNearestRobotId(area_B);
        if (autoId != -1) sendRobotToLocation(autoId, area_B);
    }
    else if (intent == "go_C_auto") {
        int autoId = getNearestRobotId(area_C);
        if (autoId != -1) sendRobotToLocation(autoId, area_C);
    }

    // 3. 복귀 명령 (Go Home)
    else if (intent == "go_home") {
        if (targetId == 2) sendRobotToLocation(2, home_2);
        else if (targetId == 3) sendRobotToLocation(3, home_3);
        else if (targetId == 4) sendRobotToLocation(4, home_4);
        else {
            // 이름을 안 부르고 "홈으로 가"라고 하면 모두 복귀
            sendRobotToLocation(2, home_2);
            sendRobotToLocation(3, home_3);
            sendRobotToLocation(4, home_4);
        }
    }

    // 4. 정지 명령 (Stop)
    else if (intent == "stop") {
        sendStopToRobot(targetId);
        qDebug() << "모든 로봇 정지 명령 수행";
    }

    // 5. 기타 (Dance)
    else if (intent == "dance") {
        // 로봇들에게 제자리 회전 등 시퀀스 명령 전달
        sendDanceToRobot(targetId);
    }

    // DB INSERT (내 코드 추가)
    // 로봇 이름으로 ID 찾기
    int robotId = 0;
    if (targetId != -1) {
        robotId = targetId;
    } else {
        // auto 계열 - 팀원 getNearestRobotId 사용
        if (intent == "go_A_auto") robotId = getNearestRobotId(area_A);
        else if (intent == "go_B_auto") robotId = getNearestRobotId(area_B);
        else if (intent == "go_C_auto") robotId = getNearestRobotId(area_C);
        if (robotId == -1) robotId = 0;
    }

    if (intent == "come_here" && robotId != 0) {
        selectedRobotId_ = robotId;
    }

    // voice_command_log INSERT
    pDb_->insertVoiceCommand(robotId, intent, text, true);

    // intent → action 매핑
    QString actionType = "";
    QString locationTo = "";

    if (intent == "stop") actionType = "stop";
    else if (intent == "come_here") actionType = "move_to";
    else if (intent == "go_home") { actionType = "return_base"; locationTo = "home"; }
    else if (intent == "go_A_select" || intent == "go_A_auto") { actionType = "move_to"; locationTo = "A"; }
    else if (intent == "go_B_select" || intent == "go_B_auto") { actionType = "move_to"; locationTo = "B"; }
    else if (intent == "go_C_select" || intent == "go_C_auto") { actionType = "move_to"; locationTo = "C"; }

    // robot_action_log INSERT
    if (!actionType.isEmpty()) {
        pDb_->insertRobotAction(robotId, -1, actionType, "", locationTo, "success");
    }

    // picking_event INSERT
    if (intent == "come_here" || intent.contains("fetch")) {
        pDb_->insertPickingEvent(robotId, locationTo, "pick_start", text);
    }

    // UI 업데이트
    labelVoiceStatus_->setText(QString("🎙️ %1 (%2)").arg(intent).arg(text));
}

void MainWindow::sendRobotToLocation(int robotNum, const double loc[4])
{
    // 1. 메시지 객체 생성 및 기본 설정
    auto msg = geometry_msgs::msg::PoseStamped();
    msg.header.stamp = node_->now();
    msg.header.frame_id = "map";

    // 2. 배열 데이터를 메시지에 매핑
    msg.pose.position.x = loc[0];
    msg.pose.position.y = loc[1];
    msg.pose.orientation.z = loc[2];
    msg.pose.orientation.w = loc[3];

    // 3. 로봇 번호에 따른 분기 처리 (1번 발행)
    switch (robotNum) {
    case 2:
        if (nav_cmd_robot2_pub) nav_cmd_robot2_pub->publish(msg);
        break;
    case 3:
        if (nav_cmd_robot3_pub) nav_cmd_robot3_pub->publish(msg);
        break;
    case 4:
        if (nav_cmd_robot4_pub) nav_cmd_robot4_pub->publish(msg);
        break;
    default:
        qDebug() << "잘못된 로봇 번호입니다:" << robotNum;
        return;
    }

    qDebug() << "Robot" << robotNames_[robotNum - 1] << "이(가) 목적지로 이동합니다. (X:" << loc[0] << ", Y:" << loc[1] << ")";
}

//voice func
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (overlayWidget_) {
        overlayWidget_->resize(this->size()); // 창 전체 크기로 맞춤
    }
}

void MainWindow::handleVoiceStatus(QString status)
{
    qDebug() << "Current Voice Status:" << status;

    // "듣는중" 상태일 때만 오버레이를 보여줌
    if (status == "듣는중") {
        overlayWidget_->show();
        overlayWidget_->raise(); // 최상단으로 올림

        // 2. 애니메이션이 할당되었는지 확인 후 시작
        if (pulseAnim_) {
            if (pulseAnim_->state() != QAbstractAnimation::Running) {
                pulseAnim_->start();
                qDebug() << "Animation started!";
            }
        }
    }
    // "대기중" 혹은 결과 수신 완료 시 다시 숨김
    else if(status == "대기중"){
        overlayWidget_->hide();
    }

    // UI 라벨 업데이트 (내 코드 추가)
    labelVoiceStatus_->setText("🎙️ " + status);
}
