#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QString>
#include <QList>
#include <QMap>

class DbClient : public QObject
{
    Q_OBJECT
public:
    explicit DbClient(QObject *parent = nullptr);
    ~DbClient();
    bool isConnected();

    void insertVoiceCommand(int target_robot_id, QString parsed_action,
                            QString parsed_target, bool success);
    void insertPickingEvent(int assigned_robot_id, QString location_code,
                            QString event_type, QString note);
    void insertRobotAction(int robot_id, int cmd_id,
                           QString action_type,
                           QString location_from, QString location_to,
                           QString result);
    void insertAlert(int robot_id, QString severity,
                     QString alert_type, QString message);
    void insertRobotStatus(int robot_id, double pos_x, double pos_y,
                           double battery_pct, QString status);

    QList<QMap<QString, QString>> selectRobotStatus();
    QList<QMap<QString, QString>> selectAlerts();
    QList<QMap<QString, QString>> selectVoiceCommands();
    QList<QMap<QString, QString>> selectRobotActions();

private:
    QSqlDatabase db_;
};
