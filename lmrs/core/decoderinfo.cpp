#include "decoderinfo.h"

#include "logging.h"
#include "userliterals.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

extern int qInitResources_decoderinfo();

namespace lmrs::core::dcc {

namespace {

constexpr auto s_bcd = "bcd"_L1;
constexpr auto s_extends = "extends"_L1;
constexpr auto s_fields = "fields"_L1;
constexpr auto s_flags = "flags"_L1;
constexpr auto s_hasRailcom = "has-railcom"_L1;
constexpr auto s_name = "name"_L1;
constexpr auto s_type = "type"_L1;
constexpr auto s_unsupported = "unsupported"_L1;
constexpr auto s_values = "values"_L1;
constexpr auto s_variables = "variables"_L1;
constexpr auto s_width = "width"_L1;
constexpr auto s_pages = "pages"_L1;
constexpr auto s_pointer = "pointer"_L1;

auto initResources()
{
    static const auto resourceId = qInitResources_decoderinfo();
    return resourceId;
}

QJsonObject readDecoderDefinitions()
{
    initResources();

    auto file = QFile{":/taschenorakel.de/lmrs/core/data/decoders.json"_L1};

    if (!file.open(QFile::ReadOnly)) {
        qCWarning(logger<DecoderInfo>(), "Could not read variable definitions: %ls",
                  qUtf16Printable(file.errorString()));
        return {};
    }

    auto status = QJsonParseError{};
    const auto json = file.readAll();
    const auto document = QJsonDocument::fromJson(json, &status);

    if (status.error != QJsonParseError::NoError) {
        const auto lineNumber = static_cast<int>(json.left(status.offset).count('\n') + 1);
        qCWarning(logger<DecoderInfo>(), "Could not read variable definitions: %ls (line %d)",
                  qUtf16Printable(status.errorString()), lineNumber);
    }

    return document.object();
}

using ManufacturerTable = QHash<int, QString>;
ManufacturerTable readManufacturerNames()
{
    initResources();

    auto file = QFile{":/taschenorakel.de/lmrs/core/data/manufacturers.json"_L1};

    if (!file.open(QFile::ReadOnly)) {
        qCWarning(logger<DecoderInfo>(), "Could not read manufacturer list: %ls",
                  qUtf16Printable(file.errorString()));
        return {};
    }

    auto status = QJsonParseError{};
    const auto json = file.readAll();
    const auto document = QJsonDocument::fromJson(json, &status);

    auto manufacturerNames = ManufacturerTable{};

    constexpr auto s_manufacturers = "manufacturers"_L1;
    constexpr auto s_identifier = "id"_L1;

    for (const auto &entry: document[s_manufacturers].toArray()) {
        const auto identifier = entry[s_identifier].toInt();
        auto name = entry[s_name].toString();

        if (identifier > 0 && !name.isEmpty())
            manufacturerNames.insert(identifier, std::move(name));
    }

    return manufacturerNames;
}

Q_GLOBAL_STATIC_WITH_ARGS(QJsonObject, s_decoderDefinitions, {readDecoderDefinitions()})
Q_GLOBAL_STATIC_WITH_ARGS(QJsonObject, s_pageDefinitions, {s_decoderDefinitions->value(s_pages).toObject()})
Q_GLOBAL_STATIC_WITH_ARGS(ManufacturerTable, s_manufacturerNames, {readManufacturerNames()})

std::optional<ExtendedPageIndex> extendedPageIndex(QJsonValue pointer)
{
    if (const auto array = pointer.toArray(); array.size() == 2) {
        const auto cv31 = static_cast<VariableValue::value_type>(array[0].toInt());
        const auto cv32 = static_cast<VariableValue::value_type>(array[1].toInt());
        return extendedPage(cv31, cv32);
    }

    return {};
}

std::optional<SusiPageIndex> susiPageIndex(QJsonValue pointer)
{
    if (pointer.isDouble())
        return SusiPageIndex{static_cast<SusiPageIndex::value_type>(pointer.toInt())};

    return {};
}

QString pageAlias(ExtendedPageIndex pageIndex)
{
    static const auto s_pageAliases = [] {
        QHash<ExtendedPageIndex, QString> aliases;

        for (auto it = s_pageDefinitions->begin(); it != s_pageDefinitions->end(); ++it) {
            if (const auto page = extendedPageIndex(it->toObject()[s_pointer]))
                aliases.insert(page.value(), it.key());
        }

        return aliases;
    }();

    return s_pageAliases[pageIndex];
}

QString pageAlias(SusiPageIndex pageIndex)
{
    static const auto s_pageAliases = [] {
        QHash<SusiPageIndex, QString> aliases;

        for (auto it = s_pageDefinitions->begin(); it != s_pageDefinitions->end(); ++it) {
            if (const auto page = susiPageIndex(it->toObject()[s_pointer]))
                aliases.insert(page.value(), it.key());
        }

        return aliases;
    }();

    return s_pageAliases[pageIndex];
}

QJsonValue resolve(QJsonObject object, QString key)
{
    const auto value = object[key];

    if (const auto ref = value.toString(); ref.startsWith("$ref:"_L1))
        return s_decoderDefinitions->value(ref.mid(5));

    return value;
}

QString flagNames(QJsonArray flags, int value)
{
    QStringList names;

    for (int i = 0; i < flags.count(); ++i) {
        if (value & (1 << i))
            names += flags[i].toString();
    }

    return names.join(", "_L1);
}

} // namespace

DecoderField::DecoderField(QJsonArray fields, int index)
    : d{fields[index].toObject()}
    , m_offset{0}
{
    for (int i = 0; i < index; ++i)
        m_offset += fields[i].toObject()[s_width].toInt();
}

QString DecoderField::name() const
{
    return d[s_name].toString();
}

quint8 DecoderField::value(quint8 value) const
{
    if (const auto bcd = d.find(s_bcd); bcd != d.end())
        return (*bcd == 0 ? value : value / 10) % 10;

    const auto mask = (1 << width()) - 1;
    return static_cast<quint8>((value >> m_offset) & mask);
}

QString DecoderField::valueName(quint8 value) const
{
    if (const auto values = resolve(d, s_values).toArray(); value < values.count())
        return values[value].toString();
    if (const auto flags = resolve(d, s_flags).toArray(); !flags.isEmpty())
        return flagNames(flags, value);

    return {};
}

int DecoderField::width() const
{
    return d[s_width].toInt();
}

QJsonArray DecoderField::flags() const
{
    return d[s_flags].toArray();
}

bool DecoderField::hasFlags() const
{
    return d.contains(s_flags);
}

DecoderVariable::DecoderVariable(QJsonObject object)
    : d{std::move(object)}
{}

bool DecoderVariable::isValid() const
{
    return !d.isEmpty();
}

QString DecoderVariable::name() const
{
    return d[s_name].toString();
}

QString DecoderVariable::type() const
{
    return d[s_type].toString();
}

DecoderField DecoderVariable::field(int index) const
{
    return DecoderField{resolve(d, s_fields).toArray(), index};
}

qsizetype DecoderVariable::fieldCount() const
{
    return resolve(d, s_fields).toArray().count();
}

QJsonArray DecoderVariable::flags() const
{
    return resolve(d, s_flags).toArray();
}

qsizetype DecoderVariable::flagCount() const
{
    return flags().count();
}

QJsonValue DecoderVariable::values() const
{
    return resolve(d, s_values);
}

QString DecoderInfo::id(BaseType baseType)
{
    switch (baseType) {
    case BaseType::Identity:
        return "NMRA:Identity"_L1;
    case BaseType::Baseline:
        return "NMRA:Baseline"_L1;
    case BaseType::Functions:
        return "NMRA:Functions"_L1;
    case BaseType::Vehicle:
        return "NMRA:Vehicle"_L1;
    case BaseType::Railcom:
        return "RCN:217:RailCom"_L1;
    }

    return {};
}

bool DecoderInfo::isValid() const
{
    return !d.isEmpty();
}

QString DecoderInfo::id() const
{
    return m_id;
};

QString DecoderInfo::id(VariableValue vendorId, VariableValue decoderId)
{
    return QString::number(vendorId) + ':'_L1 + QString::number(decoderId);
}

QString DecoderInfo::id(DecoderId decoderId)
{
    return id(static_cast<VariableValue::value_type>(decoderId & 255),
              static_cast<VariableValue::value_type>(decoderId >> 8));
}

DecoderInfo::DecoderInfo(BaseType baseType)
    : DecoderInfo{id(baseType)}
{}

DecoderInfo::DecoderInfo(QString decoderId)
    : d{s_decoderDefinitions->value(decoderId).toObject()}
    , m_id{std::move(decoderId)}
{}

DecoderInfo::DecoderInfo(DecoderId decoderId)
    : DecoderInfo{id(decoderId)}
{}

DecoderInfo::DecoderInfo(VariableValue vendorId, VariableValue decoderId)
    : DecoderInfo{id(vendorId, decoderId)}
{}

QString DecoderInfo::name() const
{
    return d[s_name].toString();
}

bool DecoderInfo::hasRailcom() const
{
    return d[s_hasRailcom].toBool(true);
}

DecoderInfo DecoderInfo::parent() const
{
    return DecoderInfo{d[s_extends].toString()};
}

QSet<ExtendedVariableIndex> DecoderInfo::unsupportedVariableIds() const
{
    QSet<ExtendedVariableIndex> unsupportedIds;

    for (const auto value: d[s_unsupported].toArray())
        unsupportedIds.insert(static_cast<ExtendedVariableIndex::value_type>(value.toInt()));

    return unsupportedIds;
}

DecoderVariable DecoderInfo::variable(ExtendedVariableIndex index, VariableFilters filters) const
{
    if (filters.testFlag(NoUnsupported) && unsupportedVariableIds().contains(index))
        return {};

    const auto baseVariable = variableIndex(index);
    QStringList keyCandidates;

    if (range(VariableSpace::Extended).contains(baseVariable)) {
        const auto page = extendedPage(index);

        if (const auto alias = pageAlias(page); !alias.isEmpty()) {
            keyCandidates.append(alias + ':'_L1 + QString::number(baseVariable - 257));
            keyCandidates.append(alias + ':'_L1 + QString::number(baseVariable));
        }

        keyCandidates.append(QString::number(page) + ':'_L1 + QString::number(baseVariable));
    } else if (range(VariableSpace::Susi).contains(baseVariable)) {
        const auto page = susiPage(index);

        if (const auto alias = pageAlias(page); !alias.isEmpty()) {
            keyCandidates.append(alias + ':'_L1 + QString::number(baseVariable - 257));
            keyCandidates.append(alias + ':'_L1 + QString::number(baseVariable));
        }

        keyCandidates.append(QString::number(page) + ':'_L1 + QString::number(baseVariable));
    } else {
        if (index.value != baseVariable.value)
            keyCandidates.append(QString::number(index));

        keyCandidates.append(QString::number(baseVariable));
    }

    for (auto info = *this; info.isValid(); info = info.parent()) {
        const auto variables = info.d[s_variables].toObject();

        for (const auto &key: keyCandidates) {
            if (const auto variable = DecoderVariable{variables[key].toObject()}; variable.isValid())
                return variable;
        }

        if (filters & NoParent)
            break;
    }

    if (filters.testFlag(NoFallback))
        return {};

    const auto railcom = DecoderInfo{BaseType::Railcom};
    return railcom.variable(index, NoFallback);
}

QList<ExtendedVariableIndex> DecoderInfo::variableIds(VariableFilters filters) const
{
    const auto unsupported = unsupportedVariableIds();
    auto ids = QSet<ExtendedVariableIndex>{};

    for (auto info = *this; info.isValid(); info = info.parent()) {
        for (const auto &key: info.d[s_variables].toObject().keys()) {
            const auto prefix = key.left(qMax(0, key.indexOf(':'_L1)));
            const auto suffix = key.mid(prefix.length() ? prefix.length() + 1 : 0);
            const auto pointer = s_pageDefinitions->value(prefix)[s_pointer];

            bool hasIndex = false;
            auto variableIndex = static_cast<VariableIndex::value_type>(suffix.toInt(&hasIndex));

            if (const auto page = extendedPageIndex(pointer)) {
                if (!prefix.isEmpty())
                    variableIndex += 257;

                if (!filters.testFlag(NoUnsupported) || !unsupported.contains(variableIndex))
                    ids.insert(value(extendedVariable(variableIndex, page.value())));
            } else {
                if (!filters.testFlag(NoUnsupported) || !unsupported.contains(variableIndex))
                    ids.insert(variableIndex);
            }
        }

        if (filters.testFlag(NoParent))
            break;
    }

    auto sortedIds = ids.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    return sortedIds;

}

QStringList DecoderInfo::knownDecoderIds(DecoderFilters filters)
{
    QStringList decoderIds;

    for (const auto &key: s_decoderDefinitions->keys()) {
        const auto info = s_decoderDefinitions->value(key).toObject();

        if (info.contains(s_variables)
                || (!filters.testFlag(NoAliases) && info.contains(s_extends)))
            decoderIds.append(key);
    }

    return decoderIds;
}

QString DecoderInfo::vendorName(VendorId vendorId)
{
    return s_manufacturerNames->value(vendorId);
}

} // namespace lmrs::core::dcc
