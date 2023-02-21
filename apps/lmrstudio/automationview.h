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

    QActionGroup *actionGroup(ActionCategory category) const override;

public slots:
    bool open(QString newFileName);

private:
    class Private;
    core::ConstPointer<Private> d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_AUTOMATIONVIEW_H
