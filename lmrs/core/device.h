#ifndef LMRS_CORE_DEVICE_H
#define LMRS_CORE_DEVICE_H

#include "continuation.h"
#include "model.h"

#include <QAbstractTableModel>
#include <QPointer>

namespace lmrs::core::parameters {
class Parameter;
}

namespace lmrs::core {

class Device;
class DeviceFactory;

using parameters::Parameter;

Q_NAMESPACE

enum class Error { // FIXME: move at a place z21client can use
    NoError,
    NotImplemented,
    RequestFailed,
    InvalidRequest,
    UnknownRequest,
    ValueRejected,
    ShortCircuit,
    Timeout,
};

Q_ENUM_NS(Error)

constexpr auto makeError(bool succeeded) { return (succeeded ? core::Error::NoError : core::Error::RequestFailed); }
Continuation retryOnError(Error error);

template<typename T> // FIXME: move to more generic place, maybe algorithms.h?
struct Result
{
    constexpr bool operator==(const Result &rhs) const { return std::tie(error, value) == std::tie(rhs.error, rhs.value); }
    bool succeeded() const{ return error == Error::NoError; }
    bool failed() const{ return error != Error::NoError; }
    constexpr operator bool() const { return succeeded(); }

    Error error;
    T value;
};

template<typename T>
inline QDebug operator<<(QDebug debug, Result<T> result)
{
    if (result.succeeded())
        return debug << result.value;
    else
        return debug << result.error;
}

///
/// The Control class is the base class for all optional features of a Device.
///
class Control : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual Device *device() const = 0;
};

///
/// The AccessoryControl class is a Control, that provides access to modelrail layout accessories
/// like turnouts, signals, and the like.
///
class AccessoryControl : public Control
{
    Q_OBJECT

public:
    enum class Feature {
        Turnouts = (1 << 0),
        Signals = (1 << 1),
        Durations = (1 << 2),
        EmergencyStop = (1 << 3),
    };

    Q_FLAG(Feature)
    Q_DECLARE_FLAGS(Features, Feature)

    using AccessoryInfoCallback = std::function<void(AccessoryInfo)>;
    using TurnoutInfoCallback = std::function<void(TurnoutInfo)>;

    using Control::Control;

    virtual Features features() const = 0;

    virtual void setAccessoryState(dcc::AccessoryAddress address, quint8 state) = 0;
    virtual void setTurnoutState(dcc::AccessoryAddress address, dcc::TurnoutState state, bool enabled = true) = 0;
    virtual void setTurnoutState(dcc::AccessoryAddress address, dcc::TurnoutState state, std::chrono::milliseconds duration) = 0;

    virtual void requestAccessoryInfo(dcc::AccessoryAddress address, AccessoryInfoCallback callback) = 0;
    virtual void requestTurnoutInfo(dcc::AccessoryAddress address, TurnoutInfoCallback callback) = 0;
    virtual void requestEmergencyStop() = 0;

signals:
    void accessoryInfoChanged(lmrs::core::AccessoryInfo accessoryInfo);
    void turnoutInfoChanged(lmrs::core::TurnoutInfo turnoutInfo);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AccessoryControl::Features)

///
/// The DebugControl class is a Control, that provides facilities for debugging a Device implementation,
/// but also for analysing a modelrail layout and its components.
///
class DebugControl : public Control
{
    Q_OBJECT

public:
    enum class Feature {
        DccFrames = (1 << 0),
        NativeFrames = (1 << 1),
    };

    Q_FLAG(Feature)
    Q_DECLARE_FLAGS(Features, Feature)

    enum class DccFeedbackMode {
        None,
        Acknowledge,
        Advanced,
    };

    Q_ENUM(DccFeedbackMode)

    enum class DccPowerMode {
        Track,
        Service,
    };

    Q_ENUM(DccPowerMode)

    using Control::Control;

    virtual Features features() const noexcept = 0;

    virtual void sendDccFrame(QByteArray frame, DccPowerMode powerMode, DccFeedbackMode feedbackMode) const;
    virtual void sendNativeFrame(QByteArray nativeFrame) const;

    [[nodiscard]] virtual QString nativeProtocolName() const noexcept;
    [[nodiscard]] virtual QList<QPair<QString, QByteArray>> nativeExampleFrames() const noexcept;

signals:
    void dccFrameReceived(QByteArray frame);
    void nativeFrameReceived(QByteArray frame);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DebugControl::Features)

