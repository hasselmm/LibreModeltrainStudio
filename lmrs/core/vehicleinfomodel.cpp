#include "vehicleinfomodel.h"

#include "algorithms.h"
#include "userliterals.h"

namespace lmrs::core {

namespace {

// FIXME: move to dccconstants?
QString toString(dcc::Direction direction)
{
    switch(direction) {
    case dcc::Direction::Forward:
        return VehicleInfoModel::tr("Forward");
    case dcc::Direction::Reverse:
        return VehicleInfoModel::tr("Reverse");
    case dcc::Direction::Unknown:
        return VehicleInfoModel::tr("Unknown");
    }

    return QString::number(value(direction));
}

// FIXME: move to dccconstants?
QString toString(dcc::FunctionState functions)
{
    QString text;

    for (size_t i = 0; i < functions.size(); ++i) {
        if (functions[i]) {
            if (!text.isEmpty())
                text += ' '_L1;

            text += 'F'_L1 + QString::number(i);
        }
    }

    return text;
}

} // namespace

void VehicleInfoModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

int VehicleInfoModel::updateVehicleInfo(VehicleInfo info)
{
    const auto it = std::find_if(m_rows.begin(), m_rows.end(), [address = info.address()](const auto &row) {
        return row.info.address() >= address;
    });

    const auto row = static_cast<int>(it - m_rows.begin());

    if (it != m_rows.end() && it->info.address() == info.address()) {
        std::swap(it->info, info);
        emit dataChanged(index(row, 0), index(row, columnCount() - 1));
    } else {
        beginInsertRows({}, row, row);
        m_rows.emplace(it, std::move(info));
        endInsertRows();
    }

    return row;
}

int VehicleInfoModel::updateVehicleName(dcc::VehicleAddress address, QString name)
{
    const auto it = std::find_if(m_rows.begin(), m_rows.end(), [address](const auto &row) {
        return row.info.address() >= address;
    });

    const auto row = static_cast<int>(it - m_rows.begin());

    if (it != m_rows.end() && it->info.address() == address) {
        std::swap(it->name, name);
        emit dataChanged(index(row, 0), index(row, columnCount() - 1));
    } else {
        beginInsertRows({}, row, row);
        m_rows.emplace(it, VehicleInfo{address, dcc::Direction::Forward, {}}, std::move(name));
        endInsertRows();
    }

    return row;
}

QModelIndex VehicleInfoModel::findVehicle(dcc::VehicleAddress address) const
{
    const auto it = std::find_if(m_rows.begin(), m_rows.end(), [address](const auto &row) {
        return row.info.address() == address;
    });

    if (Q_UNLIKELY(it == m_rows.end()))
        return {};

    return index(static_cast<int>(it - m_rows.begin()), 0);
}

int VehicleInfoModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_rows.size());
}

int VehicleInfoModel::columnCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return keyCount<Column>();
}

QVariant VehicleInfoModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        if (role == VehicleInfoRole)
            return QVariant::fromValue(m_rows[index.row()].info);

        switch (static_cast<Column>(index.column())) {
        case Column::Address:
            if (role == Qt::DisplayRole)
                return QString::number(m_rows[index.row()].info.address());
            else if (role == Qt::UserRole)
                return QVariant::fromValue(m_rows[index.row()].info.address());
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

            break;

        case Column::Name:
            if (role == Qt::DisplayRole || role == Qt::UserRole)
                return m_rows[index.row()].name;

            break;

        case Column::Speed:
            if (role == Qt::DisplayRole) {
                if (const auto speed = m_rows[index.row()].info.speed(); isValid(speed)) {
                    const auto percentilSpeed = speedCast<dcc::SpeedPercentil>(speed);
                    return QString::number(percentilSpeed.count()) + u"\u202f%"_qs;
                }
            } else if (role == Qt::ToolTipRole) {
                if (const auto speed = m_rows[index.row()].info.speed(); isValid(speed)) {
                    if (std::holds_alternative<dcc::Speed126>(speed))
                        return QString::number(std::get<dcc::Speed126>(speed).count()) + "/126"_L1;
                    else if (std::holds_alternative<dcc::Speed28>(speed))
                        return QString::number(std::get<dcc::Speed28>(speed).count()) + "/28"_L1;
                    else if (std::holds_alternative<dcc::Speed14>(speed))
                        return QString::number(std::get<dcc::Speed14>(speed).count()) + "/14"_L1;
                }
            } else if (role == Qt::UserRole) {
                return QVariant::fromValue(m_rows[index.row()].info.speed());
            } else if (role == Qt::TextAlignmentRole) {
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            }

            break;

        case Column::Direction:
            if (role == Qt::DisplayRole) {
                if (const auto info = m_rows[index.row()].info; isValid(info.speed()))
                    return toString(info.direction());
            } else if (role == Qt::UserRole) {
                return QVariant::fromValue(m_rows[index.row()].info.direction());
            } else if (role == Qt::TextAlignmentRole) {
                return static_cast<int>(Qt::AlignHCenter | Qt::AlignVCenter);
            }

            break;

        case Column::Functions:
            if (role == Qt::DisplayRole)
                return toString(m_rows[index.row()].info.functionState());
            else if (role == Qt::UserRole)
                return QVariant::fromValue(m_rows[index.row()].info.functionState());

            break;

        case Column::Flags:
            break;
        }
    }

    return {};
}

QVariant VehicleInfoModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<Column>(section)) {
        case Column::Address:
            return tr("Address");
        case Column::Name:
            return tr("Name");
        case Column::Speed:
            return tr("Speed");
        case Column::Direction:
            return tr("Direction");
        case Column::Functions:
            return tr("Functions");
        case Column::Flags:
            return tr("Flags");
        }
    }

    return {};
}

} // namespace lmrs::core
