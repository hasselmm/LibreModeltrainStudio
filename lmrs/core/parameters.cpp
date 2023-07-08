#include "parameters.h"

#include "logging.h"
#include "typetraits.h"
#include "userliterals.h"

#include <QHostAddress>
#include <QJsonValue>

namespace lmrs::core {

namespace {

constexpr char ascii_toupper(char ch)
{
    if (ch >= 'a' && ch <= 'z')
        return ch - ('a' - 'A');

    return ch;
}

QByteArray prefixedKey(QByteArray key, QByteArray prefix)
{
    return prefix + ascii_toupper(key[0]) + key.mid(1);
}

} // namespace

QVariant parameters::ChoiceListModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (static_cast<DataRole>(role)) {
        case TextRole:
            return m_choices[index.row()].text;
        case ValueRole:
            return m_choices[index.row()].value;
        }
    }

    return {};
}

int parameters::ChoiceListModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_choices.count());
}

QHash<int, QByteArray> parameters::ChoiceListModel::roleNames() const
{
    return {
        {TextRole, "text"},
        {ValueRole, "value"},
    };
}

int parameters::ChoiceListModel::findValue(QVariant value) const
{
    const auto sameByValue = [value](const Choice &choice) {
        return choice.value == value;
    };

    const auto first = m_choices.constBegin();
    const auto last = m_choices.constEnd();

    if (const auto it = std::find_if(first, last, sameByValue); it != last)
        return static_cast<int>(it - first);

    return -1;
}

QVariantList parameters::ChoiceListModel::values() const
{
    auto values = QVariantList{};
    values.reserve(m_choices.size());

    std::transform(m_choices.constBegin(), m_choices.constEnd(), std::back_inserter(values),
                   [](const Choice &choice) { return choice.value; });

    return values;
}

void parameters::ChoiceListModel::append(QString text, QVariant value)
{
    const auto nextRow = static_cast<int>(m_choices.count());

    beginInsertRows({}, nextRow, nextRow);
    m_choices.emplaceBack(std::move(text), std::move(value));
    endInsertRows();
}

void parameters::ChoiceListModel::removeAll(QVariant value)
{
    for (auto it = m_choices.begin(); it != m_choices.end(); ) {
        if (it->value == value) {
            const auto row = static_cast<int>(it - m_choices.begin());
            beginRemoveRows({}, row, row);
            it = m_choices.erase(it);
            endRemoveRows();
        } else {
            ++it;
        }
    }
}

void parameters::ChoiceListModel::setChoices(QList<Choice> choices)
{
    beginResetModel();
    m_choices = std::move(choices);
    endResetModel();
}

void parameters::ChoiceListModel::setChoices(QVariantList values, std::function<QString (QVariant)> makeText)
{
    if (!makeText) {
        setChoices(values, [](auto value) { return value.toString(); });
        return;
    }

    auto choices = QList<Choice>{};
    choices.reserve(values.count());

    std::transform(values.constBegin(), values.constEnd(), std::back_inserter(choices), [makeText](auto value) {
        auto text = makeText(value);
        return Choice{std::move(text), std::move(value)};
    });

    setChoices(std::move(choices));
}

auto &Parameter::logger()
{
    return core::logger<Parameter>();
}

bool Parameter::acceptsType(QMetaType metaType) const
{
    if (LMRS_FAILED(logger(), metaType.isValid()))
        return false;

    switch (type()) {
    case Type::Invalid:
        return false;

    case Type::Choice:
        if (metaType.id() == qMetaTypeId<int>()
                || metaType.id() == qMetaTypeId<uint>())
            return true;

        if (const auto choiceModel = core::get_if<ChoiceModel>(model()))
            return metaType == choiceModel->valueType;

        return false;

    case Type::Flag:
        return QMetaType::canConvert(metaType, QMetaType::fromType<bool>());
    case Type::HostAddress:
        return QMetaType::canConvert(metaType, QMetaType::fromType<QHostAddress>());
    case Type::Number:
        return QMetaType::canConvert(metaType, QMetaType::fromType<int>());
    case Type::Text:
        return QMetaType::canConvert(metaType, QMetaType::fromType<QString>());
    }

    LMRS_UNIMPLEMENTED_FOR_KEY(type());
    return false;
}

QByteArray Parameter::resetValueKey() const noexcept
{
    return prefixedKey(key(), "reset");
}

QByteArray Parameter::hasValueKey() const noexcept
{
    return prefixedKey(key(), "has");
}

QByteArray Parameter::valueKey() const noexcept
{
    return key();
}

QString Parameter::invalidValueText() const noexcept
{
    return coalesce(m_invalidValueText.toString(), tr("any"));
}

QByteArray Parameter::typeName(Type type)
{
    if (const auto key = core::key(type))
        return key;

    return QByteArray::number(core::value(type));
}

