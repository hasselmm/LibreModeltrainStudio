#ifndef LMRS_STUDIO_VARIABLEEDITORVIEW_H
#define LMRS_STUDIO_VARIABLEEDITORVIEW_H

#include "mainwindow.h"

#include <lmrs/core/dccconstants.h>

namespace lmrs::core {
class VariableControl;
}

namespace lmrs::widgets {
class StatusBar;
}

namespace lmrs::studio {

class VariableEditorView : public MainWindowView
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::VariableControl *variableControl READ variableControl
               WRITE setVariableControl NOTIFY variableControlChanged FINAL)

public:
    explicit VariableEditorView(QWidget *parent = nullptr);
    DeviceFilter deviceFilter() const override;
    void setDevice(core::Device *newDevice) override;
    core::Device *device() const;

    void setVariableControl(core::VariableControl *variableControl);
    core::VariableControl *variableControl() const;

    void setCurrentVehicle(core::dcc::VehicleAddress address);
    core::dcc::VehicleAddress currentVehicle() const;

    core::dcc::ExtendedVariableIndex currentVariable() const;

    void setStatusBar(widgets::StatusBar *statusBar);
    widgets::StatusBar *statusBar() const;

signals:
    void currentVehicleChanged(lmrs::core::dcc::VehicleAddress address);
    void variableControlChanged(lmrs::core::VariableControl *variableControl);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_VARIABLEEDITORVIEW_H
