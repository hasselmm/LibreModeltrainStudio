#ifndef LMRS_CORE_QUANTITY_H
#define LMRS_CORE_QUANTITY_H

#include <QLocale>

namespace lmrs::core {

template <typename T, typename Unit, typename Ratio = std::ratio<1>>
class quantity
{
public:
    using ratio_type = Ratio;
    using unit_type = Unit;
    using value_type = T;

    constexpr quantity(T value = {}) : m_value{value} {}
    constexpr T count() const { return m_value; }
    constexpr operator T() const { return count(); }

private:
    T m_value;
};

struct DurationTag;

template <typename T, class Ratio = std::ratio<1>> using current      = quantity<T, struct CurrentTag,      Ratio>;
template <typename T, class Ratio = std::ratio<1>> using voltage      = quantity<T, struct VoltageTag,      Ratio>;
template <typename T, class Ratio = std::ratio<1>> using temperature  = quantity<T, struct TemperatureTag,  Ratio>;
template <typename T, class Ratio = std::ratio<1>> using distance     = quantity<T, struct DistanceTag,     Ratio>;
template <typename T, class Ratio = std::ratio<1>> using speed        = quantity<T, struct SpeedTag,        Ratio>;
template <typename T, class Ratio = std::ratio<1>> using acceleration = quantity<T, struct AccelerationTag, Ratio>;
template <typename T, class Ratio = std::ratio<1>> using frequency    = quantity<T, struct FrequencyTag,    Ratio>;

namespace internal {

template<typename T, typename U, class R1, class R2, class Unit>
constexpr quantity<T, Unit, R1> quantityCastHelper(quantity<U, Unit, R2> q)
{
    constexpr auto cden = std::gcd(R1::den, R2::den) * std::gcd(R1::num, R2::num);
    constexpr auto num = R1::den * R2::num / cden;
    constexpr auto den = R1::num * R2::den / cden;

    if constexpr (std::is_floating_point<T>::value) {
        return static_cast<T>(q.count()) * num / den;
    } else {
        return static_cast<T>((q.count() * num + den/2) / den);
    }
}

template<typename T1, typename U1, typename R1, typename T2, typename U2, typename R2>
constexpr auto commonValues(const quantity<T1, U1, R1> &q1, const quantity<T2, U2, R2> &q2)
{
    constexpr auto cnum = std::gcd(R1::num, R2::num);
    constexpr auto cden = std::gcd(R1::den, R2::den);
    const auto v1 = q1.count() * R1::num * cden / R1::den;
    const auto v2 = q2.count() * R2::num * cden / R2::den;
    return std::make_tuple(std::ratio<cnum, cden>{}, v1, v2);
}

constexpr auto coallesce(auto a, auto b)
{
    return a && *a ? a : b;
}

template<typename T1, typename U1, typename T2, typename U2, typename R> struct QuantityProductType;
template<typename T1, typename U1, typename T2, typename U2, typename R> struct QuantityQuotientType;

template<typename Q1, typename Q2, typename Operation>
struct ArithmeticEvaluationHelper
{
    using common_type = typename std::common_type<typename Q1::value_type, typename Q2::value_type>::type;
    using operation = common_type (*)(common_type, common_type);

    static constexpr auto eval(Q1 q1, Q2 q2)
    {
        const auto common = internal::commonValues(q1, q2);
        const auto c1 = static_cast<common_type>(std::get<1>(common));
        const auto c2 = static_cast<common_type>(std::get<2>(common));

        using ratio = typename std::tuple_element<0, decltype(common)>::type;
        const auto count = Operation::eval(c1, c2) * ratio::den / ratio::num;
        return Operation::template create<Q1, Q2, std::ratio<ratio::num, ratio::den>>(count);
    }
};

template<typename Operation>
constexpr auto evaluate(auto q1, auto q2)
{
    return ArithmeticEvaluationHelper<decltype(q1), decltype(q2), Operation>::eval(q1, q2);
}

struct TypePreservingOperation
{
    template<typename Q1, typename Q2, typename R>
    static constexpr auto create(auto result)
    {
        return quantity<decltype(result), typename Q1::unit_type, R>{result};
    }
};

struct Addition : public TypePreservingOperation
{
    static constexpr auto eval(auto lhs, auto rhs) { return lhs + rhs; }
};

struct Substraction : public TypePreservingOperation
{
    static constexpr auto eval(auto lhs, auto rhs) { return lhs - rhs; }
};

struct Multiplication
{
    static constexpr auto eval(auto lhs, auto rhs) { return lhs * rhs; }

