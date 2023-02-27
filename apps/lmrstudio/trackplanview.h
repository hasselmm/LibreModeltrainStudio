#ifndef LMRS_STUDIO_TRACKPLANVIEW_H
#define LMRS_STUDIO_TRACKPLANVIEW_H

#include "mainwindow.h"

class QAbstractItemModel;

namespace lmrs::roco::z21app {
class FileSharing;
} // namespace lmrs::roco::z21app

namespace lmrs::studio {

class TrackPlanView : public MainWindowView
{
    Q_OBJECT

public:
    explicit TrackPlanView(QWidget *parent = nullptr);

    void setFileSharing(roco::z21app::FileSharing *newFileSharing);
    roco::z21app::FileSharing *fileSharing() const;

    QActionGroup *actionGroup(ActionCategory category) const override;
    QString fileName() const override;
    bool isModified() const override;

    void setCurrentDevice(core::Device *newDevice) override;
    DeviceFilter deviceFilter() const override;

public slots:
    bool open(QString newFileName);

private:
    class Private;
    Private *const d;
};

} // namespace studio::lmrs

#endif // LMRS_STUDIO_TRACKPLANVIEW_H
