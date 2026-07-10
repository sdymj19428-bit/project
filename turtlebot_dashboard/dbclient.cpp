#include "dbclient.h"

DbClient::DbClient(QObject *parent) : QObject(parent)
{
    db_ = QSqlDatabase::addDatabase("QMYSQL");
    db_.setHostName("10.10.16.32");
    db_.setDatabaseName("turtlebot_db");
    db_.setUserName("turtlebot");
    db_.setPassword("1234");

    if (db_.open()) {
        qDebug() << "DB 연결 성공!";
    } else {
        qDebug() << "DB 연결 실패:" << db_.lastError().text();
    }
}

DbClient::~DbClient()
{
    db_.close();
}

bool DbClient::isConnected()
{
    return db_.isOpen();
}

void DbClient::insertVoiceCommand(
    int target_robot_id, QString parsed_action,
    QString parsed_target, bool success)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO voice_command_log "
        "(target_robot_id, parsed_action, parsed_target, success) "
        "VALUES (:robot_id, :parsed_action, :parsed_target, :success)"
    );
    query.bindValue(":robot_id",      target_robot_id);
    query.bindValue(":parsed_action", parsed_action);
    query.bindValue(":parsed_target", parsed_target);
    query.bindValue(":success",       success);
    if (query.exec())
        qDebug() << "voice_command_log INSERT 성공!";
    else
        qDebug() << "INSERT 실패:" << query.lastError().text();
}

void DbClient::insertPickingEvent(
    int assigned_robot_id, QString location_code,
    QString event_type, QString note)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO picking_event "
        "(assigned_robot_id, location_code, event_type, note) "
        "VALUES (:robot_id, :location_code, :event_type, :note)"
    );
    query.bindValue(":robot_id",      assigned_robot_id);
    query.bindValue(":location_code", location_code);
    query.bindValue(":event_type",    event_type);
    query.bindValue(":note",          note);
    if (query.exec())
        qDebug() << "picking_event INSERT 성공!";
    else
        qDebug() << "INSERT 실패:" << query.lastError().text();
}

void DbClient::insertRobotAction(
    int robot_id, int cmd_id,
    QString action_type,
    QString location_from, QString location_to,
    QString result)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO robot_action_log "
        "(robot_id, cmd_id, action_type, location_from, location_to, result) "
        "VALUES (:robot_id, :cmd_id, :action_type, :location_from, :location_to, :result)"
    );
    query.bindValue(":robot_id",      robot_id);
    query.bindValue(":cmd_id",        cmd_id);
    query.bindValue(":action_type",   action_type);
    query.bindValue(":location_from", location_from);
    query.bindValue(":location_to",   location_to);
    query.bindValue(":result",        result);
    if (query.exec())
        qDebug() << "robot_action_log INSERT 성공!";
    else
        qDebug() << "INSERT 실패:" << query.lastError().text();
}

void DbClient::insertAlert(
    int robot_id, QString severity,
    QString alert_type, QString message)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO alert_log "
        "(robot_id, severity, alert_type, message) "
        "VALUES (:robot_id, :severity, :alert_type, :message)"
    );
    query.bindValue(":robot_id",   robot_id);
    query.bindValue(":severity",   severity);
    query.bindValue(":alert_type", alert_type);
    query.bindValue(":message",    message);
    if (query.exec())
        qDebug() << "alert_log INSERT 성공!";
    else
        qDebug() << "INSERT 실패:" << query.lastError().text();
}

void DbClient::insertRobotStatus(
    int robot_id, double pos_x, double pos_y,
    double battery_pct, QString status)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO robot_status_log "
        "(robot_id, pos_x, pos_y, battery_pct, status) "
        "VALUES (:robot_id, :pos_x, :pos_y, :battery_pct, :status)"
    );
    query.bindValue(":robot_id",    robot_id);
    query.bindValue(":pos_x",       pos_x);
    query.bindValue(":pos_y",       pos_y);
    query.bindValue(":battery_pct", battery_pct);
    query.bindValue(":status",      status);

    if (query.exec())
        qDebug() << "robot_status_log INSERT 성공!";
    else
        qDebug() << "INSERT 실패:" << query.lastError().text();
}

