#include "detectorinfotablemodel.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>

namespace lmrs::studio {

namespace {

using namespace core::accessory;

QString displayString(DetectorAddress::Type type)
{
    switch (type) {
    case DetectorAddress::Type::CanModule:
    case DetectorAddress::Type::CanNetwork:
    case DetectorAddress::Type::CanPort:
        return DetectorInfoTableModel::tr("CAN");

    case DetectorAddress::Type::RBusGroup:
    case DetectorAddress::Type::RBusModule:
    case DetectorAddress::Type::RBusPort:
        return DetectorInfoTableModel::tr("RBus");

    case DetectorAddress::Type::LoconetSIC:
    case DetectorAddress::Type::LoconetModule:
        return DetectorInfoTableModel::tr("LocoNet");

    case DetectorAddress::Type::LissyModule:
        return DetectorInfoTableModel::tr("Lissy");

    case DetectorAddress::Type::Invalid:
        break;
    }

    return {};
}

QString displayString(DetectorInfo::Occupancy occupancy)
{
    switch (occupancy) {
    case DetectorInfo::Occupancy::Free:
        return DetectorInfoTableModel::tr("free");
    case DetectorInfo::Occupancy::Occupied:
        return DetectorInfoTableModel::tr("occupied");
    case DetectorInfo::Occupancy::Unknown:
        return DetectorInfoTableModel::tr("unknown");
    case DetectorInfo::Occupancy::Invalid:
        break;
    }

    return {};
}

QString displayString(DetectorInfo::PowerState powerState)
{
    switch (powerState) {
    case DetectorInfo::PowerState::Off:
        return DetectorInfoTableModel::tr("off");
    case DetectorInfo::PowerState::On:
        return DetectorInfoTableModel::tr("on");
    case DetectorInfo::PowerState::Overload:
        return DetectorInfoTableModel::tr("overload");
    case DetectorInfo::PowerState::Unknown:
        return DetectorInfoTableModel::tr("unknown");
    }

    return {};
}

QString moduleName(const DetectorAddress &address)
{
    switch (address.type()) {
    case DetectorAddress::Type::CanModule:
    case DetectorAddress::Type::CanPort:
        return "0x"_L1 + QString::number(address.canNetwork(), 16) + '/'_L1
                + QString::number(address.canModule());
    case DetectorAddress::Type::CanNetwork:
        return "0x"_L1 + QString::number(address.canNetwork(), 16);

    case DetectorAddress::Type::RBusGroup:
        return DetectorInfoTableModel::tr("Group #%1").arg(address.rbusGroup());
    case DetectorAddress::Type::RBusModule:
    case DetectorAddress::Type::RBusPort:
        return DetectorInfoTableModel::tr("Module #%1").arg(address.rbusModule());

    case DetectorAddress::Type::LoconetSIC:
        return DetectorInfoTableModel::tr("SIC");
    case DetectorAddress::Type::LoconetModule:
        return QString::number(address.loconetModule());
    case DetectorAddress::Type::LissyModule:
        return QString::number(address.lissyModule());

    case DetectorAddress::Type::Invalid:
        break;
    }

    return {};
}

QVariant portNumber(const DetectorAddress &address)
{
    switch (address.type()) {
    case DetectorAddress::Type::CanPort:
        return address.canPort().value;
    case DetectorAddress::Type::RBusPort:
        return address.rbusPort().value;

    case DetectorAddress::Type::CanModule:
    case DetectorAddress::Type::CanNetwork:
    case DetectorAddress::Type::LissyModule:
    case DetectorAddress::Type::LoconetSIC:
    case DetectorAddress::Type::LoconetModule:
    case DetectorAddress::Type::RBusGroup:
    case DetectorAddress::Type::RBusModule:
    case DetectorAddress::Type::Invalid:
        break;
    }

    return {};
}

QString portName(const DetectorAddress &address)
{
    if (const auto port = portNumber(address); !port.isNull())
        return QString::number(port.toInt());

    return {};
}

QString vehicleDescription(const DetectorInfo &info)
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

void DetectorInfoTableModel::onDetectorInfoChanged(DetectorInfo info)
{
    const auto it = std::find_if(m_rows.begin(), m_rows.end(), [info](const DetectorInfo &row) {
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