///
/// The PowerControl class is a Control, that allows to manipulate and observe track power for a modelrail layout.
///
class PowerControl : public Control
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::PowerControl::State state READ state NOTIFY stateChanged FINAL)

public:
    enum class State {
        PowerOff,
        PowerOn,
        ServiceMode,
        EmergencyStop,
        ShortCircuit,
    };

    Q_ENUM(State)

    using Control::Control;

    virtual State state() const = 0;

    virtual void enableTrackPower(ContinuationCallback<Error> callback) = 0;
    virtual void disableTrackPower(ContinuationCallback<Error> callback) = 0;

signals:
    void stateChanged(lmrs::core::PowerControl::State state);
};

///
/// The DetectorControl class is a Control, allows to observe response modules, detectors of a modelrail layout.
///
class DetectorControl : public Control
{
    Q_OBJECT

public:
    using Control::Control;

signals:
    void detectorInfoChanged(lmrs::core::DetectorInfo detectorInfo);
};

///
/// The SpeedMeterControl class is a Control, that provides speed measurements for modelrail vehicles.
///
class SpeedMeterControl : public Control
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::millimeters_per_second filteredSpeed READ filteredSpeed NOTIFY filteredSpeedChanged FINAL)
    Q_PROPERTY(lmrs::core::millimeters_per_second rawSpeed READ rawSpeed NOTIFY rawSpeedChanged FINAL)
    Q_PROPERTY(lmrs::core::hertz_f pulses READ pulses NOTIFY pulsesChanged FINAL)

public:
    enum class Feature {
        MeasurePulses = (1 << 0),
        MeasureSpeed = (1 << 1),
    };

    Q_FLAG(Feature)
    Q_DECLARE_FLAGS(Features, Feature)

    using Control::Control;

    virtual Features features() const = 0;
    virtual millimeters_per_second filteredSpeed() const = 0;
    virtual millimeters_per_second rawSpeed() const = 0;
    virtual hertz_f pulses() const = 0;

    auto hasFeature(Feature feature) const { return features().testFlag(feature); }

signals:
    void dataReceived(std::chrono::steady_clock::time_point timestamp);
    void filteredSpeedChanged(lmrs::core::millimeters_per_second filteredSpeed);
    void rawSpeedChanged(lmrs::core::millimeters_per_second rawSpeed);
    void pulsesChanged(lmrs::core::hertz_f pulses);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SpeedMeterControl::Features)

///
/// The VariableControl class is a Control, that provides access to configuration variables
/// of modelrail vehicles and accessories.
///
class VariableControl : public Control
{
    Q_OBJECT

public:
    enum class Feature {
        DirectProgramming = (1 << 0),
        ProgrammingOnMain = (1 << 1),
    };

    Q_FLAG(Feature)
    Q_DECLARE_FLAGS(Features, Feature)

    using Control::Control;

    using VariableValueResult = Result<dcc::VariableValue>;
    using ExtendedVariableList = QList<dcc::ExtendedVariableIndex>;
    using ExtendedVariableResults = QHash<dcc::ExtendedVariableIndex, VariableValueResult>;

    virtual Features features() const = 0;

    virtual void readVariable(dcc::VehicleAddress address, dcc::VariableIndex variable,
                              ContinuationCallback<VariableValueResult> callback) = 0;
    virtual void writeVariable(dcc::VehicleAddress address,
                               dcc::VariableIndex variable, dcc::VariableValue value,
                               ContinuationCallback<VariableValueResult> callback) = 0;

    virtual void readExtendedVariable(dcc::VehicleAddress address, dcc::ExtendedVariableIndex variable,
                                      ContinuationCallback<VariableValueResult> callback);
    virtual void readExtendedVariables(dcc::VehicleAddress address, ExtendedVariableList variables,
                                      ContinuationCallback<dcc::ExtendedVariableIndex, VariableValueResult> callback);
    virtual void readExtendedVariables(dcc::VehicleAddress address, ExtendedVariableList variables,
                                       std::function<void(ExtendedVariableResults)> callback);

    virtual void writeExtendedVariable(dcc::VehicleAddress address,
                                       dcc::ExtendedVariableIndex variable, dcc::VariableValue value,
                                       ContinuationCallback<VariableValueResult> callback);

protected:
    virtual void selectPage(dcc::VehicleAddress address, dcc::ExtendedPageIndex page,
                            ContinuationCallback<Error> callback);
    virtual void selectPage(dcc::VehicleAddress address, dcc::SusiPageIndex page,
                            ContinuationCallback<Error> callback);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VariableControl::Features)

