#ifndef LMRS_ROCO_Z21_DEVICE_H
#define LMRS_ROCO_Z21_DEVICE_H

#include <lmrs/core/device.h>
#include <lmrs/core/memory.h>

class QHostAddress;

namespace lmrs::roco::z21 {

class DeviceFactory : public core::DeviceFactory
{
    Q_OBJECT

public:
    using core::DeviceFactory::DeviceFactory;

public: // DeviceFactory interface
    QString name() const override;
    QList<core::Parameter> parameters() const override;
    core::Device *create(QVariantMap parameters, QObject *parent = nullptr) override;
};

class Device : public core::Device
{
    Q_OBJECT

public:
    explicit Device(QHostAddress address, DeviceFactory *factory, QObject *parent = nullptr);

public: // Device interface
    State state() const override;
    QString name() const override;
    QString uniqueId() const override;

    bool connectToDevice() override;
    void disconnectFromDevice() override;

    core::DeviceFactory *factory() override;

    core::AccessoryControl *accessoryControl() override;
    core::DebugControl *debugControl() override;
    core::DetectorControl *detectorControl() override;
    core::PowerControl *powerControl() override;
    core::VariableControl *variableControl() override;
    core::VehicleControl *vehicleControl() override;

    QVariant deviceInfo(core::DeviceInfo id, int role = Qt::EditRole) const override;
    void updateDeviceInfo() override;

protected:
    void setDeviceInfo(core::DeviceInfo id, QVariant value) override;

private:
    class Private;
    core::ConstPointer<Private> d;
};

} // namespace lmrs::roco::z21

#endif // LMRS_ROCO_Z21_DEVICE_H
