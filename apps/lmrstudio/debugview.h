#ifndef LMRS_STUDIO_DEBUGVIEW_H
#define LMRS_STUDIO_DEBUGVIEW_H

#include "mainwindow.h"

namespace lmrs::core {
class DebugControl;
}

namespace lmrs::studio {

class DebugView : public MainWindowView
{
    Q_OBJECT

public:
    explicit DebugView(QWidget *parent = nullptr);

    DeviceFilter deviceFilter() const override;
    void setDevice(core::Device *newDevice) override;
    core::Device *device() const;

    void setDebugControl(core::DebugControl *newControl);
    core::DebugControl *debugControl() const;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_DEBUGVIEW_H
