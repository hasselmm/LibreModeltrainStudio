#ifndef LMRS_CORE_TYPETRAITS_H
#define LMRS_CORE_TYPETRAITS_H

#include <QObject>

class QAction;
class QIcon;

namespace lmrs::core {

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <typename T>
concept FlagsType = std::is_enum_v<typename T::enum_type>
        && std::is_same_v<QFlags<typename T::enum_type>, T>;

template<typename T>
concept HasIsEmpty = requires(T value) { value.isEmpty(); };

template<typename T>
concept HasIsNull = requires(T value) { value.isNull(); };

template<typename T>
concept HasParent = requires(T object) { object.parent(); };

template<typename T>
concept HasToString = requires(T object) { object.toString(); };

template<typename T, typename U>
concept ForwardDeclared = requires(T) { std::is_same_v<T, U>; };

template<typename T, std::enable_if_t<!std::is_enum<T>::value, bool> = true>
constexpr auto value(T value)
{
    return value;
}

template<typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
constexpr auto value(T value)
{
    return static_cast<typename std::underlying_type<T>::type>(value);
}

template<typename T> constexpr T MinimumValue = std::numeric_limits<T>::min();
template<typename T> constexpr T MaximumValue = std::numeric_limits<T>::max();

template<> inline constexpr auto MinimumValue<QChar> = std::numeric_limits<char16_t>::min();
template<> inline constexpr auto MaximumValue<QChar> = std::numeric_limits<char16_t>::max();

static_assert(MinimumValue<QChar> == 0);
static_assert(MaximumValue<QChar> == 65535);

template<typename T, typename Tag, auto minimum = MinimumValue<T>, auto maximum = MaximumValue<T>>
struct literal
{
    constexpr literal() = default;
    constexpr literal(T value) : value{value} {}
    constexpr operator T() const { return value; }
    constexpr auto operator*() const { return value; }
    constexpr auto operator->() const { return &value; }

    constexpr literal &operator+=(T rhs) { value += rhs; return *this; }
    constexpr literal &operator|=(T rhs) { value |= rhs; return *this; }

    constexpr literal &operator++() { ++value; return *this; }
    constexpr literal operator++(int) { return std::exchange(value, value + 1); }

    constexpr static auto Minimum = T{minimum};
    constexpr static auto Maximum = T{maximum};

    using value_type = T;
    value_type value = {};
};

template<typename T, typename Tag, auto Min, auto Max>
constexpr auto MinimumValue<literal<T, Tag, Min, Max>> = literal<T, Tag, Min, Max>::Minimum;

template<typename T, typename Tag, auto Min, auto Max>
constexpr auto MaximumValue<literal<T, Tag, Min, Max>> = literal<T, Tag, Min, Max>::Maximum;

template<typename T, typename Tag, auto Min, auto Max>
constexpr auto value(literal<T, Tag, Min, Max> literal)
{
    return literal.value;
}

template<typename T, typename Tag, auto Min, auto Max>
constexpr static auto operator+(literal<T, Tag, Min, Max> lhs, T rhs)
{
    return literal<T, Tag, Min, Max>{static_cast<T>(lhs.value + rhs)};
}

template <typename T>
struct Range
{
    T first;
    T last;

    constexpr Range() noexcept
        : Range{MinimumValue<T>, MaximumValue<T>} {}
    constexpr Range(T first, T last) noexcept
        : first{first}, last{last} {}
    template<typename U>
    constexpr Range(Range<U> range) noexcept
        : Range{range.first, range.last} {}

    constexpr bool contains(auto x) const;
    constexpr auto size() const { return value(last) - value(first) + 1; }
    constexpr auto fields() const { return std::tie(first, last); }

    constexpr auto operator==(const Range &rhs) const { return fields() == rhs.fields(); }
    constexpr auto operator!=(const Range &rhs) const { return !operator==(rhs); }

    class const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = T&;

        explicit constexpr const_iterator(T value) : m_value{value} {}

        constexpr auto operator==(const const_iterator &rhs) const { return m_value == rhs.m_value; }
        constexpr auto operator!=(const const_iterator &rhs) const { return m_value != rhs.m_value; }

        constexpr auto operator*() const { return m_value; }
        auto &operator++() { m_value = T{value(m_value) + 1}; return *this; }

    private:
        T m_value;
    };

    template<typename U = T>
    auto toList(U (*convert)(T) = [](T v) { return v; }) const
    {
        auto list = QList<U>{};
        list.reserve(size());
        std::transform(begin(), end(), std::back_inserter(list), convert);
        return list;
    }

    constexpr auto begin() const { return const_iterator{first}; }
    constexpr auto end() const { return const_iterator{T{value(last) + 1}}; }
};

template<typename T>
constexpr bool Range<T>::contains(auto x) const
{
    return value(x) >= value(first) && value(x) <= value(last);
}

static_assert(Range{1, 3}.contains(0) == false);
static_assert(Range{1, 3}.contains(1) == true);
static_assert(Range{1, 3}.contains(2) == true);
static_assert(Range{1, 3}.contains(3) == true);
static_assert(Range{1, 3}.contains(4) == false);

