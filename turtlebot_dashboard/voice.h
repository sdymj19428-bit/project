/*
 * python으로 작성된 음성인식 코드를 별도로 실행하여 print된 값을 읽고,
*  그 값을 파싱하여 시그널을 발생시키는 코드
 */

#ifndef VOICE_H
#define VOICE_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QDebug>

// 파싱된 데이터를 한 번에 묶어서 보내기 위한 구조체
struct VoiceResult {
    QString intent;
    double confidence;
    QString text;
    QString response;
};

class Voice : public QObject
{
    Q_OBJECT
public:
    explicit Voice(QObject *parent = nullptr);
    ~Voice();

    void startSystem(const QString &scriptPath); // 파이썬 실행
    void stopSystem();                           // 파이썬 종료

signals:
    // 상태 변경 시그널 (대기중, 인식중, 잡음, 타임아웃 등)
    void statusChanged(QString status);

    // 최종 분석 결과 시그널
    void voiceResultReceived(VoiceResult result);

private slots:
    // 파이썬의 print() 출력을 실시간으로 읽는 슬롯
    void handleReadyRead();
    void handleProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process;
    void parseVoiceData(const QString &line); // "intent: ..." 문장 파싱 함수
};

#endif // VOICE_H
