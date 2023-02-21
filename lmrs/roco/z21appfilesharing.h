#ifndef LMRS_ROCO_Z21APP_FILESHARING_H
#define LMRS_ROCO_Z21APP_FILESHARING_H

#include <lmrs/core/propertyguard.h>

#include <QHostAddress>
#include <QJsonObject>
#include <QObject>

namespace lmrs::roco::z21app {

class DeviceInfo
{
    Q_GADGET

public:
    DeviceInfo(QJsonObject info, QHostAddress address, quint16 port) noexcept
        : m_deviceInfo{std::move(info)}
        , m_address(std::move(address))
        , m_port{port}
    {}

    [[nodiscard]] auto deviceInfo() const { return m_deviceInfo; }
    [[nodiscard]] auto deviceAddress() const { return m_address; }
    [[nodiscard]] auto devicePort() const { return m_port; }

    [[nodiscard]] QString deviceName() const;
    [[nodiscard]] QString deviceType() const;
    [[nodiscard]] QString devicePlatform() const;
    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] int protocolVersion() const;
    [[nodiscard]] int buildNumber() const;

    [[nodiscard]] bool isValid() const;

private:
    QJsonObject m_deviceInfo;
    QHostAddress m_address;
    quint16 m_port;
};

class FileInfo
{
    Q_GADGET

public:
    FileInfo() = default;
    FileInfo(QJsonObject info, QHostAddress address) noexcept
        : m_fileInfo{std::move(info)}
        , m_address(std::move(address))
    {}

    [[nodiscard]] auto fileInfo() const { return m_fileInfo; }
    [[nodiscard]] auto deviceAddress() const { return m_address; }

    [[nodiscard]] QString fileName() const;
    [[nodiscard]] qint64 fileSize() const;
    [[nodiscard]] DeviceInfo owningDevice() const;

    [[nodiscard]] bool isValid() const;

private:
    QJsonObject m_fileInfo;
    QHostAddress m_address;
};

class FileSharing;
class FileTransferSession : public QObject
{
    Q_OBJECT

public:
    enum Error {
        NoError,
        NetworkError,
        FileError,
    };

    Q_ENUM(Error)

    enum class State
    {
        Connected,
        Started,
        Accepted,
        Rejected,
        Finished,
    };

    Q_ENUM(State)

// FIXME private:
    FileTransferSession(QAbstractSocket *socket, FileSharing *parent);

public:
    QString localFileName() const noexcept;
    FileInfo fileInfo() const noexcept;
    State state() const noexcept;
    Error error() const noexcept;

public slots:
    void accept(QString localFileName = {});
    void reject();

signals:
    void stateChanged(lmrs::roco::z21app::FileTransferSession::State state, QPrivateSignal);
    void downloadProgress(qint64 bytesReceived, qint64 totalBytes, QPrivateSignal);
    void finished(QPrivateSignal);

private:
    class Private;
    Private *const d;
};

class FileSharing : public QObject
{
    Q_OBJECT

public:
    explicit FileSharing(QObject *parent = nullptr);

    bool isListening() const;
    bool startListening();
    void stopListening();

    bool discoverDevices();

signals:
    void isListeningChanged(bool isListening);
    void deviceDiscovered(lmrs::roco::z21app::DeviceInfo deviceInfo, QPrivateSignal);
    void fileTransferOffered(lmrs::roco::z21app::FileTransferSession *session, QPrivateSignal);

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::roco::z21app

#endif // LMRS_ROCO_Z21APP_FILESHARING_H
