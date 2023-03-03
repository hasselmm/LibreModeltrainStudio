#ifndef LMRS_STUDIO_ACCESSORYTABLEMODEL_H
#define LMRS_STUDIO_ACCESSORYTABLEMODEL_H

#include <lmrs/core/accessories.h>

#include <QAbstractTableModel>
#include <QPointer>

namespace lmrs::core {
class AccessoryControl;
}

namespace lmrs::studio {

class AccessoryTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum DataColumn {
        TypeColumn,
        AddressColumn,
        StateColumn,
    };

    Q_ENUM(DataColumn)

    enum class RowType {
        Invalid,
        Accessory,
        Turnout,
    };

    Q_ENUM(RowType)

    using QAbstractTableModel::QAbstractTableModel;

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setAccessoryControl(core::AccessoryControl *newControl);
    core::AccessoryControl *accessoryControl() const;

signals:
    void accessoryControlChanged(lmrs::core::AccessoryControl *accessoryControl);

private:
    struct Row
    {
        RowType type() const;
        QVariant address() const;
        QVariant state() const;

        std::variant<core::accessory::AccessoryInfo, core::accessory::TurnoutInfo> value;
    };

    void onAccessoryInfoChanged(core::accessory::AccessoryInfo info);
    void onTurnoutInfoChanged(core::accessory::TurnoutInfo info);
    void onRowChanged(Row row);

    QPointer<core::AccessoryControl> m_control;
    QList<Row> m_rows;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_ACCESSORYTABLEMODEL_H
