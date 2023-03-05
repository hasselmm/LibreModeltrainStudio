#ifndef LMRS_STUDIO_AUTOMATIONVIEW_H
#define LMRS_STUDIO_AUTOMATIONVIEW_H

#include "mainwindow.h"

#include <lmrs/core/memory.h>

namespace lmrs::studio {

class AutomationView : public MainWindowView
{
    Q_OBJECT

public:
    explicit AutomationView(QWidget *parent = nullptr);

    QList<QActionGroup *> actionGroups(ActionCategory category) const override;
    FileState fileState() const override;
    QString fileName() const override;

    DeviceFilter deviceFilter() const override;

public slots:
    bool open(QString newFileName);

protected:
    void updateControls(core::Device *device) override;

private:
    class Private;
    core::ConstPointer<Private> d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_AUTOMATIONVIEW_H