///
/// The VehicleControl class is a Control, that allows to manipulate behavior of modelrail vehicles.
///
class VehicleControl : public Control
{
    Q_OBJECT

public:
    using Control::Control;

    enum SubscriptionType { NormalSubscription, PrimarySubscription, CancelSubscription };
    Q_ENUM(SubscriptionType)

    virtual void subscribe(dcc::VehicleAddress address, SubscriptionType type = NormalSubscription) = 0;
    virtual void unsubscribe(dcc::VehicleAddress address) { subscribe(address, CancelSubscription); }

    virtual void setSpeed(dcc::VehicleAddress address, dcc::Speed speed, dcc::Direction direction) = 0;
    virtual void setFunction(dcc::VehicleAddress address, dcc::Function function, bool enabled = true) = 0;

signals:
    void vehicleNameChanged(lmrs::core::dcc::VehicleAddress, QString name);
    void vehicleInfoChanged(lmrs::core::VehicleInfo vehicleInfo);
};

///
/// The DeviceInfo enum uniformly describes apects of a Device.
///
enum class DeviceInfo {
    ManufacturerId,
    ProductId,
    HardwareVersion,
    SerialNumber,
    CanAddress,
    HardwareLock,
    DeviceAddress,
    DevicePort,
    ProductionDate,
    BootloaderVersion,
    BootloaderDate,
    FirmwareVersion,
    FirmwareDate,
    FirmwareType,
    ProtocolVersion,
    ProtocolClientId,
    DeviceStatus,
    Capabilities,
    TrackStatus,
    MainTrackCurrent,
    MainTrackCurrentFiltered,
    MainTrackVoltage,
    ProgrammingTrackCurrent,
    ProgrammingTrackVoltage,
    SupplyVoltage,
    Temperature,
};

Q_ENUM_NS(DeviceInfo)

///
/// The DeviceInfoModel class is a QAbstractTableModel that provides canonical information about a modelrail Device.
///
class DeviceInfoModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::Device *device READ device WRITE setDevice NOTIFY deviceChanged FINAL)

public:
    enum class Column {
        Name,
        Value,
    };

    Q_ENUM(Column)

    using QAbstractTableModel::QAbstractTableModel;

    void setDevice(Device *device);
    Device *device() const;

public: // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

signals:
    void deviceChanged(lmrs::core::Device *device);

private:
    void onDeviceInfoChanged(QList<DeviceInfo> changedIds);

    struct Row
    {
        QVariant value;
        QString text;
    };

    QMap<DeviceInfo, Row> m_rows;
    QPointer<Device> m_device;
};

///
/// The Device class describes a modelrail related device that can be controlled by a computer.
///
/// Examples for such devices are DCC command stations, but also roller dynamometer. As the capabilities
/// of such devices vary greatly, the various aspects of such devices are described via optional Control instances.
///
class Device : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
    };

    Q_ENUM(State)

    using QObject::QObject;

    [[nodiscard]] virtual State state() const = 0;
    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual QString uniqueId() const = 0;

    virtual bool connectToDevice()  = 0;
    virtual void disconnectFromDevice()  = 0;

    virtual DeviceFactory *factory() = 0;

    [[nodiscard]] virtual const AccessoryControl *accessoryControl() const { return const_cast<Device *>(this)->accessoryControl(); }
    [[nodiscard]] virtual const DebugControl *debugControl() const { return const_cast<Device *>(this)->debugControl(); }
    [[nodiscard]] virtual const PowerControl *powerControl() const { return const_cast<Device *>(this)->powerControl(); }
    [[nodiscard]] virtual const DetectorControl *detectorControl() const { return const_cast<Device *>(this)->detectorControl(); }
    [[nodiscard]] virtual const SpeedMeterControl *speedMeterControl() const { return const_cast<Device *>(this)->speedMeterControl(); }
    [[nodiscard]] virtual const VariableControl *variableControl() const { return const_cast<Device *>(this)->variableControl(); }
    [[nodiscard]] virtual const VehicleControl *vehicleControl() const { return const_cast<Device *>(this)->vehicleControl(); }

    [[nodiscard]] virtual AccessoryControl *accessoryControl() { return nullptr; }
    [[nodiscard]] virtual DebugControl *debugControl() { return nullptr; }
    [[nodiscard]] virtual PowerControl *powerControl() { return nullptr; }
    [[nodiscard]] virtual DetectorControl *detectorControl() { return nullptr; }
    [[nodiscard]] virtual SpeedMeterControl *speedMeterControl() { return nullptr; }
    [[nodiscard]] virtual VariableControl *variableControl() { return nullptr; }
    [[nodiscard]] virtual VehicleControl *vehicleControl() { return nullptr; }

    template<class T> [[nodiscard]] const T *control() const;
    template<class T> [[nodiscard]] T *control();

    [[nodiscard]] virtual QVariant deviceInfo(DeviceInfo id, int role = Qt::EditRole) const = 0;
    [[nodiscard]] QString deviceInfoText(DeviceInfo id) const;
    [[nodiscard]] QString deviceInfoDisplayText(DeviceInfo id) const;

    virtual void updateDeviceInfo() = 0;

