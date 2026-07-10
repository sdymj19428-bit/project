cmake_minimum_required(VERSION 3.5)

project(turtlebot_dashboard VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql)
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    dbclient.cpp
    dbclient.h
    voice.cpp
    voice.h
)

add_executable(turtlebot_dashboard ${PROJECT_SOURCES}
  map.qrc
  voice.cpp
  voice.h
  )

target_link_libraries(turtlebot_dashboard
    Qt5::Widgets
    Qt5::Sql
)

ament_target_dependencies(turtlebot_dashboard
  rclcpp
  sensor_msgs
  geometry_msgs
)

install(TARGETS turtlebot_dashboard
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
