#ifndef LMRS_STUDIO_VEHICLECONTROLVIEW_H
#define LMRS_STUDIO_VEHICLECONTROLVIEW_H

#include <lmrs/core/dccconstants.h>
#include <lmrs/core/device.h>

#include <QWidget>

namespace lmrs::studio {

class VehicleControlView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::VehicleControl *vehicleControl READ vehicleControl
               WRITE setVehicleControl NOTIFY vehicleControlChanged FINAL)

public:
    explicit VehicleControlView(QWidget *parent = nullptr);
    ~VehicleControlView() override;

    void setDevice(core::Device *device);
    core::Device *device() const;

    void setVariableControl(core::VariableControl *variableControl);
    core::VariableControl *variableControl() const;

    void setVehicleControl(core::VehicleControl *vehicleControl);
    core::VehicleControl *vehicleControl() const;

    void setCurrentVehicle(core::dcc::VehicleAddress address);
    core::dcc::VehicleAddress currentVehicle() const;

signals:
    void currentVehicleChanged(lmrs::core::dcc::VehicleAddress address);
    void variableControlChanged(lmrs::core::VariableControl *variableControl);
    void vehicleControlChanged(lmrs::core::VehicleControl *vehicleControl);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_VEHICLECONTROLVIEW_H
