#include "voice.h"
#include <QFile>

Voice::Voice(QObject *parent) : QObject(parent)
{
    m_process = new QProcess(this);

    // [중요] 에러 메시지를 가로채기 위해 StandardError 시그널도 연결합니다.
    connect(m_process, &QProcess::readyReadStandardOutput, this, &Voice::handleReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, [=](){
        // 파이썬 문법 에러나 라이브러리 부재 시 이 로그가 찍힙니다.
        qDebug() << "[Python Critical Error]:" << m_process->readAllStandardError();
    });

    connect(m_process, &QProcess::errorOccurred, this, &Voice::handleProcessError);
}

Voice::~Voice()
{
    stopSystem();
}

void Voice::startSystem(const QString &scriptPath)
{
    qDebug() << "--- Voice System Initialization ---";

    // 1. 파일이 실제로 존재하는지 C++에서 먼저 체크
    if (!QFile::exists(scriptPath)) {
        qDebug() << "FATAL: Python script not found at" << scriptPath;
        emit statusChanged("파일 없음");
        return;
    }

    if (m_process->state() == QProcess::Running) return;

    // 2. 실행 시 "-u" 옵션을 주어 파이썬이 텍스트를 즉시 뱉어내게 강제합니다.
    m_process->start("python3", QStringList() << "-u" << scriptPath);

    if (!m_process->waitForStarted()) {
        qDebug() << "Failed to start Python process:" << m_process->errorString();
    } else {
        qDebug() << "Python process started successfully!";
    }
}

void Voice::stopSystem()
{
    if (m_process->state() == QProcess::Running) {
        m_process->terminate();
        m_process->waitForFinished(3000);
    }
}

void Voice::handleReadyRead()
{
    // readAll()로 전체를 읽어온 뒤 줄바꿈 단위로 처리
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data).trimmed();

    // 로그 창에 실시간으로 찍히는지 확인
    qDebug() << "[Raw Python Output]:" << output;

    QStringList lines = output.split("\n");
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) continue;

        if (line == "대기중" || line == "듣는중" || line == "인식중" ||
            line == "타임아웃" || line == "인식 실패" || line == "잡음") {
            emit statusChanged(line);
        }
        else if (line.startsWith("intent:")) {
            parseVoiceData(line);
        }
    }
}

void Voice::parseVoiceData(const QString &line)
{
    VoiceResult result;
    auto parts = line.split(", ");
    for (const QString &part : parts) {
        if (part.startsWith("intent:")) result.intent = part.mid(7).trimmed();
        else if (part.startsWith("confidence:")) result.confidence = part.mid(11).trimmed().toDouble();
        else if (part.startsWith("text:")) result.text = part.mid(5).trimmed();
        else if (part.startsWith("response:")) result.response = part.mid(9).trimmed();
    }
    emit voiceResultReceived(result);
}

void Voice::handleProcessError(QProcess::ProcessError error)
{
    qDebug() << "QProcess Error Occurred:" << error;
    emit statusChanged("시스템 에러");
}
