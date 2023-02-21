#ifndef LMRS_CORE_PROPERTYGUARD_H
#define LMRS_CORE_PROPERTYGUARD_H

#include <QObject>

namespace lmrs::core {

template <typename Value>
class PropertyGuard
{
public:
    using Getter = std::function<Value()>;
    using Emitter = std::function<void(Value)>;

    explicit PropertyGuard(Getter getter, Emitter emitter)
        : m_getter{getter}
        , m_emitter{emitter}
        , m_initialValue{getter()}
    {}

    ~PropertyGuard()
    {
        if (m_emitter) {
            if (const auto newValue = changedValue())
                m_emitter(newValue.value());
        }
    }

    /// This allows to conveniently use in an if() statement expression to limit the guard's scope.
    constexpr operator bool() const noexcept { return true; }

    /// Verify if the observed property has changed.
    bool hasChanged() const { return m_getter() != m_initialValue; }

    /// The initial value we compare with.
    Value initialValue() const { return m_initialValue; }

    /// Verify if the observed property has changed, and return the new value if that's the case.
    std::optional<Value> changedValue() const
    {
        if (auto currentValue = m_getter(); currentValue != m_initialValue)
            return currentValue;

        return {};
    }

private:
    const Getter m_getter;
    const Emitter m_emitter;
    const Value m_initialValue;
};

template <class Target, typename Value, typename GetterTarget = Target>
auto propertyGuard(Target *target, Value (GetterTarget::*getter)() const)
{
    return PropertyGuard<Value> {
        [target, getter] { return (target->*getter)(); },
        {}, // still useful manual change checks
    };
}

template <class Target, typename Value, typename GetterTarget = Target>
auto propertyGuard(Target *target, Value (GetterTarget::*getter)() const,
                   typename PropertyGuard<Value>::Emitter emitter)
{
    return PropertyGuard<Value> {
        [target, getter] { return (target->*getter)(); },
        std::move(emitter)
    };
}

template <class Target, typename Value, typename GetterTarget = Target, typename EmitterTarget = Target>
auto propertyGuard(Target *target, Value (GetterTarget::*getter)() const, void (EmitterTarget::*emitter)())
{
    return PropertyGuard<Value> {
        [target, getter] { return (target->*getter)(); },
        [target, emitter](auto) { Q_EMIT (target->*emitter)(); }
    };
}

template <class Target, typename Value, typename GetterTarget = Target, typename EmitterTarget = Target>
auto propertyGuard(Target *target, Value (GetterTarget::*getter)() const, void (EmitterTarget::*emitter)(Value))
{
    return PropertyGuard<Value> {
        [target, getter] { return (target->*getter)(); },
        [target, emitter](auto value) { Q_EMIT (target->*emitter)(std::move(value)); }
    };
}

template <class Target, typename Value, typename PS, typename GetterTarget = Target, typename EmitterTarget = Target>
auto propertyGuard(Target *target, Value (GetterTarget::*getter)() const,
                   void (EmitterTarget::*emitter)(Value, PS))
{
    return PropertyGuard<Value> {
        [target, getter] { return (target->*getter)(); },
        [target, emitter](auto value) { Q_EMIT (target->*emitter)(std::move(value), {}); }
    };
}

} // namespace lmrs::core

#endif // LMRS_CORE_PROPERTYGUARD_H
