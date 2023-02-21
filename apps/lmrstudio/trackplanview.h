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
    explicit TrackPlanView(QAbstractItemModel *model, QWidget *parent = nullptr); // FIXME: pass model factory, not model

    void setFileSharing(roco::z21app::FileSharing *newFileSharing);
    roco::z21app::FileSharing *fileSharing() const;

    QActionGroup *actionGroup(ActionCategory category) const override;

public slots:
    bool open(QString fileName);

private:
    class Private;
    Private *const d;
};

} // namespace studio::lmrs

#endif // LMRS_STUDIO_TRACKPLANVIEW_H
