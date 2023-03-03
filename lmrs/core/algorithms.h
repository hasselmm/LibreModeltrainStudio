#ifndef LMRS_CORE_ALGORITHMS_H
#define LMRS_CORE_ALGORITHMS_H

#include "typetraits.h"
#include "userliterals.h"

#include <QAbstractItemModel>
#include <QMetaEnum>
#include <QPointer>

#include <iterator>
#include <type_traits>

namespace lmrs::core {

template<class T, size_t N>
std::array<T *, N> generateArray(auto... args)
{
    std::array<T *, N> array;
    std::generate(array.begin(), array.end(), [args...] { return new T{args...}; });
    return array;
}

template<class StringType>
auto printable(const StringType &text)
{
    return static_cast<const wchar_t*>(static_cast<const void *>(text.utf16()));
}

template<HasIsEmpty T>
constexpr bool isEmpty(const T &value)
{
    return value.isEmpty();
}

template<typename T>
constexpr bool isEmpty(const T &value)
{
    return value == T{};
}

template<typename T>
QString hexString(T value)
{
    return "0x"_L1 + QString::number(value, 16).rightJustified(sizeof(T) * 2, '0'_L1);
}

QString hexString(QVariant value);

template<typename T>
std::optional<T> get_if(const QVariant &variant)
{
    if (variant.typeId() == qMetaTypeId<T>())
        return qvariant_cast<T>(variant);

    return {};
}

template<typename T>
std::optional<T> convert_if(const QVariant &variant)
{
    if (QMetaType::canConvert(variant.metaType(), QMetaType::fromType<T>()))
        return qvariant_cast<T>(variant);

    return {};
}

template<class StringType>
std::optional<int> toInt(StringType text, int base = 10);

template<class StringType>
std::optional<qint64> toInt64(StringType text, int base = 10);

template<typename T, class StringType>
inline std::optional<T> parse(StringType text, int base = 10)
{
    const auto result = toInt(std::move(text), base);

    if (result.has_value()
            && result.value() >= MinimumValue<T>
            && result.value() <= MaximumValue<T>)
        return static_cast<typename T::value_type>(result.value());

    return {};
}

template<HasToString T>
inline auto toString(const T &value)
{
    return value.toString();
}

template<HasToString T>
inline QStringList toStringList(const QList<T> &values)
{
    auto result = QStringList{};
    result.reserve(values.size());
    std::transform(values.begin(), values.end(), std::back_inserter(result), core::toString<T>);
    return result;
}

constexpr auto coalesce(auto value, auto... args) -> decltype(value)
{
    if constexpr(sizeof...(args) > 0) {
        if (isEmpty(value))
            return coalesce(args...);
    }

    return value;
}

static_assert(coalesce(1) == 1);
static_assert(coalesce(0, 1) == 1);

template <typename T>
bool setData(T *target, const QVariant &value)
{
    if (Q_UNLIKELY(!target || !value.canConvert<T>()))
        return false;

    *target = qvariant_cast<T>(value);
    return true;
}

namespace internal {

constexpr const char *strnrchr(const char *str, char ch, size_t n)
{
    for (auto p = str + n; p > str; --p) {
        if (*p == ch)
            return p;
    }

    return nullptr;
}

static_assert(*(strnrchr(&"foo::bar::buzz"[5], ':', 9) + 2) == 'u');

template<class Target, size_t N, size_t M, typename... Args>
QMetaMethod findMetaMethod(void (Target::*)(Args...), const char (&signature)[N], const char (&)[M])
{
    if (const auto colon = strnrchr(signature, ':', N)) {
        const auto index = Target::staticMetaObject.indexOfMethod(colon + 1);
        return Target::staticMetaObject.method(index);
    }

    return {};
}

} // namespace internal

#define LMRS_CORE_FIND_META_METHOD(Method, Args...) \
    ::lmrs::core::internal::findMetaMethod((Method), #Method "(" #Args ")", #Method)

QMetaEnum metaEnumFromMetaType(QMetaType metaType);

template<EnumType T>
inline const auto &metaEnum()
{
    static const auto meta = QMetaEnum::fromType<T>();
    return meta;
}

template<EnumType T>
inline auto keyCount()
{
    return metaEnum<T>().keyCount();
}

template<EnumType T>
inline auto key(T value)
{
    return metaEnum<T>().valueToKey(static_cast<int>(value));
}

template<EnumType T>
inline auto keys(QFlags<T> values)
{
    if (const auto s = key(T{static_cast<typename QFlags<T>::Int>(values)}))
        return QByteArray::fromRawData(s, static_cast<qsizetype>(strlen(s)));

    return metaEnum<T>().valueToKeys(values);
}

template<typename T>
class MetaEnumEntry // FIXME: move to separate header to reduce dependencies
{
public:
    MetaEnumEntry(QMetaEnum meta, int index) noexcept
        : m_meta{std::move(meta)}, m_index{index} {}

    constexpr auto index() const noexcept { return m_index; }
    auto key() const noexcept { return m_meta.key(m_index); }
    auto value() const noexcept { return static_cast<T>(m_meta.value(m_index)); }

    constexpr auto operator==(const MetaEnumEntry &rhs) const noexcept { return fields() == rhs.fields(); }
    constexpr auto operator!=(const MetaEnumEntry &rhs) const noexcept { return fields() != rhs.fields(); }

    auto &operator++() noexcept { ++m_index; return *this; }

    constexpr operator T() const noexcept { return value(); }
    operator QVariant() const noexcept { return QVariant::fromValue(value()); }

private:
        auto fields() const noexcept { return std::make_tuple(m_meta.name(), m_index); }

    QMetaEnum m_meta;
    int m_index;
};

template<typename T>
class MetaEnumIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = ptrdiff_t;
    using value_type        = T;
    using pointer           = value_type*;
    using reference         = value_type&;

    MetaEnumIterator(QMetaEnum meta, int index)
        : m_entry{std::move(meta), index} {}

    auto &operator ++() { ++m_entry; return *this; }
    auto *operator ->() const { return &m_entry; }
    auto &operator  *() const { return m_entry; }

    auto operator ==(const MetaEnumIterator &rhs) const { return m_entry == rhs.m_entry; }
    auto operator !=(const MetaEnumIterator &rhs) const { return m_entry != rhs.m_entry; }

    static auto begin(QMetaEnum meta) { return MetaEnumIterator{std::move(meta), 0}; }
    static auto end(QMetaEnum meta) { const auto n = meta.keyCount(); return MetaEnumIterator{std::move(meta), n}; }

private:
    MetaEnumEntry<T> m_entry;
};

static_assert(requires { typename std::iterator_traits<MetaEnumIterator<Qt::AlignmentFlag>>::iterator_category; });

template <FlagsType T>
class FlagsIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = ptrdiff_t;
    using value_type        = typename T::enum_type;
    using pointer           = value_type*;
    using reference         = value_type&;