    template<typename Q1, typename Q2, typename R>
    static constexpr auto create(auto result)
    {
        return typename QuantityProductType<
                typename Q1::value_type, typename Q1::unit_type,
                typename Q2::value_type, typename Q2::unit_type, R>::type{result};
    }
};

struct Division
{
    static constexpr auto eval(auto lhs, auto rhs) { return lhs / rhs; }

    template<typename Q1, typename Q2, typename R>
    static constexpr auto create(auto result)
    {
        return typename QuantityQuotientType<
                typename Q1::value_type, typename Q1::unit_type,
                typename Q2::value_type, typename Q2::unit_type, R>::type{result};
    }
};

} // namespace internal

template<typename T, typename U, class Unit, class R1, class R2>
constexpr bool operator ==(const quantity<T, Unit, R1> &q1, const quantity<U, Unit, R2> &q2)
{
    const auto common = internal::commonValues(q1, q2);
    return std::get<1>(common) == std::get<2>(common);
}

template<typename T, typename U, class Unit, class R1, class R2>
constexpr bool operator !=(const quantity<T, Unit, R1> &q1, const quantity<U, Unit, R2> &q2)
{
    return !operator==(q1, q2);
}

template<typename T, typename U, class Unit, class R1, class R2>
constexpr bool operator <(const quantity<T, Unit, R1> &q1, const quantity<U, Unit, R2> &q2)
{
    const auto common = internal::commonValues(q1, q2);
    return std::get<1>(common) < std::get<2>(common);
}

template<typename T, typename U, class Unit, class R1, class R2>
constexpr bool operator <=(const quantity<T, Unit, R1> &q1, const quantity<U, Unit, R2> &q2)
{
    const auto common = internal::commonValues(q1, q2);
    return std::get<1>(common) <= std::get<2>(common);
}

template<typename T, typename U, class Unit, class R1, class R2>
constexpr bool operator >=(const quantity<T, Unit, R1> &q1, const quantity<U, Unit, R2> &q2)
{
    const auto common = internal::commonValues(q1, q2);
    return std::get<1>(common) >= std::get<2>(common);
}

template<typename T, typename U, class Unit, class R1, class R2>
constexpr bool operator >(const quantity<T, Unit, R1> &q1, const quantity<U, Unit, R2> &q2)
{
    const auto common = internal::commonValues(q1, q2);
    return std::get<1>(common) > std::get<2>(common);
}

template<typename T1, typename R1, typename T2, typename R2, typename U>
constexpr auto operator +(quantity<T1, U, R1> q1, quantity<T2, U, R2> q2)
{
    return internal::evaluate<internal::Addition>(q1, q2);
}

template<typename T1, typename R1, typename T2, typename R2, typename U>
constexpr auto operator -(quantity<T1, U, R1> q1, quantity<T2, U, R2> q2)
{
    return internal::evaluate<internal::Substraction>(q1, q2);
}

template<typename T1, typename U1, typename R1, typename T2, typename U2, typename R2>
constexpr auto operator *(quantity<T1, U1, R1> q1, quantity<T2, U2, R2> q2)
{
    return internal::evaluate<internal::Multiplication>(q1, q2);
}

template<typename T1, typename U1, typename R1, typename T2, typename U2, typename R2>
constexpr auto operator /(quantity<T1, U1, R1> q1, quantity<T2, U2, R2> q2)
{
    return internal::evaluate<internal::Division>(q1, q2);
}

template<typename T1, typename R1, typename T2, typename R2, typename U>
constexpr auto &operator +=(quantity<T1, U, R1> &q1, quantity<T2, U, R2> q2)
{
    return q1 = quantityCast<quantity<T1, U, R1>>(q1 + q2);
}

template<typename T1, typename R1, typename T2, typename R2, typename U>
constexpr auto &operator -=(quantity<T1, U, R1> &q1, quantity<T2, U, R2> q2)
{
    return q1 = quantityCast<quantity<T1, U, R1>>(q1 - q2);
}

template<typename TQ, typename UQ, typename RQ, typename TD, typename RD>
constexpr auto operator *(quantity<TQ, UQ, RQ> q, std::chrono::duration<TD, RD> d)
{
    return q * quantity<TD, DurationTag, RD>{d.count()};
}

template<typename TQ, typename UQ, typename RQ, typename TD, typename RD>
constexpr auto operator *(std::chrono::duration<TD, RD> d, quantity<TQ, UQ, RQ> q)
{
    return quantity<TD, DurationTag, RD>{d.count()} * q;
}

template<typename TQ, typename UQ, typename RQ, typename TD, typename RD>
constexpr auto operator /(quantity<TQ, UQ, RQ> q, std::chrono::duration<TD, RD> d)
{
    return q / quantity<TD, DurationTag, RD>{d.count()};
}

template<typename TQ, typename UQ, typename RQ, typename TD, typename RD>
constexpr auto operator /(std::chrono::duration<TD, RD> d, quantity<TQ, UQ, RQ> q)
{
    return quantity<TD, DurationTag, RD>{d.count()} / q;
}

template<class Q1, class Q2>
requires std::is_same<typename Q1::unit_type, typename Q2::unit_type>::value
constexpr Q1 quantityCast(Q2 q)
{
    return internal::quantityCastHelper<
            typename Q1::value_type, typename Q2::value_type,
            typename Q1::ratio_type, typename Q2::ratio_type,
            typename Q1::unit_type>(q);
}

template<typename quantity> constexpr auto unit = std::monostate{};

template<typename T> constexpr auto unit<std::chrono::duration<T, std::ratio<3600>>> =  u"h";
template<typename T> constexpr auto unit<std::chrono::duration<T, std::ratio<60>>>   =  u"m";
template<typename T> constexpr auto unit<std::chrono::duration<T>>                   =  u"s";
template<typename T> constexpr auto unit<std::chrono::duration<T, std::milli>>       = u"ms";
template<typename T> constexpr auto unit<std::chrono::duration<T, std::micro>>       = u"us";
template<typename T> constexpr auto unit<std::chrono::duration<T, std::nano>>        = u"ns";

template <typename T, class Unit, class Ratio>
inline QString toString(lmrs::core::quantity<T, Unit, Ratio> quantity)
{
    return (QLocale{}.toString(quantity.count())) + u'\u202f'
            + QString::fromUtf16(lmrs::core::unit<decltype(quantity)>);
}

template <class Unit, class Ratio>
inline QString toString(lmrs::core::quantity<qreal, Unit, Ratio> quantity, char format = 'f', int precision = 1)
{
    return (QLocale{}.toString(quantity.count(), format, precision)) + u'\u202f'
            + QString::fromUtf16(lmrs::core::unit<decltype(quantity)>);
}

#define LMRS_CORE_DECLARE_QUANTITY(category, name, ratio, suffix, unitName) \
    using name     = category<qint64, ratio>; \
    using name##_f = category<qreal,  ratio>; \
    \
    template<typename T> constexpr auto unit<category<T, ratio>> = \
        lmrs::core::internal::coallesce(unitName, u"" #suffix); \
    \
    namespace literals { \
    \
    constexpr name     operator ""_##suffix(unsigned long long value) { return static_cast<name::value_type>(value); } \
    constexpr name##_f operator ""_##suffix(long double value) { return static_cast<name##_f::value_type>(value); } \
    \
    } \
    \
    // end

#define LMRS_CORE_DECLARE_QUANTITY_X(category, name, num, den, suffix, unitName) \
    namespace internal { using name##_ratio = std::ratio<num / std::gcd(num, den), den / std::gcd(num, den)>; } \
    LMRS_CORE_DECLARE_QUANTITY(category, name, internal::name##_ratio, suffix, unitName) \
    // end

LMRS_CORE_DECLARE_QUANTITY(current, microamperes, std::micro,     uA, u"\u00B5A")
LMRS_CORE_DECLARE_QUANTITY(current, milliamperes, std::milli,     mA, u"")
LMRS_CORE_DECLARE_QUANTITY(current,      amperes, std::ratio<1>,   A, u"")
LMRS_CORE_DECLARE_QUANTITY(current,  kiloamperes, std::kilo,      kA, u"")

LMRS_CORE_DECLARE_QUANTITY(voltage,   microvolts, std::micro,     uV, u"\u00B5V")
LMRS_CORE_DECLARE_QUANTITY(voltage,   millivolts, std::milli,     mV, u"")
LMRS_CORE_DECLARE_QUANTITY(voltage,        volts, std::ratio<1>,   V, u"")
LMRS_CORE_DECLARE_QUANTITY(voltage,    kilovolts, std::kilo,      kV, u"")

LMRS_CORE_DECLARE_QUANTITY(temperature, celsius, std::ratio<1>, celsius, u"\u00b0C")

LMRS_CORE_DECLARE_QUANTITY(distance, micrometers, std::micro,     um, u"\u00B5m")
LMRS_CORE_DECLARE_QUANTITY(distance, millimeters, std::milli,     mm, u"")
LMRS_CORE_DECLARE_QUANTITY(distance, centimeters, std::centi,     cm, u"")
LMRS_CORE_DECLARE_QUANTITY(distance, decimeters,  std::deci,      dm, u"")
LMRS_CORE_DECLARE_QUANTITY(distance, meters,      std::ratio<1>,   m, u"")
LMRS_CORE_DECLARE_QUANTITY(distance, kilometers,  std::kilo,      km, u"")

LMRS_CORE_DECLARE_QUANTITY  (speed, millimeters_per_second,   std::milli,     mm_s, u"mm/s")
LMRS_CORE_DECLARE_QUANTITY  (speed, meters_per_second,        std::ratio<1>,   m_s,  u"m/s")
LMRS_CORE_DECLARE_QUANTITY_X(speed, feet_per_second,          1000, 3281,     ft_s, u"ft/s")
LMRS_CORE_DECLARE_QUANTITY_X(speed, kilometers_per_hour,      1000, 3600,     km_h, u"km/h")
LMRS_CORE_DECLARE_QUANTITY_X(speed, miles_per_hour,           1600, 3600,      mph,     u"")
LMRS_CORE_DECLARE_QUANTITY_X(speed, knots,                    1852, 3600,       kn,     u"")

LMRS_CORE_DECLARE_QUANTITY(acceleration, meters_per_square_second, std::ratio<1>, m_s2, u"m/s\u00B2")

LMRS_CORE_DECLARE_QUANTITY(frequency, hertz, std::ratio<1>,  hz, u"")

#define LMRS_CORE_DECLARE_QUANTITY_MULTIPLICATION(U1, U2, PR) \
    template<typename T1, typename T2, typename R> \
    struct lmrs::core::internal::QuantityProductType<T1, U1, T2, U2, R> \
        : public std::type_identity<PR<typename std::common_type<T1, T2>::type, R>> {}; \
    \
    template<typename T1, typename T2, typename R> \
    struct lmrs::core::internal::QuantityProductType<T2, U2, T1, U1, R> \
        : public lmrs::core::internal::QuantityProductType<T1, U1, T2, U2, R> {}; \
    \
    // end

#define LMRS_CORE_DECLARE_QUANTITY_DIVISION(U1, U2, QR) \
    template<typename T1, typename T2, typename R> \
    struct lmrs::core::internal::QuantityQuotientType<T1, U1, T2, U2, R> \
        : public std::type_identity<QR<typename std::common_type<T1, T2>::type, R>> {}; \
    \
    template<typename T1, typename T2, typename R> \
    struct lmrs::core::internal::QuantityQuotientType<T2, U2, T1, U1, R> \
        : public lmrs::core::internal::QuantityQuotientType<T1, U1, T2, U2, R> {}; \
    \
    // end

LMRS_CORE_DECLARE_QUANTITY_MULTIPLICATION(DurationTag, SpeedTag, distance)

LMRS_CORE_DECLARE_QUANTITY_DIVISION(DistanceTag, SpeedTag,    std::chrono::duration)
LMRS_CORE_DECLARE_QUANTITY_DIVISION(DistanceTag, DurationTag, speed)
LMRS_CORE_DECLARE_QUANTITY_DIVISION(SpeedTag,    DurationTag, acceleration)

} // namespace lmrs::core

template <typename T, typename Unit, typename Ratio>
struct std::numeric_limits<lmrs::core::quantity<T, Unit, Ratio>>
{
    using ratio_type = typename lmrs::core::quantity<T, Unit, Ratio>::ratio_type;
    using value_type = typename lmrs::core::quantity<T, Unit, Ratio>::value_type;

    static constexpr auto min() noexcept { return value_type{0}; }
    static constexpr auto max() noexcept { return value_type{ratio_type::den}; }
};

using namespace lmrs::core::literals;

#endif // LMRS_CORE_QUANTITY_H
