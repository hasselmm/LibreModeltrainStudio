#ifndef LMRS_CORE_VEHICLEINFOMODEL_H
#define LMRS_CORE_VEHICLEINFOMODEL_H

#include "device.h"

#include <QAbstractTableModel>

namespace lmrs::core {

class VehicleInfoModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Role {
        DataRole = Qt::UserRole,
        VehicleInfoRole,
    };

    Q_ENUM(Role)

    enum class Column {
        Address,
        Name,
        Direction,
        Speed,
        Functions,
        Flags,
    };

    Q_ENUM(Column)

    using QAbstractTableModel::QAbstractTableModel;

    void clear();

    int updateVehicleInfo(VehicleInfo info);
    int updateVehicleName(dcc::VehicleAddress address, QString name);

    QModelIndex findVehicle(dcc::VehicleAddress address) const;

public: // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent = {}) const;
    int columnCount(const QModelIndex &parent = {}) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

private:
    struct Row {
        Row(VehicleInfo info, QString name = {}) noexcept
            : info{std::move(info)}
            , name{std::move(name)}
        {}

        VehicleInfo info;
        QString name;
    };

    QList<Row> m_rows;
};

} // namespace lmrs::core

#endif // LMRS_CORE_VEHICLEINFOMODEL_H
