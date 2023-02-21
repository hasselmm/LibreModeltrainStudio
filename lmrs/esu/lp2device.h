#ifndef LMRS_ESU_LP2DEVICE_H
#define LMRS_ESU_LP2DEVICE_H

#include "lp2constants.h"

#include <lmrs/core/device.h>
#include <lmrs/core/memory.h>

namespace lmrs::esu::lp2 {

class DeviceFactory : public core::DeviceFactory
{
    Q_OBJECT

public:
    using core::DeviceFactory::DeviceFactory;

    QString name() const override;
    QList<core::Parameter> parameters() const override;
    core::Device *create(QVariantMap parameters, QObject *parent = nullptr) override;
};

class Device : public core::Device
{
    Q_OBJECT

public:
    enum class Result { Success, Failure };
    Q_ENUM(Result)

    explicit Device(QString portName, DeviceFactory *factory, QObject *parent = {});

    using ReadDeviceInfoCallback = std::function<void(Result result, QHash<lp2::InterfaceInfo, QVariant>)>;
    void readDeviceInformation(QList<lp2::InterfaceInfo> ids, ReadDeviceInfoCallback callback);

public: // Device interface
    State state() const override;
    QString name() const override;
    QString uniqueId() const override;

    bool connectToDevice() override;
    void disconnectFromDevice() override;

    core::DeviceFactory *factory() override;

    core::DebugControl *debugControl() override;
    core::PowerControl *powerControl() override;
    core::VariableControl *variableControl() override;
    core::VehicleControl *vehicleControl() override;

    QVariant deviceInfo(core::DeviceInfo id, int role = Qt::EditRole) const override;
    void updateDeviceInfo() override;

protected:
    void setDeviceInfo(core::DeviceInfo id, QVariant value) override;

private:
    class DebugControl;
    class PowerControl;
    class VariableControl;
    class VehicleControl;

    class Private;
    core::ConstPointer<Private> d;
};

} // namespace lmrs::esu::lp2

#endif // LMRS_ESU_LP2DEVICE_H
