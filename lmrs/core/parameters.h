#ifndef LMRS_CORE_PARAMETERS_H
#define LMRS_CORE_PARAMETERS_H

#include "algorithms.h"
#include "localization.h"
#include "typetraits.h"

#include <QVariant>

class QHostAddress;

namespace lmrs::core::parameters {

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

class ChoiceListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum DataRole {
        TextRole = Qt::DisplayRole,
        ValueRole = Qt::UserRole,
    };

    explicit ChoiceListModel(QMetaType valueType, QObject *parent = nullptr)
        : QAbstractListModel{parent}
        , m_valueType{std::move(valueType)}
    {}

    explicit ChoiceListModel(QMetaType valueType, QList<Choice> choices, QObject *parent = nullptr)
        : QAbstractListModel{parent}
        , m_valueType{std::move(valueType)}
        , m_choices{std::move(choices)}
    {}

    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] auto valueType() const { return m_valueType; }
    [[nodiscard]] int findValue(QVariant value) const;
    [[nodiscard]] QVariantList values() const;

    void append(QString text, QVariant value);
    void removeAll(QVariant value);

    void setChoices(QList<Choice> choices);
    void setChoices(QVariantList values, std::function<QString (QVariant)> makeText = {});

    [[nodiscard]] static auto text(const QModelIndex &index) { return index.data(TextRole).toString(); }
    [[nodiscard]] static auto value(const QModelIndex &index) { return index.data(ValueRole); }

private:
    QMetaType m_valueType;
    QList<Choice> m_choices;
};

struct ParameterModel
{
    Q_GADGET

public:
    constexpr ParameterModel(QMetaType valueType = {})
        : valueType{std::move(valueType)}
    {}

    [[nodiscard]] auto isValid() const { return valueType.isValid(); }

    QMetaType valueType;
};

struct ChoiceModel : public ParameterModel
{
    Q_GADGET
    Q_PROPERTY(lmrs::core::parameters::ChoiceListModel *choices READ choices CONSTANT FINAL)

public:
    constexpr ChoiceModel() = default;

    ChoiceModel(QMetaType valueType, QList<Choice> choices)
        : ParameterModel{std::move(valueType)}
        , m_choices{std::make_shared<ChoiceListModel>(std::move(valueType), std::move(choices))}
    {}

    ChoiceModel(QMetaType valueType, std::shared_ptr<ChoiceListModel> choices)
        : ParameterModel{std::move(valueType)}
        , m_choices{std::move(choices)}
    {}

    ChoiceListModel *choices() const { return m_choices.get(); }

private:
    std::shared_ptr<ChoiceListModel> m_choices;
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

    [[nodiscard]] constexpr auto type() const noexcept { return m_type; }
    [[nodiscard]] constexpr auto flags() const noexcept { return m_flags; }

    [[nodiscard]] auto key() const noexcept { return m_key; }
    [[nodiscard]] auto name() const noexcept { return m_name; }
    [[nodiscard]] auto nameTr() const noexcept { return m_name.toString(); }
    [[nodiscard]] auto model() const noexcept { return m_model; }

    [[nodiscard]] bool acceptsType(QMetaType metaType) const;
    [[nodiscard]] QByteArray resetValueKey() const noexcept;
    [[nodiscard]] QByteArray hasValueKey() const noexcept;
    [[nodiscard]] QByteArray valueKey() const noexcept;

    [[nodiscard]] QString invalidValueText() const noexcept;

    [[nodiscard]] QByteArray typeName() const { return typeName(type()); }
    [[nodiscard]] static QByteArray typeName(Type type);

    [[nodiscard]] QVariant fromJson(QJsonValue value) const;
    [[nodiscard]] QJsonValue toJson(QVariant value) const;

    [[nodiscard]] static Parameter choice(QByteArrayView key, l10n::String name, ChoiceModel model, Flags flags = {});
    [[nodiscard]] static Parameter flag(QByteArrayView key, l10n::String name, bool defaultValue = false, Flags flags = {});
    [[nodiscard]] static Parameter number(QByteArrayView key, l10n::String name, NumberModel model, Flags flags = {});
    [[nodiscard]] static Parameter text(QByteArrayView key, l10n::String name, TextModel model = {}, Flags flags = {});

    [[nodiscard]] static Parameter hostAddress(QByteArrayView key, l10n::String name,
                                               ForwardDeclared<QList<QHostAddress>> auto proposals = {},
                                               Flags flags = {}); // FIXME: make this a generic value type


    [[nodiscard]] static Parameter choice(QByteArrayView key, l10n::String name,
                                          std::shared_ptr<ChoiceListModel> choices, Flags flags = {})
    {
        auto model = ChoiceModel{choices->valueType(), std::move(choices)};
        return choice(std::move(key), std::move(name), std::move(model), std::move(flags));
    }

    template<class T>
    [[nodiscard]] static Parameter choice(QByteArrayView key, l10n::String name, QList<Choice> choices, Flags flags = {})
    {
        qRegisterMetaType<T>();
        auto model = ChoiceModel{QMetaType::fromType<T>(), std::move(choices)};
        return choice(std::move(key), std::move(name), std::move(model), std::move(flags));
    }

    template<EnumType T>
    [[nodiscard]] static Parameter choice(QByteArrayView key, l10n::String name, Flags flags = {})
    {
        qRegisterMetaType<T>();
        return choice(std::move(key), std::move(name), QMetaType::fromType<T>(), core::metaEnum<T>(), std::move(flags));
    }

    template<typename T>
    [[nodiscard]] static Parameter number(QByteArrayView key, l10n::String name, Flags flags = {})
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
    [[nodiscard]] static auto &logger();

    [[nodiscard]] static Parameter text(QByteArrayView key, l10n::String name, QMetaType type,
                                        TextModel model, Flags flags);

    [[nodiscard]] static Parameter choice(QByteArrayView key, l10n::String name, QMetaType type,
                                          QMetaEnum values, Flags flags);
    [[nodiscard]] static Parameter choice(QByteArrayView key, l10n::String name, QMetaType type,
                                          std::shared_ptr<ChoiceListModel> choices, Flags flags);

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
