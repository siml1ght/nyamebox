

#include <nekobox/global/HydraStdioBridge.hpp>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/sys/Settings.h>
#include <nekobox/ui/mainwindow.h>

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QUrlQuery>

#include <nekobox/configs/proxy/HydraBean.hpp>

#ifdef Q_OS_WIN
#define NKR_HYDRA_CLIENT_NAME "hydra-client.exe"
#else
#define NKR_HYDRA_CLIENT_NAME "hydra-client"
#endif

#define HYDRA_FRAME_FROM_GAME 0x01
#define HYDRA_FRAME_TO_GAME 0x02
#define HYDRA_FRAME_CLOSE 0x03

#define HYDRA_HIBYTE16(x) ((char) (((x) >> 8) & 0xFF))
#define HYDRA_LOBYTE16(x) ((char) ((x) & 0xFF))

static const int kHydraDnsBasePort = 53;

static quint32 hyIpv4ToUint(const QByteArray &a) {
    if (a.size() != 4) return 0;
    return ((quint32) (unsigned char) a[0] << 24) | ((quint32) (unsigned char) a[1] << 16) |
           ((quint32) (unsigned char) a[2] << 8) | (quint32) (unsigned char) a[3];
}

static QByteArray hySock5MethodResponse() {
    QByteArray r;
    r.append((char) 0x05);
    r.append((char) 0x00);
    return r;
}

static QByteArray hySocks5AssociateResponse(quint16 relayPort) {
    QByteArray r;
    r.append((char) 0x05);
    r.append((char) 0x00);
    r.append((char) 0x00);
    r.append((char) 0x01);
    r.append((char) 127);
    r.append((char) 0);
    r.append((char) 0);
    r.append((char) 1);
    r.append(HYDRA_HIBYTE16(relayPort));
    r.append(HYDRA_LOBYTE16(relayPort));
    return r;
}

static bool hyParseSocksUdp(const QByteArray &d, QByteArray &addr4, quint16 &port, QByteArray &payload) {
    if (d.size() < 10) return false;
    if (((unsigned char) d[0]) != 0 || ((unsigned char) d[1]) != 0) return false;
    if (((unsigned char) d[2]) != 0) return false;
    unsigned char atyp = (unsigned char) d[3];
    if (atyp == 0x01) {
        if (d.size() < 10) return false;
        addr4 = d.mid(4, 4);
        port = (quint16) (((unsigned char) d[8] << 8) | (unsigned char) d[9]);
        payload = d.mid(10);
        return true;
    }
    if (atyp == 0x03) {
        int len = (unsigned char) d[4];
        int need = 4 + len + 2 + 1;
        if (d.size() < need) return false;
        port = (quint16) (((unsigned char) d[4 + len] << 8) | (unsigned char) d[5 + len]);
        payload = d.mid(6 + len);
        addr4 = QByteArray(4, '\0');
        return true;
    }
    return false;
}

static QString hyKeyFor(const QByteArray &addr4, quint16 port) {
    return QString("%1:%2").arg(QHostAddress(hyIpv4ToUint(addr4)).toString()).arg(port);
}

HydraStdioBridge *HydraStdioBridge::instance() {
    static HydraStdioBridge *p = new HydraStdioBridge;
    return p;
}

HydraStdioBridge::HydraStdioBridge(QObject *parent) : QObject(parent) {}

HydraStdioBridge::~HydraStdioBridge() { stop(); }

bool HydraStdioBridge::isRunning() const {
    return process != nullptr && process->state() != QProcess::NotRunning;
}

bool HydraStdioBridge::start(const std::shared_ptr<Configs::ProxyEntity> &ent) {
    if (ent == nullptr || ent->type != "hydra") return false;

    auto bean = ent->HydraBean();
    if (bean == nullptr) return false;

    stop();

    auto corePath = getResource(NKR_HYDRA_CLIENT_NAME);
    QFileInfo coreFile(corePath);
    if (!coreFile.exists() || !coreFile.isFile()) {
        MW_show_log("[Hydra] " + QObject::tr("hydra-client not found: %1").arg(corePath));
        return false;
    }

    auto corePathNative = QDir::toNativeSeparators(coreFile.absoluteFilePath());

    if (!listenRelay((quint16) bean->local_relay_port)) {
        MW_show_log("[Hydra] " + QObject::tr("cannot listen relay port %1").arg(bean->local_relay_port));
        return false;
    }

    QString hopSecret = bean->hop_secret;
    if (hopSecret.isEmpty()) hopSecret = bean->secret_key;

    int portOffset = ent->serverPort - kHydraDnsBasePort;
    if (portOffset < 0) portOffset = 0;

    QStringList args{
        "--server", ent->serverAddress,
        "--port", QString::number(ent->serverPort),
        "--secret", bean->secret_key,
        "--session", QString::number((long long) bean->session_id),
        "--hop-secret", hopSecret,
        "--hop-base", QString::number(bean->hop_base),
        "--hop-range", QString::number(bean->hop_range),
        "--port-offset", QString::number(portOffset),
    };

    process = new QProcess(this);
    process->setProgram(corePathNative);
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, &HydraStdioBridge::onProcessStdout);
    connect(process, &QProcess::readyReadStandardError, this, &HydraStdioBridge::onProcessStderr);
    connect(process, &QProcess::finished, this, &HydraStdioBridge::onProcessFinished);

    process->start();
    if (!process->waitForStarted(3000)) {
        MW_show_log("[Hydra] " + QObject::tr("failed to start hydra-client: %1").arg(process->errorString()));
        stop();
        return false;
    }
    MW_show_log("[Hydra] " + QObject::tr("hydra-client started (relay 127.0.0.1:%1)").arg(bean->local_relay_port));
    return true;
}

