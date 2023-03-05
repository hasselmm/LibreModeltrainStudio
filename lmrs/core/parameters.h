#ifndef LMRS_CORE_PARAMETERS_H
#define LMRS_CORE_PARAMETERS_H

#include "algorithms.h"
#include "localization.h"
#include "typetraits.h"

#include <QVariant>

class QHostAddress;

namespace lmrs::core::parameters {

struct ParameterModel
{
    Q_GADGET

public:
    constexpr ParameterModel(QMetaType valueType = {})
        : valueType{std::move(valueType)}
    {}

    QMetaType valueType;
};

struct Choice
{
    Q_GADGET
    Q_PROPERTY(QString text MEMBER text CONSTANT FINAL)
    Q_PROPERTY(QVariant value MEMBER value CONSTANT FINAL)

public:
    Choice(QString text, QVariant value)
        : text{std::move(text)}
        , value{std::move(value)}
    {}

    QString text;
    QVariant value;
};

struct ChoiceModel : public ParameterModel
{
    Q_GADGET
    Q_PROPERTY(QList<lmrs::core::parameters::Choice> choices MEMBER choices CONSTANT FINAL)
    Q_PROPERTY(QVariantList qmlChoices READ qmlChoices CONSTANT FINAL)

public:
    QList<Choice> choices;

    QVariantList qmlChoices() const
    {
        auto result = QVariantList{};
        result.reserve(choices.size());
        std::transform(choices.begin(), choices.end(), std::back_inserter(result), &QVariant::fromValue<Choice>);
        return result;
    }
};

struct NumberModel : public ParameterModel
{
    Q_GADGET
    Q_PROPERTY(int minimumValue MEMBER minimumValue CONSTANT FINAL)
    Q_PROPERTY(int maximumValue MEMBER maximumValue CONSTANT FINAL)

public:
    int minimumValue;
    int maximumValue;

    constexpr NumberModel(int minimumValue, int maximumValue) noexcept
        : ParameterModel{QMetaType::fromType<int>()}
        , minimumValue{minimumValue}
        , maximumValue{maximumValue}
    {}

    template<typename T = int>
    constexpr NumberModel(Range<T> range) noexcept
        : ParameterModel{QMetaType::fromType<T>()}
        , minimumValue{range.first}
        , maximumValue{range.last}
    {}
};

struct TextModel
{
    Q_GADGET
    Q_PROPERTY(QStringList proposals MEMBER proposals CONSTANT FINAL)

public:
    QStringList proposals;

    TextModel(QStringList proposals = {}) noexcept
        : proposals{proposals}
    {}
};

struct Parameter
{
    Q_GADGET

    Q_PROPERTY(Type type READ type CONSTANT FINAL)
    Q_PROPERTY(Flags flags READ flags CONSTANT FINAL)

    Q_PROPERTY(QByteArray key READ key CONSTANT FINAL)
    Q_PROPERTY(QByteArray valueKey READ valueKey CONSTANT FINAL)
    Q_PROPERTY(QByteArray hasValueKey READ hasValueKey CONSTANT FINAL)
    Q_PROPERTY(QByteArray resetValueKey READ resetValueKey CONSTANT FINAL)

    Q_PROPERTY(QString name READ nameTr CONSTANT FINAL)
    Q_PROPERTY(QVariant model READ model CONSTANT FINAL)

    Q_PROPERTY(QString invalidValueText READ invalidValueText CONSTANT FINAL)
    Q_PROPERTY(QByteArray typeName READ typeName CONSTANT FINAL)

    QT_TR_FUNCTIONS

public:
    enum class Type {
        Invalid,
        Choice,
        Flag,
        HostAddress,
        Number,
        Text,
    };

    Q_ENUM(Type)

    enum class Flag {
        Optional = (1 << 0),
        Hexadecimal = (1 << 1),
    };

    Q_FLAG(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    Parameter() noexcept = default;
    Parameter(Type type, Flags flags, QByteArrayView key, l10n::String name, QVariant model) noexcept
        : m_type{type}
        , m_flags{std::move(flags)}
        , m_key{key.toByteArray()}
        , m_name{std::move(name)}
        , m_model{std::move(model)}
    {}

    constexpr auto type() const noexcept { return m_type; }
    constexpr auto flags() const noexcept { return m_flags; }

    auto key() const noexcept { return m_key; }
    auto name() const noexcept { return m_name; }
    auto nameTr() const noexcept { return m_name.toString(); }
    auto model() const noexcept { return m_model; }

    bool acceptsType(QMetaType metaType) const;
    QByteArray resetValueKey() const noexcept;
    QByteArray hasValueKey() const noexcept;
    QByteArray valueKey() const noexcept;

    QString invalidValueText() const noexcept;

    QByteArray typeName() const { return typeName(type()); }
    static QByteArray typeName(Type type);

    QVariant fromJson(QJsonValue value) const;
    QJsonValue toJson(QVariant value) const;

    static Parameter choice(QByteArrayView key, l10n::String name, ChoiceModel model, Flags flags = {});
    static Parameter flag(QByteArrayView key, l10n::String name, bool defaultValue = false, Flags flags = {});
    static Parameter number(QByteArrayView key, l10n::String name, NumberModel model, Flags flags = {});
    static Parameter text(QByteArrayView key, l10n::String name, TextModel model = {}, Flags flags = {});

    static Parameter hostAddress(QByteArrayView key, l10n::String name,
                                 ForwardDeclared<QList<QHostAddress>> auto proposals = {},
                                 Flags flags = {}); // FIXME: make this a generic value type


    template<class T>
    static Parameter choice(QByteArrayView key, l10n::String name, QList<Choice> choices, Flags flags = {})
    {
        qRegisterMetaType<T>();
        auto model = ChoiceModel{QMetaType::fromType<T>(), std::move(choices)};
        return choice(std::move(key), std::move(name), std::move(model), std::move(flags));
    }

    template<EnumType T>
    static Parameter choice(QByteArrayView key, l10n::String name, Flags flags = {})
    {
        qRegisterMetaType<T>();
        return choice(std::move(key), std::move(name), QMetaType::fromType<T>(), core::metaEnum<T>(), std::move(flags));
    }

    template<typename T>
    static Parameter number(QByteArrayView key, l10n::String name, Flags flags = {})
    {
        qRegisterMetaType<T>();

        if (!QMetaType::hasRegisteredConverterFunction<T, int>())
            QMetaType::registerConverter<T, int>();
        if (!QMetaType::hasRegisteredConverterFunction<int, T>())
            QMetaType::registerConverter<int, T>();

        return number(std::move(key), std::move(name), core::Range<T>{}, std::move(flags));
    }

private:
    static auto &logger(auto) = delete;
    static auto &logger();

    static Parameter text(QByteArrayView key, l10n::String name, QMetaType type, TextModel model, Flags flags);
    static Parameter choice(QByteArrayView key, l10n::String name, QMetaType type, QMetaEnum values, Flags flags);

    Type m_type = Type::Invalid;
    Flags m_flags;
    QByteArray m_key;
    l10n::String m_name;
    QVariant m_model;
    l10n::String m_invalidValueText;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Parameter::Flags)

} // namespace lmrs::core::parameters

namespace lmrs::core {
using parameters::Parameter;
}

#endif // LMRS_CORE_PARAMETERS_H
