#ifndef LMRS_CORE_ACCESSORIES_H
#define LMRS_CORE_ACCESSORIES_H

#include "dccconstants.h"

namespace lmrs::core::accessory {

struct AccessoryInfo
{
    Q_GADGET
    Q_PROPERTY(lmrs::core::dcc::AccessoryAddress address READ address CONSTANT FINAL)
    Q_PROPERTY(quint8 state READ state CONSTANT FINAL) // FIXME: How to have dcc::AccessoryState as type?

public:
    enum class CommonSignal {
        Halt            = 0,
        Go              = 0x10,
        StopEmred       = 0x40,
        ShuntGo         = 0x41,
        SignalDark      = 0x42,
        Kennlicht       = 0x43,
        Joker           = 0x44,
        Substitution    = 0x45,
        Counterline     = 0x46,
        Cautiousness    = 0x47,
    };

    Q_ENUM(CommonSignal)

    constexpr AccessoryInfo(dcc::AccessoryAddress address, dcc::AccessoryState state) noexcept
        : m_address(address)
        , m_state(state)
    {}

    constexpr auto address() const noexcept { return m_address; }
    constexpr auto state() const noexcept { return m_state; }

private:
    dcc::AccessoryAddress m_address;
    dcc::AccessoryState m_state;
};

struct TurnoutInfo
{
    Q_GADGET
    Q_PROPERTY(lmrs::core::dcc::AccessoryAddress address READ address CONSTANT FINAL)
    Q_PROPERTY(lmrs::core::dcc::TurnoutState state READ state CONSTANT FINAL)

public:
    constexpr TurnoutInfo(dcc::AccessoryAddress address, dcc::TurnoutState state) noexcept
        : m_address(address)
        , m_state(state)
    {}

    constexpr auto address() const noexcept { return m_address; }
    constexpr auto state() const noexcept { return m_state; }

private:
    dcc::AccessoryAddress m_address;
    dcc::TurnoutState m_state;
};

QDebug operator<<(QDebug debug, const AccessoryInfo &info);
QDebug operator<<(QDebug debug, const TurnoutInfo &info);

} // namespace lmrs::core::accessory

#endif // LMRS_CORE_ACCESSORIES_H
