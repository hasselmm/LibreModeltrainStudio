#ifndef LMRS_STUDIO_ACCESSORYCONTROLVIEW_H
#define LMRS_STUDIO_ACCESSORYCONTROLVIEW_H

#include <lmrs/core/device.h>

#include <QWidget>

namespace lmrs::studio {

class AccessoryControlView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::AccessoryControl *accessoryControl READ accessoryControl
               WRITE setAccessoryControl NOTIFY accessoryControlChanged FINAL)

public:
    explicit AccessoryControlView(QWidget *parent = nullptr);
    ~AccessoryControlView() override;

    void setDevice(core::Device *device);
    core::Device *device() const;

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

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_ACCESSORYCONTROLVIEW_H