signals:
    void stateChanged(lmrs::core::Device::State state);
    void deviceInfoChanged(QList<lmrs::core::DeviceInfo> changedIds);
    void controlsChanged();

protected:
    template<class Observable, typename T, typename U = T>
    void observe(core::DeviceInfo id, Observable *observable,
                 T (Observable::*getter)() const, void (Observable::*notify)(T),
                 U (*convert)(T) = [](T value) { return value; });

    virtual void setDeviceInfo(core::DeviceInfo id, QVariant value) = 0;
};

template<> inline const  AccessoryControl *Device::control() const { return accessoryControl(); }
template<> inline        AccessoryControl *Device::control()       { return accessoryControl(); }
template<> inline const      DebugControl *Device::control() const { return debugControl(); }
template<> inline            DebugControl *Device::control()       { return debugControl(); }
template<> inline const   DetectorControl *Device::control() const { return detectorControl(); }
template<> inline         DetectorControl *Device::control()       { return detectorControl(); }
template<> inline const      PowerControl *Device::control() const { return powerControl(); }
template<> inline            PowerControl *Device::control()       { return powerControl(); }
template<> inline const SpeedMeterControl *Device::control() const { return speedMeterControl(); }
template<> inline       SpeedMeterControl *Device::control()       { return speedMeterControl(); }
template<> inline const   VariableControl *Device::control() const { return variableControl(); }
template<> inline         VariableControl *Device::control()       { return variableControl(); }
template<> inline const    VehicleControl *Device::control() const { return vehicleControl(); }
template<> inline          VehicleControl *Device::control()       { return vehicleControl(); }

template<class Observable, typename T, typename U>
inline void Device::observe(core::DeviceInfo id, Observable *observable,
                            T (Observable::*getter)() const, void (Observable::*notify)(T),
                            U (*convert)(T))
{
    const auto updateDeviceInfo = [this, convert, id](auto value) {
        setDeviceInfo(id, QVariant::fromValue(convert(std::move(value))));
    };

    connect(observable, notify, this, updateDeviceInfo);
    updateDeviceInfo((observable->*getter)());
}

///
/// The DeviceFactory class is used to describe and create Device instances.
///
/// Supported parameters of the managed Device are described by the parameters() function.
/// To create an instance use the create() function.
///
class DeviceFactory : public QObject
{
public:
    using QObject::QObject;

    /// A description of the Device class instances produced by the DeviceFactory that can be shown to users.
    [[nodiscard]] virtual QString name() const = 0;

    /// The parameters that have to be passed to create() when creating a new Device instance.
    [[nodiscard]] virtual QList<Parameter> parameters() const = 0;

    /// Creates a new Device instance using the configuration described by @p parameters.
    /// The Device will be created parent, if @p parent is not @c null.
    [[nodiscard]] virtual Device *create(QVariantMap parameters, QObject *parent = nullptr) = 0;

    /// Unique identifier of the Device described by @p parameters.
    /// This identifier is used for storing the Device's configuration within the local settings.
    [[nodiscard]] virtual QString uniqueId(QVariantMap parameters);

    /// Registers @p factory with the LMRS library.
    /// A list of registered factories can be obtained using the deviceFactories() function.
    static void addDeviceFactory(DeviceFactory *factory);

    /// A list of DeviceFactory instances registered with the LMRS library.
    [[nodiscard]] static QList<DeviceFactory *> deviceFactories();
};

} // namespace lmrs::core

#endif // LMRS_CORE_DEVICE_H
