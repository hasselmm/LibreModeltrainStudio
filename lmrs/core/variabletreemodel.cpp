#include "variabletreemodel.h"

#include <lmrs/core/decoderinfo.h>
#include <lmrs/core/userliterals.h>

#include <QJsonArray>

namespace lmrs::core {

using namespace dcc;

namespace {

QString toBinary(int value, int width) // FIXME: unify with lmrs::widgets::SpinBox
{
    auto text = (QString{width, '0'_L1} + QString::number(value, 2)).right(width);

    // separate into groups of four bits
    for (auto i = text.length() - 4; i > 0; i -= 4)
        text.insert(i, ' '_L1);

    return text;
}

bool isSpecificType(QString type, QString prefix)
{
    if (type.startsWith(prefix)) {
        if (type.length() == prefix.length())
            return true;
        if (type.at(prefix.length()) == ':'_L1)
            return true;
    }

    return false;
}

bool isCommonType(QString type, QString suffix)
{
    if (type.endsWith(suffix)) {
        if (type.length() == suffix.length())
            return true;
        if (type.at(type.length() - suffix.length() - 1) == ':'_L1)
            return true;
    }

    return false;
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

quintptr internalId(const QModelIndex &parent)
{
    if (!parent.isValid()) {
        return 0;
    } else if (const auto grandParent = parent.parent(); !grandParent.isValid()) {
        return (static_cast<uint>(parent.row()) << 2) | 1;
    } else {
        return (static_cast<uint>(parent.row()) << 10)
                | (static_cast<uint>(grandParent.row()) << 2) | 2;
    }
}

} // namespace

void VariableModel::updateVariable(ExtendedVariableIndex variable, VariableValue value)
{
    const auto sameOrNext = [variable](const auto &row) { return row.variable >= variable; };

    auto hasVariableIndex = [](ExtendedVariableIndex variable) {
        return [variable](const auto &row) { return row.variable == variable; };
    };

    auto it = std::find_if(m_rows.begin(), m_rows.end(), sameOrNext);
    auto insertingRow = false;
    auto upperByteRow = -1;

    if (it == m_rows.end() || it->variable != variable) {
        // initialize flags via introspection
        const auto info = variableInfo(variable);
        const auto type = info.type();

        auto flags = Row::Flags{};

        if (const auto fieldCount = info.fieldCount(); fieldCount > 0) {
            flags |= Row::HasFields;

            for (auto i = 0; i < fieldCount; ++i) {
                if (info.field(i).hasFlags()) {
                    flags |= Row::HasFieldsWithFlags;
                    break;
                }
            }
        }

        if (info.flagCount() > 0)
            flags |= Row::HasFlags;

        if (isCommonType(type, "U16H"_L1))
            flags |= Row::IsHighByte;
        else if (isCommonType(type, "U16L"_L1))
            flags |= Row::IsLowByte;
        else if (isCommonType(type, "U32A"_L1))
            flags |= Row::IsByteA;
        else if (isCommonType(type, "U32B"_L1))
            flags |= Row::IsByteB;
        else if (isCommonType(type, "U32C"_L1))
            flags |= Row::IsByteC;
        else if (isCommonType(type, "U32D"_L1))
            flags |= Row::IsByteD;

        // insert new variable
        const auto rowNumber = static_cast<int>(it - m_rows.begin());
        beginInsertRows({}, rowNumber, rowNumber);
        it = m_rows.insert(it, {variable, flags});
        insertingRow = true;
    }

    // store new value
    it->value8 = value;

    if (it->flags & Row::IsHighByte) {
        // this variable is an upper byte
        it->value16 = static_cast<quint16>(value << 8);

        // ... merge with lower byte if known
        const auto lowerByte = hasVariableIndex(variable);
        if (const auto lo = std::find_if(m_rows.begin(), m_rows.end(), lowerByte); lo != m_rows.end()) {
            it->flags.setFlag(Row::HasValue16);
            it->value16 |= static_cast<quint16>(lo->value8);
        }
    } else if (it->flags & Row::IsLowByte) {
        // this variable is a lower byte, update upper byte if known
        const auto upperByte = hasVariableIndex(variable - 1);
        if (const auto up = std::find_if(m_rows.begin(), m_rows.end(), upperByte); up != m_rows.end()) {
            up->value16 = static_cast<quint16>((up->value8 << 8) | value);
            up->flags.setFlag(Row::HasValue16);
            upperByteRow = static_cast<int>(up - m_rows.begin());
        }
    }

    if (it->flags & Row::IsByteA) {
        const auto itB = std::find_if(it,  m_rows.end(), hasVariableIndex(variable + 1));
        const auto itC = std::find_if(itB, m_rows.end(), hasVariableIndex(variable + 2));
        const auto itD = std::find_if(itC, m_rows.end(), hasVariableIndex(variable + 3));

        if (itB != m_rows.end() && itC != m_rows.end() && itD != m_rows.end()) {
            it->value32
                    = static_cast<quint32>(it ->value8 <<  0)
                    | static_cast<quint32>(itB->value8 <<  8)
                    | static_cast<quint32>(itC->value8 << 16)
                    | static_cast<quint32>(itD->value8 << 24);
            it->flags.setFlag(Row::HasValue32);
        }
    }

    if (insertingRow)
        endInsertRows();
    else
        emitDataChanges(static_cast<int>(it - m_rows.begin()));

    if (upperByteRow >= 0)
        emitDataChanges(upperByteRow);

    emit variableChanged(variable, value);
}

VariableValue VariableModel::variableValue(ExtendedVariableIndex variable) const
{
    const auto byIndex = [variable](const auto &row) { return row.variable == variable; };
    if (const auto it = std::find_if(m_rows.begin(), m_rows.end(), byIndex); it != m_rows.end())
        return it->value8;

    return 0;
}

void VariableModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

QModelIndex VariableModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column, internalId(parent));
}

