#ifndef LMRS_ZIMO_MX1_DEVICE_H
#define LMRS_ZIMO_MX1_DEVICE_H

#include <lmrs/core/device.h>
#include <lmrs/core/memory.h>

namespace lmrs::zimo::mx1 {

class DeviceFactory : public core::DeviceFactory
{
    Q_OBJECT

public:
    using core::DeviceFactory::DeviceFactory;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QList<core::Parameter> parameters() const override;
    [[nodiscard]] core::Device *create(QVariantMap parameters, QObject *parent = nullptr) override;
};

class Device : public lmrs::core::Device
{
    Q_OBJECT

public:
    explicit Device(QString portName, int portSpeed, DeviceFactory *factory, QObject *parent = {});

    [[nodiscard]] State state() const override;
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString uniqueId() const override;

    bool connectToDevice() override;
    void disconnectFromDevice() override;

    [[nodiscard]] QVariant deviceInfo(core::DeviceInfo id, int role = Qt::EditRole) const override;
    void updateDeviceInfo() override;

    [[nodiscard]] core::DeviceFactory *factory() override;

    [[nodiscard]] core::AccessoryControl *accessoryControl() override;
    [[nodiscard]] core::DebugControl *debugControl() override;
    [[nodiscard]] core::PowerControl *powerControl() override;
    [[nodiscard]] core::VariableControl *variableControl() override;
    [[nodiscard]] core::VehicleControl *vehicleControl() override;

protected:
    void setDeviceInfo(core::DeviceInfo id, QVariant value) override;

private:
    class AccessoryControl;
    class DebugControl;
    class PowerControl;
    class VariableControl;
    class VehicleControl;

    class Private;
    core::ConstPointer<Private> d;
};

} // namespace lmrs::zimo::mx1

#endif // LMRS_ZIMO_MX1_DEVICE_H
