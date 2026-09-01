#pragma once

#include <QObject>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHash>
#include <QHostAddress>

namespace Configs {
    class ProxyEntity;
}

class HydraStdioBridge : public QObject {
    Q_OBJECT

public:
    static HydraStdioBridge *instance();
    ~HydraStdioBridge() override;

    bool start(const std::shared_ptr<Configs::ProxyEntity> &ent);
    void stop();
    [[nodiscard]] bool isRunning() const;

private:
    explicit HydraStdioBridge(QObject *parent = nullptr);

    bool listenRelay(quint16 port);
    void onNewControlConnection();
    void onControlReadyRead();
    void onRelayReadyRead();
    void onProcessStdout();
    void onProcessStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    QProcess *process = nullptr;
    QTcpServer *controlServer = nullptr;
    QUdpSocket *relay = nullptr;
    quint16 relayPort = 0;
    QHash<QTcpSocket *, QByteArray> controlBuffers;
    QHash<QString, QPair<QHostAddress, quint16>> udpClients;
    QByteArray stdoutBuffer;
};