QVariant Parameter::fromJson(QJsonValue value) const
{
    switch (type()) {
    case Type::Invalid:
        break;

    case Type::Choice:
        if (const auto choiceModel = core::get_if<ChoiceModel>(model())) {
            auto variant = QVariant::fromValue(value.toString());
            if (variant.convert(choiceModel->valueType))
                return variant;
        }

        break;

    case Type::Flag:
        return value.toBool();

    case Type::HostAddress:
        return QVariant::fromValue(QHostAddress{value.toString()});

    case Type::Number:
        return value.toInt();

    case Type::Text:
        return value.toString();
    }

    qCCritical(logger()) << "Could not convert value" << value
                         << "from JSON for" << core::key(type())
                         << "parameter" << key();

    return value.toVariant();
}

QJsonValue Parameter::toJson(QVariant value) const
{
    switch (type()) {
    case Type::Invalid:
        break;

    case Type::Choice:
        return value.toString(); // FIXME: is this really sufficient?

    case Type::Flag:
        if (const auto flag = core::get_if<bool>(value))
            return flag.value();

        break;

    case Type::HostAddress:
        if (const auto address = core::get_if<QHostAddress>(value))
            return address->toString();

        break;

    case Type::Number:
        if (const auto number = core::convert_if<int>(value))
            return number.value();

        break;

    case Type::Text:
        if (const auto text = core::get_if<QString>(value))
            return text.value();

        break;
    }

    qCCritical(logger()) << "Could not convert value" << value
                         << "to JSON for" << core::key(type())
                         << "parameter" << key();

    return QJsonValue::fromVariant(std::move(value));
}

Parameter Parameter::choice(QByteArrayView key, l10n::String name, parameters::ChoiceModel model, Flags flags)
{
    if (LMRS_FAILED(logger(), model.isValid())
            || LMRS_FAILED(logger(), model.choices())) {
        qCCritical(logger(), "Invalid choice model for parameter \"%s\"", key.constData());
        return {};
    }

    for (const auto &index: model.choices()) {
        const auto value = ChoiceListModel::value(index);

        if (LMRS_FAILED_EQUALS(logger(), value.metaType(), model.valueType)) {
            qCCritical(logger(), "Choice \"%ls\" at index %d has invalid type %s (expected: %s)",
                       qUtf16Printable(ChoiceListModel::text(index)), index.row(),
                       value.typeName(), model.valueType.name());
        }
    }

    return {Type::Choice, std::move(flags), std::move(key), std::move(name), QVariant::fromValue(std::move(model))};
}

Parameter Parameter::choice(QByteArrayView key, l10n::String name, QMetaType type, QMetaEnum values, Flags flags)
{
    if (LMRS_FAILED(logger(), type.isRegistered())
            || LMRS_FAILED(logger(), type.flags() & QMetaType::IsEnumeration)
            || LMRS_FAILED(logger(), values.isValid()))
        return {};

    auto choices = QList<Choice>{};

    for (const auto &entry: values) {
        auto value = QVariant::fromValue(entry.value());

        if (!value.convert(type)) {
            if (static bool showWarning = true; showWarning) {
                qCWarning(logger(), "Could not convert enum value to type %s", type.name());
                showWarning = false;
            }
        }

        choices.emplaceBack(QString::fromLatin1(entry.key()), std::move(value));
    }

    return choice(std::move(key), std::move(name), {std::move(type), std::move(choices)}, std::move(flags));
}

Parameter Parameter::flag(QByteArrayView key, l10n::String name, bool defaultValue, Flags flags)
{
    return {Type::Flag, std::move(flags), std::move(key), std::move(name), defaultValue};
}

Parameter Parameter::number(QByteArrayView key, l10n::String name, parameters::NumberModel model, Flags flags)
{
    if (LMRS_FAILED(logger(), model.valueType.isRegistered())
            || LMRS_FAILED(logger(), QMetaType::canConvert(model.valueType, QMetaType::fromType<int>()))
            || LMRS_FAILED(logger(), QMetaType::canConvert(QMetaType::fromType<int>(), model.valueType))
            || LMRS_FAILED_COMPARE(logger(), model.minimumValue, <=, model.maximumValue))
        qCWarning(logger(), "Invalid value type for parameter \"%s\"", key.constData());

    return {Type::Number, std::move(flags), std::move(key), std::move(name), QVariant::fromValue(std::move(model))};
}

Parameter Parameter::text(QByteArrayView key, l10n::String name, parameters::TextModel model, Flags flags)
{
    return {Type::Text, std::move(flags), std::move(key), std::move(name), QVariant::fromValue(std::move(model))};
}

template<>
Parameter Parameter::hostAddress(QByteArrayView key, l10n::String name, QList<QHostAddress> proposals, Flags flags)
{
    return {Type::HostAddress, std::move(flags), std::move(key), std::move(name), QVariant::fromValue(std::move(proposals))};
}

} // namespace lmrs::core
