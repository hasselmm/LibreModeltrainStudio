#ifndef LMRS_STUDIO_DEBUGVIEW_H
#define LMRS_STUDIO_DEBUGVIEW_H

#include <lmrs/core/device.h>

#include <QWidget>

namespace lmrs::studio {

class DebugView : public QWidget
{
    Q_OBJECT

public:
    explicit DebugView(QWidget *parent = nullptr);

    void setDevice(core::Device *device) { setDebugControl(device->debugControl()); }
    core::Device *device() const { return debugControl()->device(); }

    void setDebugControl(core::DebugControl *newControl);
    core::DebugControl *debugControl() const;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_DEBUGVIEW_H
