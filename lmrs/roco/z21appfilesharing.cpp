#include "z21appfilesharing.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QPointer>
#include <QSaveFile>
#include <QSysInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QVersionNumber>

namespace lmrs::roco::z21app {

namespace {

constexpr auto s_servicePort = 5728_u16;

constexpr auto s_requestType =                      "request"_L1;
constexpr auto s_requestType_deviceInfo =           "device_information_request"_L1;
constexpr auto s_requestType_fileTransferInfo =     "file_transfer_info"_L1;

constexpr auto s_deviceInfo_apiVersion =            "apiVersion"_L1;
constexpr auto s_deviceInfo_appVersion =            "appVersion"_L1;
constexpr auto s_deviceInfo_buildNumber =           "buildNumber"_L1;
constexpr auto s_deviceInfo_deviceName =            "deviceName"_L1;
constexpr auto s_deviceInfo_deviceType =            "deviceType"_L1;
constexpr auto s_deviceInfo_platform =              "os"_L1;

constexpr auto s_fileTransfer_fileName =            "fileName"_L1;
constexpr auto s_fileTransfer_fileSize =            "fileSize"_L1;
constexpr auto s_fileTransfer_owningDevice =        "owningDevice"_L1;

auto isSameAddress(QHostAddress lhs, QHostAddress rhs)
{
    if (lhs.protocol() == rhs.protocol())
        return lhs == rhs;

    if (lhs.protocol() == QHostAddress::IPv4Protocol
            && rhs.protocol() == QHostAddress::IPv6Protocol)
        return QHostAddress{lhs.toIPv6Address()} == rhs;

    if (lhs.protocol() == QHostAddress::IPv6Protocol
            && rhs.protocol() == QHostAddress::IPv4Protocol)
        return lhs == QHostAddress{rhs.toIPv6Address()};

    return false;
}

auto isOwnAddress(QHostAddress address)
{
    for (const auto &iface: QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;

        for (const auto &entry: iface.addressEntries())
            if (isSameAddress(entry.ip(), address))
                return true;
    }

    return false;
}

auto allBroadcastAddresses()
{
    auto broadcastAddresses = QSet<QHostAddress>{};

    for (const auto &iface: QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::CanBroadcast))
            continue;

        for (const auto &entry: iface.addressEntries())
            if (auto address = entry.broadcast(); !address.isNull())
                broadcastAddresses.insert(std::move(address));
    }