QList<QMap<QString, QString>> DbClient::selectRobotStatus()
{
    QList<QMap<QString, QString>> result;
    QSqlQuery query;
    query.exec(
        "SELECT r.robot_name, s.status, s.pos_x, s.pos_y, s.battery_pct "
        "FROM robot r "
        "LEFT JOIN robot_status_log s ON r.robot_id = s.robot_id "
        "WHERE s.log_id IN ("
        "  SELECT MAX(log_id) FROM robot_status_log GROUP BY robot_id)"
    );
    while (query.next()) {
        QMap<QString, QString> row;
        row["robot_name"]  = query.value("robot_name").toString();
        row["status"]      = query.value("status").toString();
        row["pos_x"]       = query.value("pos_x").toString();
        row["pos_y"]       = query.value("pos_y").toString();
        row["battery_pct"] = query.value("battery_pct").toString();
        result.append(row);
    }
    return result;
}

QList<QMap<QString, QString>> DbClient::selectAlerts()
{
    QList<QMap<QString, QString>> result;
    QSqlQuery query;
    query.exec(
        "SELECT a.alert_id, r.robot_name, "
        "a.severity, a.alert_type, a.message, a.triggered_at "
        "FROM alert_log a "
        "LEFT JOIN robot r ON a.robot_id = r.robot_id "
        "WHERE a.resolved = false "
        "ORDER BY a.triggered_at DESC"
    );
    while (query.next()) {
        QMap<QString, QString> row;
        row["alert_id"]    = query.value("alert_id").toString();
        row["robot_name"]  = query.value("robot_name").toString();
        row["severity"]    = query.value("severity").toString();
        row["alert_type"]  = query.value("alert_type").toString();
        row["message"]     = query.value("message").toString();
        row["triggered_at"] = query.value("triggered_at").toString();
        result.append(row);
    }
    return result;
}

QList<QMap<QString, QString>> DbClient::selectVoiceCommands()
{
    QList<QMap<QString, QString>> result;
    QSqlQuery query;
    query.exec(
        "SELECT v.cmd_id, r.robot_name, "
        "v.parsed_action, v.parsed_target, v.success, v.issued_at "
        "FROM voice_command_log v "
        "LEFT JOIN robot r ON v.target_robot_id = r.robot_id "
        "ORDER BY v.issued_at DESC LIMIT 20"
    );
    while (query.next()) {
        QMap<QString, QString> row;
        row["robot_name"]    = query.value("robot_name").toString();
        row["parsed_action"] = query.value("parsed_action").toString();
        row["parsed_target"] = query.value("parsed_target").toString();
        row["success"]       = query.value("success").toString();
        row["issued_at"]     = query.value("issued_at").toString();
        result.append(row);
    }
    return result;
}

QList<QMap<QString, QString>> DbClient::selectRobotActions()
{
    QList<QMap<QString, QString>> result;
    QSqlQuery query;
    query.exec(
        "SELECT r.robot_name, a.action_type, "
        "a.location_from, a.location_to, a.result "
        "FROM robot_action_log a "
        "LEFT JOIN robot r ON a.robot_id = r.robot_id "
        "ORDER BY a.action_id DESC LIMIT 20"
    );
    while (query.next()) {
        QMap<QString, QString> row;
        row["robot_name"]    = query.value("robot_name").toString();
        row["action_type"]   = query.value("action_type").toString();
        row["location_from"] = query.value("location_from").toString();
        row["location_to"]   = query.value("location_to").toString();
        row["result"]        = query.value("result").toString();
        result.append(row);
    }
    return result;
}