    FlagsIterator(T mask = {}, int index = keyCount())
        : m_index{index}
        , m_mask{mask}
    {
        if (!hasValue() && isInRange())
            increment();
    }

    [[nodiscard]] auto operator==(const FlagsIterator &rhs) const { return fields() == rhs.fields(); }
    [[nodiscard]] auto operator!=(const FlagsIterator &rhs) const { return fields() != rhs.fields(); }

    [[nodiscard]] auto value() const { return static_cast<value_type>(metaEnum().value(m_index)); }
    [[nodiscard]] auto operator*() const { return value(); }

    FlagsIterator &operator++() { increment(); return *this; }
    FlagsIterator operator++(int) { auto backup = *this; increment(); return backup; }

private:
    static auto metaEnum() { static const auto s_metaEnum = QMetaEnum::fromType<value_type>(); return s_metaEnum; }
    static auto keyCount() { return metaEnum().keyCount(); }

    auto fields() const { return std::make_tuple(m_index, m_mask); }
    void increment() { do { ++m_index; } while(!hasValue() && isInRange()); }

    auto isValue(auto value) const  {  return core::value(value) != 0 && m_mask.testFlag(value); }
    auto isInRange() const { return m_index < keyCount(); }
    auto hasValue() const { return isValue(value()); }

    int m_index;
    T m_mask;
};

static_assert(requires { typename std::iterator_traits<FlagsIterator<Qt::Alignment>>::iterator_category; });

class ItemModelIterator // FIXME: move to separate header to reduce dependencies
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = int;
    using value_type        = QModelIndex;
    using pointer           = value_type*;
    using reference         = value_type&;

    ItemModelIterator() noexcept = default;
    ItemModelIterator(const QAbstractItemModel *model, int row) noexcept
        : m_model{model}
        , m_row{row}
    {}

    auto operator*() const { return m_model ? m_model->index(m_row, 0) : QModelIndex{}; }
    auto &operator++() { ++m_row; return *this; }
    auto operator-(const ItemModelIterator &rhs) const noexcept { return m_row - rhs.m_row; }

    auto fields() const noexcept { return std::tie(m_model, m_row); }
    auto operator==(const ItemModelIterator &rhs) const noexcept { return fields() == rhs.fields(); }
    auto operator!=(const ItemModelIterator &rhs) const noexcept { return fields() != rhs.fields(); }

private:
    QPointer<const QAbstractItemModel> m_model;
    int m_row;
};

static_assert(requires { typename std::iterator_traits<ItemModelIterator>::iterator_category; });

} // namespace lmrs::core

inline auto begin(const QMetaEnum &meta)
{
    return lmrs::core::MetaEnumIterator<int>::begin(meta);
}

inline auto end(const QMetaEnum &meta)
{
    return lmrs::core::MetaEnumIterator<int>::end(meta);
}

template<lmrs::core::EnumType T>
inline auto begin(const QMetaTypeId<T> &)
{
    return lmrs::core::MetaEnumIterator<T>::begin(lmrs::core::metaEnum<T>());
}

template<lmrs::core::EnumType T>
inline auto end(const QMetaTypeId<T> &)
{
    return lmrs::core::MetaEnumIterator<T>::end(lmrs::core::metaEnum<T>());
}

template <lmrs::core::FlagsType T>
inline auto begin(T flags)
{
    return lmrs::core::FlagsIterator<T>{flags, 0};
}

template <lmrs::core::FlagsType T>
inline auto end(T flags)
{
    return lmrs::core::FlagsIterator<T>{flags};
}

inline auto begin(const QAbstractItemModel *model)
{
    return lmrs::core::ItemModelIterator{model, 0};
}

inline auto end(const QAbstractItemModel *model)
{
    return lmrs::core::ItemModelIterator{model, model->rowCount()};
}

#endif // LMRS_CORE_ALGORITHMS_H
