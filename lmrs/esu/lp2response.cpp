#include "lp2response.h"

#include <lmrs/esu/lp2request.h>

#include <QVariant>

#include <QDateTime>
#include <QVersionNumber>
#include <QtEndian>

namespace lmrs::esu::lp2 {

template <typename T> constexpr T s_invalidValue;
template<> quint32 s_invalidValue<quint32> = 0xffffffff;

Response::Status InterfaceFlagsResponse::status() const
{
    return static_cast<Response::Status>(data[0]);
}

Response::Status ValueResponse::status() const
{
    return static_cast<Response::Status>(data[0]);
}

template<>
quint8 ValueResponse::value() const
{
    if (data.size() >= 2)
        return static_cast<quint8>(data[1]);

    return {};
}

template<>
quint16 ValueResponse::value() const
{
    if (data.size() >= 3)
        return qFromLittleEndian<quint16>(data.constData() + 1);

    return {};
}

template<>
quint32 ValueResponse::value() const
{
    if (data.size() >= 5)
        return qFromLittleEndian<quint32>(data.constData() + 1);

    return {};
}

template<>
QVersionNumber ValueResponse::value() const
{
    if (data.size() >= 5) {
        if (const auto versionCode = value<quint32>(); versionCode != s_invalidValue<quint32>) {
            const auto major = static_cast<int>(versionCode >> 24) & 0xff;
            const auto minor = static_cast<int>(versionCode >> 16) & 0xff;
            const auto build = static_cast<int>(versionCode >>  0) & 0xffff;
            return {major, minor, build};
        }
    }

    return {};
}

template<>
QDateTime ValueResponse::value() const
{
    if (data.size() >= 5) {
        if (const auto timestamp = value<quint32>(); timestamp != s_invalidValue<quint32>) {
            static const auto epoch = QDateTime{{2000, 1, 1}, {}, Qt::UTC}.toSecsSinceEpoch();
            return QDateTime::fromSecsSinceEpoch(timestamp + epoch);
        }
    }

    return {};
}

template<>
InterfaceApplicationTypes ValueResponse::value() const
{
    return static_cast<InterfaceApplicationTypes>(value<quint32>());
}

template<>
QVariant ValueResponse::value() const
{
    if (data.size() >= 5)
        return value<quint32>();
    else if (data.size() >= 3)
        return value<quint16>();
    else if (data.size() >= 2)
        return value<quint8>();

    return {};
}

template<>
QVariant InterfaceInfoResponse::value() const
{
    switch (info) {
    case InterfaceInfo::ManufacturerId:
    case InterfaceInfo::ProductId:
        return ValueResponse::value<quint32>();

    case InterfaceInfo::SerialNumber:
        if (const auto value = ValueResponse::value<quint32>(); value != s_invalidValue<quint32>)
            return value;

        break;

    case InterfaceInfo::BootloaderCode:
    case InterfaceInfo::ApplicationCode:
        if (const auto version = ValueResponse::value<QVersionNumber>(); version.segmentCount() > 0)
            return QVariant::fromValue(version);

        break;

    case InterfaceInfo::ProductionDate:
    case InterfaceInfo::BootloaderDate:
    case InterfaceInfo::ApplicationDate:
        if (const auto dateTime = ValueResponse::value<QDateTime>(); dateTime.isValid())
            return QVariant::fromValue(dateTime);

        break;

    case InterfaceInfo::ApplicationType:
        return QVariant::fromValue(ValueResponse::value<InterfaceApplicationTypes>());
    }

    return {};
}

Response::Status DccResponse::status() const
{
    return static_cast<Response::Status>(data[0]);
}

DccResponse::Acknowledge DccResponse::acknowledge() const
{
    if (data.size() < 2)
        return Acknowledge::None;
    else if (data[1] != 0)
        return Acknowledge::Positive;
    else
        return Acknowledge::Negative;
}

QDebug operator<<(QDebug debug, InterfaceFlagsResponse response)
{
    const auto state = QDebugStateSaver{debug};

    if (debug.verbosity() > 0)
        debug.nospace() << "DeviceFlagsResponse";

    debug.setVerbosity(0);

    return debug.nospace() << '(' << response.status() << ')';
}

QDebug operator<<(QDebug debug, ValueResponse response)
{
    const auto state = QDebugStateSaver{debug};

    if (debug.verbosity() > 0)
        debug.nospace() << "ValueResponse";

    debug.setVerbosity(0);

    if (const auto status = response.status(); status != Response::Status::Success)
        return debug.nospace() << '(' << status << ')';

    debug.nospace() << '(';

    auto value = response.value();

    if (value.typeId() == QMetaType::UInt)
        debug << ", " << value.toUInt();
    else if (value.isValid())
        debug << ", " << std::move(value);

    return debug << ')';
}

QDebug operator<<(QDebug debug, InterfaceInfoResponse response)
{
    const auto state = QDebugStateSaver{debug};

    if (debug.verbosity() > 0)
        debug.nospace() << "InterfaceInfoResponse";

    debug.setVerbosity(0);

    if (const auto status = response.status(); status != Response::Status::Success)
        return debug.nospace() << '(' << status << ')';

    debug.nospace() << "(" << response.info;


    auto responseData = response.value();

    if (responseData.typeId() == QMetaType::UInt)
        debug << ", " << responseData.toUInt();
    else if (responseData.typeId() == qMetaTypeId<InterfaceApplicationTypes>())
        debug << ", " << responseData.value<InterfaceApplicationTypes>();
    else if (responseData.typeId() == qMetaTypeId<QDateTime>())
        debug << ", " << responseData.value<QDateTime>().toString(Qt::ISODate);
    else if (responseData.typeId() == qMetaTypeId<QVersionNumber>())
        debug << ", " << responseData.value<QVersionNumber>();
    else if (responseData.isValid())
        debug << ", " << std::move(responseData);

    return debug << ')';
}

QDebug operator<<(QDebug debug, DccResponse response)
{
    const auto state = QDebugStateSaver{debug};

    if (debug.verbosity() > 0)
        debug.nospace() << "DccResponse";

    debug.setVerbosity(0);

    if (const auto status = response.status(); status != Response::Status::Success)
        return debug.nospace() << '(' << status << ')';

    return debug.nospace() << '(' << response.acknowledge() << ')';
}

} // namespace lmrs::esu::lp2
