#ifndef LMRS_STUDIO_SPEEDMETERVIEW_H
#define LMRS_STUDIO_SPEEDMETERVIEW_H

#include "mainwindow.h"

namespace lmrs::core {
class SpeedMeterControl;
}

namespace lmrs::studio {

class SpeedMeterView : public MainWindowView
{
    Q_OBJECT

public:
    explicit SpeedMeterView(QWidget *parent = nullptr);
    DeviceFilter deviceFilter() const override;

    void setSpeedMeter(core::SpeedMeterControl *newSpeedMeter);
    core::SpeedMeterControl *speedMeter() const;

protected:
    void updateControls(core::Device *newDevice) override;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_SPEEDMETERVIEW_H