QModelIndex VariableModel::parent(const QModelIndex &child) const
{
    if (const auto type = child.internalId() & 3; type == 1) {
        return index(static_cast<int>(child.internalId() >> 2), Column::Value, {});
    } else if (type == 2) {
        const auto parent = index(static_cast<int>(child.internalId() >> 10), Column::Value, {});
        return index((child.internalId() >> 2) & 255, Column::Value, parent);
    } else {
        return {};
    }
}

int VariableModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return static_cast<int>(m_rows.size());
    } else if (const auto grandParent = parent.parent(); !grandParent.isValid()) {
        const auto &row = m_rows[parent.row()];
        auto childCount = 0_size;

        if (row.flags) {
            const auto variable = variableInfo(row.variable);

            if (row.flags & Row::HasFields)
                childCount += variable.fieldCount();
            if (row.flags & Row::HasFlags)
                childCount += variable.flags().size();
        }

        return static_cast<int>(childCount);
    } else {
        const auto &row = m_rows[grandParent.row()];

        if (row.flags & Row::HasFields) {
            const auto variable = variableInfo(row.variable);
            const auto field = variable.field(parent.row());
            return static_cast<int>(field.flags().count());
        }

        return 0;
    }
}

int VariableModel::columnCount(const QModelIndex &) const
{
    return 5;
}

bool VariableModel::hasChildren(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return true;
    else if (const auto grandParent = parent.parent(); !grandParent.isValid())
        return m_rows[parent.row()].flags & Row::HasChildren;
    else if (const auto grandGrandParent = grandParent.parent(); !grandGrandParent.isValid())
        return m_rows[parent.row()].flags & Row::HasGrandChildren;
    else
        return false;
}

QVariant VariableModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        if (const auto parent = index.parent(); !parent.isValid())
            return m_rows[index.row()].data(index, role);
        else if (const auto grandParent = parent.parent(); !grandParent.isValid())
            return m_rows[parent.row()].childData(index, role);
        else
            return m_rows[grandParent.row()].grandChildData(index, role);
    } else {
        qInfo() << index;
    }

    return {};
}

QVariant VariableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<Column>(section)) {
        case Column::Index:
            return tr("Variable");
        case Column::Name:
            return tr("Name");
        case Column::Bitmask:
            return tr("Bits");
        case Column::Value:
            return tr("Value");
        case Column::Description:
            return tr("Description");
        }
    }

    return {};
}

QString VariableModel::decoderId() const
{
    return DecoderInfo::id(variableValue(8), variableValue(7));
}

QList<ExtendedVariableIndex> VariableModel::knownVariables() const
{
    if (const auto variables = DecoderInfo{decoderId()}.variableIds(DecoderInfo::NoUnsupported); !variables.isEmpty())
        return variables;

    return DecoderInfo{DecoderInfo::Vehicle}.variableIds();
}

DecoderVariable VariableModel::variableInfo(ExtendedVariableIndex index) const
{
    for (const auto &baseType: {DecoderInfo{decoderId()}, DecoderInfo{DecoderInfo::Vehicle}}) {
        for (auto type = baseType; type.isValid(); type = type.parent()) {
            if (const auto variable = type.variable(index); variable.isValid())
                return variable;
        }
    }

    return {};
}

void VariableModel::emitDataChanges(int row)
{
    const auto modelIndex = index(row, Column::Index, {});

    if (const auto count = rowCount(modelIndex); count > 0) {
        emit dataChanged(index(0, Column::Bitmask, modelIndex),
                         index(count - 1, Column::Description, modelIndex),
                         {Qt::CheckStateRole, Qt::DisplayRole, Qt::UserRole});
    }

    emit dataChanged(index(row, Column::Bitmask, {}),
                     index(row, Column::Description, {}),
                     {Qt::DisplayRole, Qt::UserRole});
}

