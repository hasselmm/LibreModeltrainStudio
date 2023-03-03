#include "accessorytablemodel.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>

namespace lmrs::studio {

namespace {

using namespace core::accessory;
namespace dcc = core::dcc;

QString displayString(AccessoryTableModel::RowType type)
{
    switch (type) {
    case AccessoryTableModel::RowType::Accessory:
        return AccessoryTableModel::tr("Accessory");
    case AccessoryTableModel::RowType::Turnout:
        return AccessoryTableModel::tr("Turnout");
    case AccessoryTableModel::RowType::Invalid:
        break;
    }

    return {};
}

QString stateName(QVariant state)
{
    if (state.typeId() == qMetaTypeId<dcc::AccessoryState>()) {
        return QString::number(qvariant_cast<dcc::AccessoryState>(state));
    } else if (state.typeId() == qMetaTypeId<dcc::TurnoutState>()) {
        switch (qvariant_cast<dcc::TurnoutState>(state)) {
        case dcc::TurnoutState::Straight:
            return AccessoryTableModel::tr("straight/green");
        case dcc::TurnoutState::Branched:
            return AccessoryTableModel::tr("branched/red");
        case dcc::TurnoutState::Unknown:
            return AccessoryTableModel::tr("unknown");
        case dcc::TurnoutState::Invalid:
            break;
        }
    }

    qInfo() << state;

    return {};
}

}

int AccessoryTableModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_rows.count());
}

int AccessoryTableModel::columnCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return core::keyCount<DataColumn>();
}

QVariant AccessoryTableModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (static_cast<DataColumn>(index.column())) {
        case TypeColumn:
            if (role == Qt::DisplayRole)
                return displayString(m_rows[index.row()].type());
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
            else if (role == Qt::UserRole)
                return QVariant::fromValue(m_rows[index.row()].type());

            break;

        case AddressColumn:
            if (role == Qt::DisplayRole)
                return m_rows[index.row()].address().toString();
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);
            else if (role == Qt::UserRole)
                return m_rows[index.row()].address();

            break;

        case StateColumn:
            if (role == Qt::DisplayRole)
                return stateName(m_rows[index.row()].state());
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
            else if (role == Qt::UserRole)
                return m_rows[index.row()].state();

            break;
        }
    }

    return {};
}

QVariant AccessoryTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<DataColumn>(section)) {
        case TypeColumn:
            return tr("Type");
        case AddressColumn:
            return tr("Address");
        case StateColumn:
            return tr("State");
        }
    }

    return {};
}

void AccessoryTableModel::setAccessoryControl(core::AccessoryControl *newControl)
{
    if (const auto oldControl = std::exchange(m_control, newControl); oldControl != newControl) {
        if (oldControl)
            oldControl->disconnect(this);

        if (newControl) {
            connect(newControl, &core::AccessoryControl::accessoryInfoChanged,
                    this, &AccessoryTableModel::onAccessoryInfoChanged);
            connect(newControl, &core::AccessoryControl::turnoutInfoChanged,
                    this, &AccessoryTableModel::onTurnoutInfoChanged);
        }

        emit accessoryControlChanged(newControl);
    }
}

core::AccessoryControl *AccessoryTableModel::accessoryControl() const
{
    return m_control;
}

void AccessoryTableModel::onAccessoryInfoChanged(AccessoryInfo info)
{
    onRowChanged(Row{info});
}

void AccessoryTableModel::onTurnoutInfoChanged(TurnoutInfo info)
{
    onRowChanged(Row{info});
}

void AccessoryTableModel::onRowChanged(Row row)
{
    const auto it = std::find_if(m_rows.begin(), m_rows.end(), [newRow = row](const Row &oldRow) {
        if (const auto typeOrder = core::value(oldRow.type()) <=> core::value(newRow.type());
                typeOrder != std::strong_ordering::equal)
            return typeOrder == std::strong_ordering::greater;

        return oldRow.address().toInt() >= newRow.address().toInt();
    });

    const auto rowNumber = static_cast<int>(it - m_rows.begin());

    if (it == m_rows.end()
            || row.type() != it->type()
            || row.address() != it->address()) {
        beginInsertRows({}, rowNumber, rowNumber);
        m_rows.insert(it, std::move(row));
        endInsertRows();
    } else {
        *it = std::move(row);
        emit dataChanged(index(rowNumber, 0), index(rowNumber, core::keyCount<DataColumn>() - 1));
    }
}

AccessoryTableModel::RowType AccessoryTableModel::Row::type() const
{
    if (std::holds_alternative<AccessoryInfo>(value))
        return RowType::Accessory;
    else if (std::holds_alternative<TurnoutInfo>(value))
        return RowType::Turnout;
    else
        return RowType::Invalid;
}

QVariant AccessoryTableModel::Row::address() const
{
    if (const auto info = std::get_if<AccessoryInfo>(&value))
        return core::value(info->address());
    if (const auto info = std::get_if<TurnoutInfo>(&value))
        return core::value(info->address());
    else
        return {};
}

QVariant AccessoryTableModel::Row::state() const
{
    if (const auto info = std::get_if<AccessoryInfo>(&value))
        return QVariant::fromValue(info->state());
    if (const auto info = std::get_if<TurnoutInfo>(&value))
        return QVariant::fromValue(info->state());
    else
        return {};
}

} // namespace lmrs::studio
