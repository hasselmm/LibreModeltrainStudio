#ifndef LMRS_CORE_ESU_LOKPROGRAMMER_H
#define LMRS_CORE_ESU_LOKPROGRAMMER_H

#include <lmrs/core/dccconstants.h>

#include <QObject>

namespace lmrs::esu::lp2 {
class Request;
class Response;

struct PowerSettings;

enum class InterfaceInfo : quint8;
}

namespace esu::programmer {

using namespace lmrs::core; // FIXME
using namespace lmrs::esu::lp2;

class LokProgrammer : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Error,
    };

    Q_ENUM(State)

    enum class Result {
        Success,
        Failure,
    };

    Q_ENUM(Result)

    using ResultCallback = std::function<void(Result)>;
    using ResponseCallback = std::function<void(Response)>;

    explicit LokProgrammer(QObject *parent = nullptr);

    static QStringList availablePorts();

    QString errorString() const;
    State state() const;

    void powerOn(PowerSettings setting, ResultCallback callback);
    void powerOff(ResultCallback callback);

    void sendRequest(Request request, ResponseCallback callback);

    using ReadDeviceInfoCallback = std::function<void(QHash<InterfaceInfo, QVariant>)>;
    void readDeviceInformation(QList<InterfaceInfo> ids, ReadDeviceInfoCallback callback);

    using ReadVariableCallback = std::function<void(Result, quint8)>;
    void readVariable(dcc::VariableIndex variable, ReadVariableCallback callback);
    void readVariable(dcc::ExtendedVariableIndex variable, ReadVariableCallback callback);
    void readVariable(dcc::VehicleVariable variable, ReadVariableCallback callback);

    using WriteVariableCallback = ResultCallback;
    void writeVariable(dcc::VariableIndex variable, dcc::VariableValue value, WriteVariableCallback callback);

public slots:
    bool connectToDevice(QString portName);
    void disconnectFromDevice();

signals:
    void errorOccured(QString errorString);
    void stateChanged(State state);

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::core::esu

#endif // LMRS_CORE_ESU_LOKPROGRAMMER_H