QVariant VariableModel::Row::data(QModelIndex index, int role) const
{
    const auto model = static_cast<const VariableModel *>(index.model());

    switch (static_cast<Column>(index.column())) {
    case Column::Index:
        if (role == Qt::DisplayRole)
            return dcc::variableString(variable);
        else if (role == Qt::UserRole)
            return QVariant::fromValue(variable);

        break;
    case Column::Name:
        if (role == Qt::DisplayRole)
            return model->variableInfo(variable).name();

        break;

    case Column::Bitmask:
        if (role == Qt::DisplayRole)
            return toBinary(value8, 8);
        else if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);

        break;

    case Column::Value:
        if (role == Qt::DisplayRole || role == Qt::UserRole)
            return value8;
        else if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);

        break;

    case Column::Description:
        if (role == Qt::DisplayRole)
            return description(model->variableInfo(variable));

        break;
    }

    return {};
}

QVariant VariableModel::Row::childData(QModelIndex index, int role) const
{
    const auto model = static_cast<const VariableModel *>(index.model());
    const auto info = model->variableInfo(variable);

    auto row = index.row();

    if (flags & HasFlags) {
        const auto flags = info.flags();

        if (row < flags.count())
            return flagData(index, role, std::move(flags), value8);

        row -= static_cast<int>(flags.count());
    }

    if (flags & HasFields) {
        if (row < info.fieldCount())
            return fieldData(index, role, std::move(info));
    }

    return {};
}

QVariant VariableModel::Row::grandChildData(QModelIndex index, int role) const
{
    const auto model = static_cast<const VariableModel *>(index.model());
    const auto info = model->variableInfo(variable);

    const auto field = info.field(index.parent().row());
    if (const auto flags = field.flags(); !flags.isEmpty())
        return flagData(index, role, std::move(flags), field.value(value8));

    return {};
}

QVariant VariableModel::Row::flagData(QModelIndex index, int role, QJsonArray flags, VariableValue value)
{
    switch (static_cast<Column>(index.column())) {
    case Column::Index:
        if (role == Qt::DisplayRole)
            return tr("Bit %1").arg(index.row());

        break;

    case Column::Name:
        if (role == Qt::DisplayRole)
            return flags[index.row()].toString();

        break;

    case Column::Bitmask:
        if (role == Qt::DisplayRole)
            return toBinary(1 << index.row(), 8);
        else if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);

        break;

    case Column::Value:
        if (role == Qt::CheckStateRole)
            return value & (1 << index.row()) ? Qt::Checked : Qt::Unchecked;
        else if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);

        break;

    case Column::Description:
        break;
    }

    return {};
}

QVariant VariableModel::Row::fieldData(QModelIndex index, int role, DecoderVariable variable) const
{
    const auto field = variable.field(index.row());

    switch (static_cast<Column>(index.column())) {
    case Column::Index:
        if (role == Qt::DisplayRole)
            return tr("Field %1").arg(index.row());

        break;
    case Column::Name:
        if (role == Qt::DisplayRole)
            return field.name();

        break;

    case Column::Bitmask:
        if (role == Qt::DisplayRole)
            return toBinary(field.value(value8), field.width());
        else if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);

        break;

    case Column::Value:
        if (role == Qt::DisplayRole || role == Qt::UserRole)
            return field.value(value8);
        else if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);

        break;

    case Column::Description:
        if (role == Qt::DisplayRole)
            return field.valueName(field.value(value8));

        break;
    }

    return {};
}

QString VariableModel::Row::description(DecoderVariable variable) const
{
    // FIXME: move to DecoderVariable: problem the actualVariable thing
    const auto type = variable.type();
    const auto actualValue = isCommonType(type, "U16H"_L1) ? value16 : value8;

    if (const auto values = variable.values(); values.isArray()) {
        return values.toArray().at(actualValue).toString();
    } else if (values.isObject()) {
        return values.toObject().value(QString::number(actualValue)).toString();
    } else if (const auto names = variable.flags(); !names.isEmpty()) {
        return flagNames(names, actualValue);
    } else if (!type.isEmpty()) {
        if (isSpecificType(type, "NMRA:ManufacturerId"_L1)) {
            return DecoderInfo::vendorName(value8);
        } else if (flags.testFlag(HasValue16)) {
            if (isSpecificType(type, "NMRA:DecoderType"_L1))
                return DecoderInfo{value16}.name();
            else if (isSpecificType(type, "NMRA:DecoderLock:U16H"_L1))
                return value8 == (value16 & 255) ? tr("Unlocked") : tr("Locked");
            else if (isSpecificType(type, "NMRA:ExtendedAddress:U16H"_L1))
                return QString::number(value16 & 0x3fff);
            else if (isCommonType(type, "U16H"_L1))
                return QString::number(value16);
        } else if (flags.testFlag(HasValue32)) {
            if (isCommonType(type, "RCN:DateTime:U32A"_L1)) {
                static const auto s_epochOffset = QDateTime{QDate{2000, 1, 1}, QTime{}, Qt::UTC}.toSecsSinceEpoch();
                return QDateTime::fromSecsSinceEpoch(value32 + s_epochOffset).toString(Qt::ISODate);
            } else if (isCommonType(type, "U32A"_L1)) {
                return QString::number(value32) + " (0x"_L1 + QString::number(value32, 16) + ')'_L1;
            }
        }
    }

    return {};
}

} // namespace lmrs::core
