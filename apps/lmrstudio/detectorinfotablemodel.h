#ifndef LMRS_STUDIO_DETECTORINFOTABLEMODEL_H
#define LMRS_STUDIO_DETECTORINFOTABLEMODEL_H

#include <lmrs/core/model.h>

#include <QAbstractTableModel>
#include <QPointer>

namespace lmrs::core {
class DetectorControl;
}

namespace lmrs::studio {

class DetectorInfoTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum DataColumn {
        TypeColumn,
        ModuleColumn,
        PortColumn,
        OccupancyColumn,
        PowerStateColumn,
        VehiclesColumn,
    };

    Q_ENUM(DataColumn)

    using QAbstractTableModel::QAbstractTableModel;

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setDetectorControl(core::DetectorControl *newControl);
    core::DetectorControl *detectorControl() const;

signals:
    void detectorControlChanged(lmrs::core::DetectorControl *detectorControl);

private:
    void onDetectorInfoChanged(core::DetectorInfo info);

    QPointer<core::DetectorControl> m_control;
    QList<core::DetectorInfo> m_rows;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_DETECTORINFOTABLEMODEL_H
