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
    DeviceFilter deviceFilter() const override;

    void setFileSharing(roco::z21app::FileSharing *newFileSharing);
    roco::z21app::FileSharing *fileSharing() const;

    QList<QActionGroup *> actionGroups(ActionCategory category) const override;
    QString fileName() const override;
    bool isModified() const override;

public slots:
    bool open(QString newFileName);

protected:
    void updateControls(core::Device *newDevice) override;

private:
    class Private;
    Private *const d;
};

} // namespace studio::lmrs

#endif // LMRS_STUDIO_TRACKPLANVIEW_H