void HydraStdioBridge::stop() {
    if (process != nullptr) {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            if (!process->waitForFinished(2000)) process->terminate();
        }
        process->deleteLater();
        process = nullptr;
    }
    if (controlServer != nullptr) {
        controlServer->close();
        controlServer->deleteLater();
        controlServer = nullptr;
    }
    if (relay != nullptr) {
        relay->close();
        relay->deleteLater();
        relay = nullptr;
    }
    controlBuffers.clear();
    udpClients.clear();
    stdoutBuffer.clear();
    relayPort = 0;
}

bool HydraStdioBridge::listenRelay(quint16 port) {
    relayPort = port;
    controlServer = new QTcpServer(this);
    if (!controlServer->listen(QHostAddress::LocalHost, port)) {
        delete controlServer;
        controlServer = nullptr;
        return false;
    }
    connect(controlServer, &QTcpServer::newConnection, this, &HydraStdioBridge::onNewControlConnection);
    relay = new QUdpSocket(this);
    if (!relay->bind(QHostAddress::LocalHost, port)) {
        delete relay;
        relay = nullptr;
        controlServer->close();
        delete controlServer;
        controlServer = nullptr;
        return false;
    }
    connect(relay, &QUdpSocket::readyRead, this, &HydraStdioBridge::onRelayReadyRead);
    return true;
}

void HydraStdioBridge::onNewControlConnection() {
    while (controlServer->hasPendingConnections()) {
        auto sock = controlServer->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead, this, &HydraStdioBridge::onControlReadyRead);
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
        controlBuffers[sock] = {};
    }
}

void HydraStdioBridge::onControlReadyRead() {
    auto sock = dynamic_cast<QTcpSocket *>(sender());
    if (sock == nullptr) return;
    controlBuffers[sock] += sock->readAll();
    auto buf = controlBuffers[sock];

    if (buf.size() >= 2) {
        if ((unsigned char) buf[0] != 0x05) {
            sock->disconnectFromHost();
            return;
        }
        int nMethods = (unsigned char) buf[1];
        if (buf.size() < 2 + nMethods) {
            controlBuffers[sock] = buf;
            return;
        }
        sock->write(hySock5MethodResponse());
        buf.remove(0, 2 + nMethods);
    }

    if (buf.size() >= 10 && (unsigned char) buf[0] == 0x05 && (unsigned char) buf[1] == 0x03) {
        if ((unsigned char) buf[3] != 0x01) {
            sock->disconnectFromHost();
            return;
        }
        sock->write(hySocks5AssociateResponse(relayPort));
        buf.remove(0, 10);
    }
    controlBuffers[sock] = buf;
}

void HydraStdioBridge::onRelayReadyRead() {
    if (relay == nullptr) return;
    while (relay->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize((int) relay->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        relay->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        QByteArray addr4;
        quint16 port = 0;
        QByteArray payload;
        if (!hyParseSocksUdp(datagram, addr4, port, payload)) continue;

        udpClients[hyKeyFor(addr4, port)] = {sender, senderPort};

        if (process == nullptr || !isRunning()) continue;

        QByteArray frame;
        frame.append(HYDRA_HIBYTE16(payload.size()));
        frame.append(HYDRA_LOBYTE16(payload.size()));
        frame.append((char) HYDRA_FRAME_FROM_GAME);
        frame += addr4;
        frame.append(HYDRA_HIBYTE16(port));
        frame.append(HYDRA_LOBYTE16(port));
        frame += payload;
        process->write(frame);
    }
}

void HydraStdioBridge::onProcessStdout() {
    if (process == nullptr) return;
    stdoutBuffer += process->readAllStandardOutput();

    int offset = 0;
    while (stdoutBuffer.size() - offset >= 9) {
        int len = ((unsigned char) stdoutBuffer[offset] << 8) | (unsigned char) stdoutBuffer[offset + 1];
        if (stdoutBuffer.size() - offset < 9 + len) break;

        QByteArray addr4 = stdoutBuffer.mid(offset + 3, 4);
        quint16 port = (quint16) (((unsigned char) stdoutBuffer[offset + 7] << 8) |
                                  (unsigned char) stdoutBuffer[offset + 8]);
        QByteArray payload = stdoutBuffer.mid(offset + 9, len);
        offset += 9 + len;

        QString key = hyKeyFor(addr4, port);
        if (!udpClients.contains(key)) continue;
        auto target = udpClients[key];

        QByteArray resp;
        resp.append((char) 0);
        resp.append((char) 0);
        resp.append((char) 0);
        resp.append((char) 0x01);
        resp += addr4;
        resp.append(HYDRA_HIBYTE16(port));
        resp.append(HYDRA_LOBYTE16(port));
        resp += payload;
        relay->writeDatagram(resp, target.first, target.second);
    }
    if (offset > 0) stdoutBuffer.remove(0, offset);
}

void HydraStdioBridge::onProcessStderr() {
    if (process == nullptr) return;
    auto data = process->readAllStandardError();
    for (const auto &line : data.split('\n')) {
        auto t = QString::fromLocal8Bit(line.trimmed());
        if (t.isEmpty()) continue;
        MW_show_log("[Hydra] " + t);
    }
}

void HydraStdioBridge::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode)
    if (exitStatus == QProcess::CrashExit) {
        MW_show_log("[Hydra] " + QObject::tr("hydra-client exited unexpectedly, profile traffic stopped"));
    }
}
