#include "detectorinfotablemodel.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>

namespace lmrs::studio {

namespace {

namespace rm = core::rm;

QString displayString(rm::DetectorAddress::Type type)
{
    switch (type) {
    case rm::DetectorAddress::Type::CanModule:
    case rm::DetectorAddress::Type::CanNetwork:
    case rm::DetectorAddress::Type::CanPort:
        return DetectorInfoTableModel::tr("CAN");

    case rm::DetectorAddress::Type::RBusGroup:
    case rm::DetectorAddress::Type::RBusModule:
    case rm::DetectorAddress::Type::RBusPort:
        return DetectorInfoTableModel::tr("RBus");

    case rm::DetectorAddress::Type::LoconetSIC:
    case rm::DetectorAddress::Type::LoconetModule:
        return DetectorInfoTableModel::tr("LocoNet");

    case rm::DetectorAddress::Type::LissyModule:
        return DetectorInfoTableModel::tr("Lissy");

    case rm::DetectorAddress::Type::Invalid:
        break;
    }

    return {};
}

QString displayString(core::DetectorInfo::Occupancy occupancy)
{
    switch (occupancy) {
    case core::DetectorInfo::Occupancy::Free:
        return DetectorInfoTableModel::tr("free");
    case core::DetectorInfo::Occupancy::Occupied:
        return DetectorInfoTableModel::tr("occupied");
    case core::DetectorInfo::Occupancy::Unknown:
        return DetectorInfoTableModel::tr("unknown");
    case core::DetectorInfo::Occupancy::Invalid:
        break;
    }

    return {};
}

QString displayString(core::DetectorInfo::PowerState powerState)
{
    switch (powerState) {
    case core::DetectorInfo::PowerState::Off:
        return DetectorInfoTableModel::tr("off");
    case core::DetectorInfo::PowerState::On:
        return DetectorInfoTableModel::tr("on");
    case core::DetectorInfo::PowerState::Overload:
        return DetectorInfoTableModel::tr("overload");
    case core::DetectorInfo::PowerState::Unknown:
        return DetectorInfoTableModel::tr("unknown");
    }

    return {};
}

QString moduleName(const core::rm::DetectorAddress &address)
{
    switch (address.type()) {
    case rm::DetectorAddress::Type::CanModule:
    case rm::DetectorAddress::Type::CanPort:
        return "0x"_L1 + QString::number(address.canNetwork(), 16) + '/'_L1
                + QString::number(address.canModule());
    case rm::DetectorAddress::Type::CanNetwork:
        return "0x"_L1 + QString::number(address.canNetwork(), 16);

    case rm::DetectorAddress::Type::RBusGroup:
        return DetectorInfoTableModel::tr("Group #%1").arg(address.rbusGroup());
    case rm::DetectorAddress::Type::RBusModule:
    case rm::DetectorAddress::Type::RBusPort:
        return DetectorInfoTableModel::tr("Module #%1").arg(address.rbusModule());

    case rm::DetectorAddress::Type::LoconetSIC:
        return DetectorInfoTableModel::tr("SIC");
    case rm::DetectorAddress::Type::LoconetModule:
        return QString::number(address.loconetModule());
    case rm::DetectorAddress::Type::LissyModule:
        return QString::number(address.lissyModule());

    case rm::DetectorAddress::Type::Invalid:
        break;
    }

    return {};
}

QVariant portNumber(const core::rm::DetectorAddress &address)
{
    switch (address.type()) {
    case rm::DetectorAddress::Type::CanPort:
        return address.canPort().value;
    case rm::DetectorAddress::Type::RBusPort:
        return address.rbusPort().value;

    case rm::DetectorAddress::Type::CanModule:
    case rm::DetectorAddress::Type::CanNetwork:
    case rm::DetectorAddress::Type::LissyModule:
    case rm::DetectorAddress::Type::LoconetSIC:
    case rm::DetectorAddress::Type::LoconetModule:
    case rm::DetectorAddress::Type::RBusGroup:
    case rm::DetectorAddress::Type::RBusModule:
    case rm::DetectorAddress::Type::Invalid:
        break;
    }

    return {};
}

QString portName(const core::rm::DetectorAddress &address)
{
    if (const auto port = portNumber(address); !port.isNull())
        return QString::number(port.toInt());

    return {};
}

QString vehicleDescription(const core::DetectorInfo &info)
{
    auto result = QStringList{};

    const auto vehicles = info.vehicles();
    const auto directions = info.directions();

    for (auto i = 0; i < vehicles.count(); ++i) {
        if (const auto address = vehicles[i]) {
            result += QString::number(address);

            if (i < directions.count()) {
                switch (directions[i]) {
                case core::dcc::Direction::Forward:
                    result.back() += QChar{0x21D2};
                    break;

                case core::dcc::Direction::Reverse:
                    result.back() += QChar{0x21D0};
                    break;

                case core::dcc::Direction::Unknown:
                    break;
                }
            }
        }
    }

    return result.join(", "_L1);
}

} // namespace

int DetectorInfoTableModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_rows.count());
}

