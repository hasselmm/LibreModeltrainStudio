#ifndef LMRS_WIDGETS_SYMBOLICTRACKPLANVIEW_H
#define LMRS_WIDGETS_SYMBOLICTRACKPLANVIEW_H

#include <QWidget>

namespace lmrs::core {
class AccessoryControl;
class DetectorControl;
class SymbolicTrackPlanModel;
}

namespace lmrs::widgets {

class SymbolicTrackPlanView : public QWidget
{
    Q_OBJECT

public:
    explicit SymbolicTrackPlanView(QWidget *parent = nullptr);

    void setModel(core::SymbolicTrackPlanModel *newModel);
    core::SymbolicTrackPlanModel *model() const;

    void setAccessoryControl(core::AccessoryControl *newControl);
    core::AccessoryControl *accessoryControl() const;

    void setDetectorControl(core::DetectorControl *newControl);
    core::DetectorControl *detectorControl() const;

    void setTileSize(int newSize);
    int tileSize() const;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void modelChanged(lmrs::core::SymbolicTrackPlanModel *model);
    void accessoryControlChanged(lmrs::core::AccessoryControl *control);
    void detectorControlChanged(lmrs::core::DetectorControl *control);
    void tileSizeChanged(int tileSize);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_SYMBOLICTRACKPLANVIEW_H
