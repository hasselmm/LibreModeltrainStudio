#ifndef LMRS_STUDIO_FUNCTIONMAPPINGVIEW_H
#define LMRS_STUDIO_FUNCTIONMAPPINGVIEW_H

#include "mainwindow.h"

#include <QTableView>

namespace lmrs::core {
class VariableControl;
} // namespace lmrs::core

namespace lmrs::studio {

class FunctionMappingView : public MainWindowView
{
    Q_OBJECT

public:
    explicit FunctionMappingView(QWidget *parent = {});

    QList<QActionGroup *> actionGroups(ActionCategory category) const override;
    QString fileName() const override;
    bool isModified() const override;

    DeviceFilter deviceFilter() const override;

    void setDevice(core::Device *newDevice) override;
    core::Device *device() const;

    void setVariableControl(core::VariableControl *newControl);
    core::VariableControl *variableControl() const;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_FUNCTIONMAPPINGVIEW_H
