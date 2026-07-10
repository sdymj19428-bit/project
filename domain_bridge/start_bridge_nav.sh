#!/bin/bash

# 설정을 위한 변수
MAIN_DOMAIN=24
START_ROBOT_DOMAIN=25
NUM_ROBOTS=4

echo "🤖 4대 로봇 시스템 시동 (Main Domain: $MAIN_DOMAIN)"

# 1. 각 로봇별 실행 루프
for i in $(seq 1 $NUM_ROBOTS)
do
    DOMAIN_ID=$((START_ROBOT_DOMAIN + i - 1))
    echo "------------------------------------------"
    echo "🚀 Robot $i 설정 중... (Domain ID: $DOMAIN_ID)"
    
    # [A] 공통: Navigation2 실행 터미널
    gnome-terminal -- bash -c "echo 'Robot $i Navigation'; export ROS_DOMAIN_ID=$DOMAIN_ID; export TURTLEBOT3_MODEL=burger; ros2 launch turtlebot3_navigation2 navigation2.launch.py use_sim_time:=False; exec bash"

    # [B] 조건부: 로봇 2, 3, 4번만 파이썬 브릿지 노드 실행
    # i=1 (Robot 1, Domain 25)은 제외하고 i가 2 이상일 때만 실행
    if [ $i -ge 2 ]; then
        echo "🔗 Robot $i 전용 Nav-Goal 브릿지 노드 실행..."
        # Nav2가 올라올 시간을 조금 주기 위해 2초 대기 후 실행
        gnome-terminal -- bash -c "sleep 2; echo 'Robot $i Bridge Node'; export ROS_DOMAIN_ID=$DOMAIN_ID; python3 nav_cmd_to_goal_pose.py; exec bash"
    fi
done

# 2. 메인 도메인 브릿지 실행 (Domain 24)
# 이 브릿지는 모든 도메인의 토픽을 24번으로 모으는 역할을 유지합니다.
echo "------------------------------------------"
echo "🔗 Global Domain Bridge 실행 중... (To Domain $MAIN_DOMAIN)"
export ROS_DOMAIN_ID=$MAIN_DOMAIN
ros2 run domain_bridge domain_bridge bridge_config.yaml