    return broadcastAddresses;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QString DeviceInfo::deviceName() const
{
    return m_deviceInfo[s_deviceInfo_deviceName].toString();
}

QString DeviceInfo::deviceType() const
{
    return m_deviceInfo[s_deviceInfo_deviceType].toString();
}

QString DeviceInfo::devicePlatform() const
{
    return m_deviceInfo[s_deviceInfo_platform].toString();
}

QString DeviceInfo::appVersion() const
{
    return m_deviceInfo[s_deviceInfo_appVersion].toString();
}

int DeviceInfo::protocolVersion() const
{
    return m_deviceInfo[s_deviceInfo_apiVersion].toInt();
}

int DeviceInfo::buildNumber() const
{
    return m_deviceInfo[s_deviceInfo_buildNumber].toInt();
}

bool DeviceInfo::isValid() const
{
    return !m_deviceInfo.isEmpty()
            && !m_address.isNull()
            && m_port != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QString FileInfo::fileName() const
{
    return m_fileInfo[s_fileTransfer_fileName].toString();
}

qint64 FileInfo::fileSize() const
{
    return m_fileInfo[s_fileTransfer_fileSize].toInteger();
}

DeviceInfo FileInfo::owningDevice() const
{
    return DeviceInfo{m_fileInfo[s_fileTransfer_owningDevice].toObject(), m_address, 0};
}

bool FileInfo::isValid() const
{
    return !m_fileInfo.isEmpty()
            && !m_address.isNull()
            && !fileName().isEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FileSharing::Private : public core::PrivateObject<FileSharing>
{
public:
    using PrivateObject::PrivateObject;

    QPointer<QTcpServer> m_tcpServer;
    QPointer<QUdpSocket> m_udpSocket;

//    QHash<QHostAddress, QPointer<FileTransferSession>> m_sessions;

    // TCP server
    void onTcpConnected();

    // UDP server
    void onUdpReadyRead();
    void processDeviceInfo(QNetworkDatagram message);
    bool sendDeviceInfo(QHostAddress address, quint16 port);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSharing::Private::onTcpConnected()
{
    while (const auto socket = m_tcpServer->nextPendingConnection()) {
        // FIXME: This makes pending sessions for same address unreachable:
        // Shall we cancel old sessions, shall we track them all. Do we even care?
        // m_sessions.emplace(socket->peerAddress(), );
        const auto session = new FileTransferSession{socket, q()};

        connect(session, &FileTransferSession::stateChanged, this,
                [this, session](FileTransferSession::State state) {
            qInfo() << __func__ << state;

            if (state == FileTransferSession::State::Started)
                emit q()->fileTransferOffered(session, FileSharing::QPrivateSignal{});
        });

    }
}

void FileSharing::Private::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        const auto message = m_udpSocket->receiveDatagram();

        if (isOwnAddress(message.senderAddress()))
            continue;

        if (message.data().contains(s_requestType_deviceInfo)) {
            processDeviceInfo(std::move(message));
        } else if (const auto timestamp = core::toInt64(message.data())) {
            sendDeviceInfo(message.senderAddress(), static_cast<quint16>(message.senderPort()));
        }
    }
}

void FileSharing::Private::processDeviceInfo(QNetworkDatagram message)
{
    auto jsonStatus = QJsonParseError{};
    auto json = QJsonDocument::fromJson(message.data(), &jsonStatus).object();

    if (jsonStatus.error != QJsonParseError::NoError) {
        qCWarning(logger(),
                  "Invalid device information request from %ls: %ls",
                  qUtf16Printable(message.senderAddress().toString()),
                  qUtf16Printable(jsonStatus.errorString()));

        return;
    }

    const auto senderPort = static_cast<quint16>(message.senderPort());
    auto info = DeviceInfo{std::move(json), message.senderAddress(), senderPort};

    if (!info.isValid()) {
        qCWarning(logger(),
                  "Invalid device information request from %ls",
                  qUtf16Printable(message.senderAddress().toString()));

        return;
    }

    qCInfo(logger(), "Nearby Z21 App discovered: %ls", qUtf16Printable(info.deviceName()));
    emit q()->deviceDiscovered(std::move(info),{});
}

bool FileSharing::Private::sendDeviceInfo(QHostAddress address, quint16 port)
{
    auto deviceType = QSysInfo::prettyProductName() + " ("_L1 + QSysInfo::currentCpuArchitecture() + ")"_L1;

    auto deviceInfo = QJsonObject {
        {s_deviceInfo_apiVersion, 1},           // FIXME: make this a constant?
        {s_deviceInfo_appVersion, "1.4.7"_L1},  // FIXME: make this a constant, or report qApp->applicationVersion()
        {s_deviceInfo_buildNumber, 6142},       // FIXME: make this a constant, or report qApp->applicationVersion()
        {s_deviceInfo_deviceName, QSysInfo::machineHostName()},
        {s_deviceInfo_deviceType, deviceType},
        {s_deviceInfo_platform, QSysInfo::productType()},
        {s_requestType, s_requestType_deviceInfo},
    };

    auto data = QJsonDocument{std::move(deviceInfo)}.toJson();
    const auto bytesToWrite = data.size();

    if (m_udpSocket->writeDatagram(std::move(data), std::move(address), port) != bytesToWrite) {
        qCWarning(logger(), "Could not send device information to %ls: %ls",
                  qUtf16Printable(address.toString()), qUtf16Printable(m_udpSocket->errorString()));

        return false;
    }

    qCInfo(logger(), "Device information sent to to %ls", qUtf16Printable(address.toString()));
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileSharing::FileSharing(QObject *parent)
    : QObject{parent}
    , d{new Private{this}}
{}

bool FileSharing::isListening() const
{
    return (d->m_tcpServer && d->m_tcpServer->isListening())
            && d->m_udpSocket;
}

bool FileSharing::startListening()
{
    if (isListening())
        return false;

    auto propertyGuard = core::propertyGuard(this, &FileSharing::isListening, &FileSharing::isListeningChanged);
    auto socketGuard = qScopeGuard([this] { stopListening(); });

    d->m_tcpServer = new QTcpServer{d};
    d->m_udpSocket = new QUdpSocket{d};

    connect(d->m_tcpServer, &QTcpServer::newConnection, d, &Private::onTcpConnected);
    connect(d->m_udpSocket, &QUdpSocket::readyRead, d, &Private::onUdpReadyRead);

    if (!d->m_tcpServer->listen(QHostAddress::Any, s_servicePort)) {
        qCWarning(d->logger(),
                  "Could not start file receiver for Roco Z21 App: %ls",
                  qUtf16Printable(d->m_udpSocket->errorString()));

        return false;
    }

    if (!d->m_udpSocket->bind(s_servicePort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCWarning(d->logger(),
                  "Could not start device discovery for Roco Z21 App: %ls",
                  qUtf16Printable(d->m_udpSocket->errorString()));

        return false;
    }

    qCInfo(d->logger(), "Ready for file sharing with Roco Z21 App");
    socketGuard.dismiss();

    return true;
}

void FileSharing::stopListening()
{
    auto propertyGuard = core::propertyGuard(this, &FileSharing::isListening, &FileSharing::isListeningChanged);

    // do not clear old sessions: we do not want accepted file transfers to be aborted

    if (const auto oldServer = std::exchange(d->m_tcpServer, nullptr)) {
        oldServer->close();
        oldServer->deleteLater();
    }

    if (const auto oldSocket = std::exchange(d->m_udpSocket, nullptr)) {
        oldSocket->close();
        oldSocket->deleteLater();
    }

    qCInfo(d->logger(), "File sharing with Roco Z21 App stopped");
}

bool FileSharing::discoverDevices()
{
    if (!isListening())
        return false;

    auto token = QByteArray::number(QDateTime::currentMSecsSinceEpoch());
    const auto bytesToWrite = token.size();
    auto succeeded = false;

    for (const auto addressList = allBroadcastAddresses(); const auto &address: addressList) {
        if (d->m_udpSocket->writeDatagram(std::move(token), address, s_servicePort) != bytesToWrite) {
            qCWarning(d->logger(), "Could not send device info request via %ls: %ls",
                      qUtf16Printable(address.toString()), qUtf16Printable(d->m_udpSocket->errorString()));
        } else {
            succeeded = true; // at least one datagram sent
        }
    }

    return succeeded;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FileTransferSession::Private : public core::PrivateObject<FileTransferSession>
{
public:
    explicit Private(QAbstractSocket *socket, FileTransferSession *parent);

    core::ConstPointer<QSaveFile> localFile{q()};
    const QPointer<QAbstractSocket> socket;
    FileInfo fileInfo;

    qint64 totalBytesReceived = 0;
    State state = State::Connected;
    Error error = NoError;

    auto makeStateGuard();
    void reportError(Error error, QString errorMessage, auto *propertyGuard);

    void startTransfer();
    void receiveFile();
    void cancel();

    void onReadyRead();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileTransferSession::Private::Private(QAbstractSocket *socket, FileTransferSession *parent)
    : PrivateObject{parent}
    , socket{socket}
{
    connect(socket, &QTcpSocket::readyRead, this, &Private::onReadyRead);
}

auto FileTransferSession::Private::makeStateGuard()
{
    return core::propertyGuard(q(), &FileTransferSession::state, [this](State value) {
        emit q()->stateChanged(value, FileTransferSession::QPrivateSignal{});
    });
}

void FileTransferSession::Private::reportError(Error error, QString errorMessage, auto *stateGuard)
{
    if (stateGuard) {
        error = Error::FileError;
        state = State::Finished;
    } else {
        const auto stateGuard = makeStateGuard();
        reportError(error, std::move(errorMessage), &stateGuard);
    }
}

void FileTransferSession::Private::onReadyRead()
{
    auto sessionGuard = qScopeGuard([this] { cancel(); });

    switch (state) {
    case State::Started:
    case State::Rejected:
    case State::Finished:
        break;

    case State::Connected:
        sessionGuard.dismiss();
        startTransfer();
        return;

    case State::Accepted:
        sessionGuard.dismiss();
        receiveFile();
        return;
    }

    qCWarning(logger(), "Unexpected data from %ls in %s state: %s",
              qUtf16Printable(socket->peerAddress().toString()), core::key(state),
              socket->readAll().toPercentEncoding("{}:,\" ").constData());
}

void FileTransferSession::Private::startTransfer()
{
    const auto stateGuard = makeStateGuard();
    auto sessionGuard = qScopeGuard([this] { cancel(); });

    if (socket.isNull()) // FIXME: softassert
        return;

    auto jsonStatus = QJsonParseError{};
    auto json = QJsonDocument::fromJson(socket->readAll(), &jsonStatus).object();

    if (jsonStatus.error != QJsonParseError::NoError) {
        qCWarning(logger(),
                  "Invalid request during session start from %ls: %ls",
                  qUtf16Printable(socket->peerAddress().toString()),
                  qUtf16Printable(jsonStatus.errorString()));

        return;
    }

    if (const auto requestType = json[s_requestType].toString();
            requestType == s_requestType_fileTransferInfo) {
        fileInfo = {std::move(json), socket->peerAddress()};

        if (!fileInfo.isValid()) {
            qCWarning(logger(),
                      "Invalid file transfer request from %ls",
                      qUtf16Printable(socket->peerAddress().toString()));

            return;
        }

        sessionGuard.dismiss();
        state = State::Started;
    } else {
        qCWarning(logger(),
                  "Unsupported request during session start from %ls: %ls",
                  qUtf16Printable(socket->peerAddress().toString()),
                  qUtf16Printable(requestType));
    }
}

void FileTransferSession::Private::receiveFile()
{

    if (socket.isNull()                             // FIXME: softassert
            || localFile->fileName().isEmpty())     // FIXME: softassert
        return;

    const auto stateGuard = makeStateGuard();
    auto sessionGuard = qScopeGuard([this] { cancel(); });

    if (!localFile->isOpen()
            && !localFile->open(QSaveFile::WriteOnly)) {
        qCWarning(logger(), "Could not create local file to receive \"%ls\" from %ls: %ls",
                  qUtf16Printable(localFile->fileName()),
                  qUtf16Printable(socket->peerAddress().toString()),
                  qUtf16Printable(localFile->errorString()));

        reportError(FileError, localFile->errorString(), &stateGuard);
        return;
    }

    while (socket->bytesAvailable() > 0
           && totalBytesReceived < fileInfo.fileSize()) {
        auto data = socket->read(0x10000);
        const auto bytesReceived = data.size();

        if (bytesReceived == 0) {
            qCWarning(logger(), "Failed to receive \"%ls\" from %ls: %ls",
                      qUtf16Printable(localFile->fileName()),
                      qUtf16Printable(socket->peerAddress().toString()),
                      qUtf16Printable(socket->errorString()));

            reportError(NetworkError, socket->errorString(), &stateGuard);
            return;
            return;
        }

        if (localFile->write(std::move(data)) != bytesReceived) {
            qCWarning(logger(), "Failed to receive \"%ls\" from %ls: %ls",
                      qUtf16Printable(localFile->fileName()),
                      qUtf16Printable(socket->peerAddress().toString()),
                      qUtf16Printable(localFile->errorString()));

            reportError(FileError, localFile->errorString(), &stateGuard);
            return;
        }

        qCDebug(logger(), "%lld bytes received for \"%ls\"",
                bytesReceived, qUtf16Printable(localFile->fileName()));

        totalBytesReceived += bytesReceived;

        emit q()->downloadProgress(totalBytesReceived, fileInfo.fileSize(), FileTransferSession::QPrivateSignal{});
    }

    if (totalBytesReceived < fileInfo.fileSize()) {
        sessionGuard.dismiss(); // more data to come
        return;
    }

    qCInfo(logger(), "Transfer of \"%ls\" from %ls has finished",
           qUtf16Printable(localFile->fileName()),
           qUtf16Printable(socket->peerAddress().toString()));

    localFile->commit();
    state = State::Finished;
}

void FileTransferSession::Private::cancel()
{
    if (socket.isNull())
        return;

    socket->close();
    socket->deleteLater(); // it still is owned by the server socket

    q()->deleteLater();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileTransferSession::FileTransferSession(QAbstractSocket *socket, FileSharing *parent)
    : QObject{parent}
    , d{new Private{socket, this}}
{
    connect(this, &FileTransferSession::stateChanged, this, [this](State state) {
        qInfo() << __func__ << state;

        if (state == State::Finished)
            emit finished(QPrivateSignal{});
    });
}

QString FileTransferSession::localFileName() const noexcept
{
    return d->localFile->fileName();
}

FileInfo FileTransferSession::fileInfo() const noexcept
{
    return d->fileInfo;
}

FileTransferSession::State FileTransferSession::state() const noexcept
{
    return d->state;
}

FileTransferSession::Error FileTransferSession::error() const noexcept
{
    return d->error;
}

void FileTransferSession::accept(QString localFileName)
{
    if (localFileName.isEmpty())
        localFileName = d->fileInfo.fileName();

    if (d->socket.isNull())
        return;

    const auto stateGuard = d->makeStateGuard();
    auto sessionGuard = qScopeGuard([this] { d->cancel(); });

    qCInfo(d->logger(), "Accepting file transfer from %ls",
           qUtf16Printable(d->socket->peerAddress().toString()));

    constexpr auto s_response = "install"_L1;

    if (d->socket->write(s_response.data(), s_response.size()) != s_response.size()) {
        qCWarning(d->logger(), "Could not accept file transfer from %ls: %ls",
                  qUtf16Printable(d->socket->peerAddress().toString()),
                  qUtf16Printable(d->socket->errorString()));

        return;
    }

    d->state = State::Accepted;
    d->localFile->setFileName(std::move(localFileName));
    sessionGuard.dismiss();
}

void FileTransferSession::reject()
{
    const auto stateGuard = d->makeStateGuard();
    auto sessionGuard = qScopeGuard([this] { d->cancel(); });

    qCInfo(d->logger(), "Rejection file transfer from %ls",
           qUtf16Printable(d->socket->peerAddress().toString()));

    d->state = State::Rejected;
}

} // namespace lmrs::roco::z21app
