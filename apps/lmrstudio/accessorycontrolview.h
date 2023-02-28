#ifndef LMRS_STUDIO_ACCESSORYCONTROLVIEW_H
#define LMRS_STUDIO_ACCESSORYCONTROLVIEW_H

#include "mainwindow.h"

#include <lmrs/core/dccconstants.h>

namespace lmrs::core {
class AccessoryControl;
class DetectorControl;
}

namespace lmrs::studio {

class AccessoryControlView : public MainWindowView
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::AccessoryControl *accessoryControl READ accessoryControl
               WRITE setAccessoryControl NOTIFY accessoryControlChanged FINAL)

public:
    explicit AccessoryControlView(QWidget *parent = nullptr);

    DeviceFilter deviceFilter() const override;

    void setAccessoryControl(core::AccessoryControl *newControl);
    core::AccessoryControl *accessoryControl() const;

    void setDetectorControl(core::DetectorControl *newControl);
    core::DetectorControl *detectorControl() const;

    void setCurrentAccessory(core::dcc::AccessoryAddress address);
    core::dcc::AccessoryAddress currentAccessory() const;

signals:
    void currentAccessoryChanged(lmrs::core::dcc::AccessoryAddress address);
    void accessoryControlChanged(lmrs::core::AccessoryControl *accessoryControl);
    void detectorControlChanged(lmrs::core::DetectorControl *detectorControl);

protected:
    void updateControls(core::Device *newDevice) override;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_ACCESSORYCONTROLVIEW_H
