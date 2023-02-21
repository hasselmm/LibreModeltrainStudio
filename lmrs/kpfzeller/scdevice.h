#ifndef LMRS_KPFZELLER_SCDEVICE_H
#define LMRS_KPFZELLER_SCDEVICE_H

#include <lmrs/core/device.h>
#include <lmrs/core/parameters.h>

namespace lmrs::kpfzeller {

class DeviceFactory : public core::DeviceFactory
{
    Q_OBJECT

public:
    using core::DeviceFactory::DeviceFactory;

    QString name() const override;
    QList<core::Parameter> parameters() const override;
    core::Device *create(QVariantMap parameters, QObject *parent = nullptr) override;
};

class SpeedCatDevice : public core::Device
{
    Q_OBJECT

public:
    enum class Scale {
        Invalid,
        RawPulses,
        Gauge1,
        Scale0,
        ScaleH0,
        ScaleTT,
        ScaleN,
    };

    Q_ENUM(Scale)

    enum class RubberType {
        None,
        Red,
    };

    explicit SpeedCatDevice(QString portName, Scale scale, RubberType rubber,
                            QObject *parent = {}, DeviceFactory *factory = {});

public: // Device interface
    State state() const override;
    QString name() const override;
    QString uniqueId() const override;

    bool connectToDevice() override;
    void disconnectFromDevice() override;

    core::DeviceFactory *factory() override;
    core::SpeedMeterControl *speedMeterControl() override;
    const core::SpeedMeterControl *speedMeterControl() const override;

    QVariant deviceInfo(core::DeviceInfo id, int role = Qt::EditRole) const override;
    void updateDeviceInfo() override;

protected:
    void setDeviceInfo(core::DeviceInfo id, QVariant value) override;

private:
    class SpeedMeter;

    class Private;
    Private *const d;
};

} // namespace lmrs::kpfzeller

#endif // LMRS_KPFZELLER_SCDEVICE_H
