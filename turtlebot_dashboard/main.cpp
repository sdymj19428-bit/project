#include "mainwindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <rclcpp/rclcpp.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication::addLibraryPath("/usr/lib/x86_64-linux-gnu/qt5/plugins");

    rclcpp::init(argc, argv);
    auto node = rclcpp::Node::make_shared("turtlebot_dashboard");

    QApplication a(argc, argv);

    MainWindow w(node);
//    w.setFixedSize(800, 480);
    w.show();
//    w.showFullScreen();
    return a.exec();
}
