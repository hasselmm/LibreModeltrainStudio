#ifndef LMRS_STUDIO_VARIABLEEDITORVIEW_H
#define LMRS_STUDIO_VARIABLEEDITORVIEW_H

#include <lmrs/core/dccconstants.h>
#include <lmrs/core/device.h>

#include <QWidget>

namespace lmrs::widgets {
class StatusBar;
}

namespace lmrs::studio {

class VariableEditorView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::VariableControl *variableControl READ variableControl
               WRITE setVariableControl NOTIFY variableControlChanged FINAL)

public:
    explicit VariableEditorView(QWidget *parent = nullptr);
    ~VariableEditorView() override;

    void setDevice(core::Device *device) { setVariableControl(device->variableControl()); }
    core::Device *device() const { return variableControl()->device(); }

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