int DetectorInfoTableModel::columnCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return core::keyCount<DataColumn>();
}

QVariant DetectorInfoTableModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (static_cast<DataColumn>(index.column())) {
        case TypeColumn:
            if (role == Qt::DisplayRole)
                return displayString(m_rows[index.row()].address().type());
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignHCenter);
            else if (role == Qt::UserRole)
                return QVariant::fromValue(m_rows[index.row()].address().type());

            break;

        case ModuleColumn:
            if (role == Qt::DisplayRole)
                return moduleName(m_rows[index.row()].address());
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);
            else if (role == Qt::UserRole)
                return QVariant::fromValue(m_rows[index.row()].address());

            break;

        case PortColumn:
            if (role == Qt::DisplayRole)
                return portName(m_rows[index.row()].address());
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);
            else if (role == Qt::UserRole)
                return portNumber(m_rows[index.row()].address());

            break;

        case OccupancyColumn:
            if (role == Qt::DisplayRole)
                return displayString(m_rows[index.row()].occupancy());
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignHCenter);
            else if (role == Qt::UserRole)
                return QVariant::fromValue(m_rows[index.row()].occupancy());

            break;

        case PowerStateColumn:
            if (role == Qt::DisplayRole)
                return displayString(m_rows[index.row()].powerState());
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignHCenter);
            else if (role == Qt::UserRole)
                return QVariant::fromValue(m_rows[index.row()].powerState());

            break;

        case VehiclesColumn:
            if (role == Qt::DisplayRole)
                return vehicleDescription(m_rows[index.row()]);
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);

            break;
        }
    }

    return {};
}

QVariant DetectorInfoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<DataColumn>(section)) {
        case TypeColumn:
            return tr("Type");
        case ModuleColumn:
            return tr("Module");
        case PortColumn:
            return tr("Port");
        case OccupancyColumn:
            return tr("Occupancy");
        case PowerStateColumn:
            return tr("Power");
        case VehiclesColumn:
            return tr("Vehicles");
        }
    }

    return {};
}

void DetectorInfoTableModel::setDetectorControl(core::DetectorControl *newControl)
{
    if (const auto oldControl = std::exchange(m_control, newControl); oldControl != newControl) {
        if (oldControl)
            oldControl->disconnect(this);

        if (newControl) {
            connect(newControl, &core::DetectorControl::detectorInfoChanged,
                    this, &DetectorInfoTableModel::onDetectorInfoChanged);
        }

        emit detectorControlChanged(newControl);
    }
}

core::DetectorControl *DetectorInfoTableModel::detectorControl() const
{
    return m_control;
}

void DetectorInfoTableModel::onDetectorInfoChanged(core::DetectorInfo info)
{
    const auto it = std::find_if(m_rows.begin(), m_rows.end(), [info](const core::DetectorInfo &row) {
        return info.address() == row.address();
    });

    if (it != m_rows.end()) {
        *it = std::move(info);
        const auto row = static_cast<int>(it - m_rows.begin());
        emit dataChanged(index(row, 0), index(row, core::keyCount<DataColumn>() - 1));
    } else {
        const auto row = static_cast<int>(m_rows.count());
        beginInsertRows({}, row, row);
        m_rows.append(std::move(info));
        endInsertRows();
    }
}

} // namespace lmrs::studio
