#ifndef LMRS_STUDIO_SPEEDMETERVIEW_H
#define LMRS_STUDIO_SPEEDMETERVIEW_H

#include <lmrs/core/device.h>

#include <QWidget>

namespace lmrs::studio {

class SpeedMeterView : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedMeterView(QWidget *parent = nullptr);

    void setDevice(core::Device *device) { setSpeedMeter(device->speedMeterControl()); }
    core::Device *device() const { return speedMeter()->device(); }

    void setSpeedMeter(core::SpeedMeterControl *newSpeedMeter);
    core::SpeedMeterControl *speedMeter() const;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_SPEEDMETERVIEW_H
