#ifndef LMRS_CORE_VEHICLEINFOMODEL_H
#define LMRS_CORE_VEHICLEINFOMODEL_H

#include "device.h"

#include <QAbstractTableModel>

namespace lmrs::core {

struct VehicleInfo
{
    Q_GADGET

public:
    enum class Flag {
        IsClaimed = (1 << 0),
        IsConsist = (1 << 1),
    };

    Q_FLAG(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    VehicleInfo() = default;

    constexpr explicit VehicleInfo(dcc::VehicleAddress address, dcc::Direction direction, dcc::Speed speed,
                                   dcc::FunctionState functionState = {}, Flags flags = {}) noexcept
        : m_address{address}, m_direction{direction}, m_speed{std::move(speed)}
        , m_functionState{std::move(functionState)}, m_flags{flags}
    {}

    constexpr auto address() const noexcept { return m_address; }
    constexpr auto direction() const noexcept { return m_direction; }
    constexpr auto speed() const noexcept { return m_speed; }
    constexpr auto flags() const noexcept { return Flags{m_flags}; }

    constexpr bool functionState(dcc::Function function) const noexcept { return m_functionState[function]; }
    constexpr auto functionState() const noexcept { return m_functionState; }

private:
    dcc::VehicleAddress m_address = 0;
    dcc::Direction m_direction = dcc::Direction::Forward;
    dcc::Speed m_speed;
    dcc::FunctionState m_functionState;
    Flags::Int m_flags;
};

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

QDebug operator<<(QDebug debug, const VehicleInfo &info);

} // namespace lmrs::core

#endif // LMRS_CORE_VEHICLEINFOMODEL_H