template<typename T, typename Value = T>
constexpr bool inRange(Value value) { return Range<T>{}.contains(value); }

template<typename T, typename Value = T>
constexpr bool outOfRange(Value value) { return !inRange<T>(value); }

static_assert(inRange<qint8>(127));
static_assert(inRange<qint8>(-128));
static_assert(outOfRange<qint8>(383));
static_assert(outOfRange<qint8>(-129));
static_assert(outOfRange<quint8>(256) && static_cast<quint8>(256) == 0);

template<class T, class U>
constexpr T checked_cast(U object)
{
    Q_ASSERT(!object || dynamic_cast<T>(object));
    return static_cast<T>(object);
}

template<EnumType T>
struct EnableFlags : public std::false_type {};

template<typename T>
concept HasFlagsEnabled = EnumType<T> && EnableFlags<T>::value;

#define LMRS_CORE_ENABLE_FLAGS(T) template<> struct lmrs::core::EnableFlags<T> : public std::true_type {};

template<HasFlagsEnabled T>
struct FlagsTag { using type = T; };

template<HasFlagsEnabled T>
struct Flags : public literal<uint, FlagsTag<T>>
{
    using literal<uint, FlagsTag<T>>::literal;

    constexpr explicit Flags(uint value) : literal<uint, FlagsTag<T>>{value} {}
    constexpr Flags(T value) : literal<uint, FlagsTag<T>>{(1U << core::value(value))} {}

    constexpr auto isSet(T test) const;
    constexpr auto areSet(Flags<T> test) const;
};

template<EnumType T> [[nodiscard]]
constexpr auto flags(T value)
{
    return Flags<T>{value};
}

template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator&(Flags<T> lhs, Flags<T> rhs) noexcept { return Flags<T>{lhs.value & rhs.value}; }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator|(Flags<T> lhs, Flags<T> rhs) noexcept { return Flags<T>{lhs.value | rhs.value}; }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator^(Flags<T> lhs, Flags<T> rhs) noexcept { return Flags<T>{lhs.value ^ rhs.value}; }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator+(Flags<T> lhs, Flags<T> rhs) noexcept = delete;
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator-(Flags<T> lhs, Flags<T> rhs) noexcept = delete;
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator*(Flags<T> lhs, Flags<T> rhs) noexcept = delete;
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator/(Flags<T> lhs, Flags<T> rhs) noexcept = delete;
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator%(Flags<T> lhs, Flags<T> rhs) noexcept = delete;

template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator&(Flags<T> lhs, T rhs) noexcept { return lhs & flags(rhs); }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator|(Flags<T> lhs, T rhs) noexcept { return lhs | flags(rhs); }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator^(Flags<T> lhs, T rhs) noexcept { return lhs ^ flags(rhs); }

template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator&(T lhs, T rhs) noexcept { return flags(lhs) & flags(rhs); }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator|(T lhs, T rhs) noexcept { return flags(lhs) | flags(rhs); }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator^(T lhs, T rhs) noexcept { return flags(lhs) ^ flags(rhs); }

template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator~(Flags<T> flags) noexcept { return Flags<T>{~flags.value}; }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator!(Flags<T> flags) noexcept = delete;

template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator~(T flag) noexcept { return ~flags(flag); }
template<HasFlagsEnabled T> [[nodiscard]] constexpr auto operator!(T flag) noexcept = delete;

template<HasFlagsEnabled T>
constexpr auto Flags<T>::areSet(Flags<T> test) const
{
    return (literal<uint, FlagsTag<T>>::value & test) == test;
}

template<HasFlagsEnabled T>
constexpr auto Flags<T>::isSet(T test) const
{
    return areSet(flags(test));
}

template<class T>
inline const QLoggingCategory &logger();

template<class PublicObject, HasParent BaseType = QObject>
class PrivateObject : public BaseType
{
    friend PublicObject;

protected:
    explicit PrivateObject(PublicObject *parent)
        : BaseType{parent} {}

    auto q() const { return core::checked_cast<const PublicObject *>(BaseType::parent()); }
    auto q() { return core::checked_cast<PublicObject *>(BaseType::parent()); }

    static QString tr(const char *s, const char *c = nullptr, int n = -1) { return PublicObject::tr(s, c, n); }

    static auto &logger(auto) = delete; // purposefully not defined to catch errors
    static auto &logger() { return core::logger<PublicObject>(); }

    template<ForwardDeclared<QIcon> Icon, typename... Args>
    QAction *createAction(Icon icon, QString text, QString tooltip, Args... args)
    {
        const auto action = q()->addAction(std::move(icon), std::move(text), std::move(args)...);
        action->setToolTip(std::move(tooltip));
        return action;
    }
};

template<class PublicObject, class BaseType>
const QLoggingCategory &logger(const PrivateObject<PublicObject, BaseType> *) = delete;

template<class PublicObject, class BaseType>
const QLoggingCategory &logger(PrivateObject<PublicObject, BaseType> *) = delete;

} // namespace lmrs::core

template <typename T, typename Tag>
struct std::underlying_type<lmrs::core::literal<T, Tag>>
        : public std::type_identity<T> {};

#endif // LMRS_CORE_TYPETRAITS_H
