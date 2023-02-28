#include "functionmappingmodel.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>
#include <lmrs/core/fileformat.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/core/dccconstants.h>

#include <QFile>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QTextStream>

#include <iterator>

QDebug operator<<(QDebug debug, lmrs::core::dcc::ExtendedVariableIndex variable)
{
    const auto prettyPrinter = lmrs::core::PrettyPrinter<decltype(variable)>{debug};

    if (debug.verbosity() > QDebug::DefaultVerbosity) {
        debug << "cv31=" << cv31(variable)
              << ", cv32=" << cv32(variable)
              << ", cv=" << variableIndex(variable);
    } else {
        debug << variableIndex(variable) << '['
              << cv31(variable) << '/'
              << cv32(variable) << ']';
    }

    return debug;
}

namespace lmrs::esu {

using namespace core::dcc;

namespace {

// In a map the indices are sorted. This makes it easier to generate output that's grouped by page.
using VariableValueMap = QMap<ExtendedVariableIndex, VariableValue>;

template<core::EnumType T>
[[nodiscard]] QString displayString(T value)
{
    static const auto metaEnum = QMetaEnum::fromType<T>();

    if (const auto key = metaEnum.valueToKey(static_cast<int>(value)))
        return QString::fromLatin1(key);

    return QString::fromLatin1(metaEnum.enumName()) + '('_L1 + QString::number(core::value(value)) + ')'_L1;
}

[[nodiscard]] QString displayString(Condition::Variable variable)
{
    switch (variable) {
    case Condition::Driving:
        return FunctionMappingModel::tr("Driving");
    case Condition::Direction:
        return FunctionMappingModel::tr("Direction");
    case Condition::WheelSensor:
        return FunctionMappingModel::tr("Wheel Sensor");
    case Condition::Unused:
        break;

    case Condition::Function0:
    case Condition::Function1:
    case Condition::Function2:
    case Condition::Function3:
    case Condition::Function4:
    case Condition::Function5:
    case Condition::Function6:
    case Condition::Function7:
    case Condition::Function8:
    case Condition::Function9:
    case Condition::Function10:
    case Condition::Function11:
    case Condition::Function12:
    case Condition::Function13:
    case Condition::Function14:
    case Condition::Function15:
    case Condition::Function16:
    case Condition::Function17:
    case Condition::Function18:
    case Condition::Function19:
    case Condition::Function20:
    case Condition::Function21:
    case Condition::Function22:
    case Condition::Function23:
    case Condition::Function24:
    case Condition::Function25:
    case Condition::Function26:
    case Condition::Function27:
    case Condition::Function28:
    case Condition::Function29:
    case Condition::Function30:
    case Condition::Function31:
        return FunctionMappingModel::tr("F%1").arg(variable - Condition::Function0);

    case Condition::Sensor1:
    case Condition::Sensor2:
    case Condition::Sensor3:
    case Condition::Sensor4:
        return FunctionMappingModel::tr("Sensor %1").arg(variable - Condition::Sensor1 + 1);
    }

    return {};
}

[[nodiscard]] QString displayString(const Condition &condition)
{
    if (condition.variable == Condition::Direction) {
        switch (condition.state) {
        case Condition::Forward:
            return FunctionMappingModel::tr("Forward");
        case Condition::Reverse:
            return FunctionMappingModel::tr("Reverse");
        case Condition::Ignore:
            break;
        }
    } else if (condition.variable != Condition::Unused) {
        switch (condition.state) {
        case Condition::Enabled:
            return displayString(condition.variable);
        case Condition::Disabled:
            return FunctionMappingModel::tr("not %1").arg(displayString(condition.variable));
        case Condition::Ignore:
            break;
        }
    }

    return {};
}

using std::begin;

template <class Container, std::forward_iterator = decltype(begin(Container{}))>
[[nodiscard]] QString displayString(const Container &container)
{
    auto result = QString{};

    for (const auto &value: container) {
        if (!result.isEmpty())
            result += ", "_L1;

        result += displayString(value);
    }

    return result;
}

[[nodiscard]] auto variableValue(const QList<Condition> &conditions, int offset)
{
    const auto first = offset, last = offset + 4;
    auto result = core::dcc::VariableValue{};

    for (const auto &c: conditions) {
        if (c.variable >= first && c.variable < last)
            result |= static_cast<quint8>(c.state << ((c.variable % 4) * 2));
    }

    return result;
}

template<core::FlagsType T>
[[nodiscard]] auto variableValue(T flags, int offset)
{
    return core::dcc::VariableValue{static_cast<quint8>((flags >> offset) & 255)};
}

class Columns
{
public:
    explicit Columns(ExtendedVariableIndex base, char first, char last)
        : m_first{first - 'A'}
        , m_size{last - first + 1}
        , m_base{base}
    {}

    class const_iterator
    {
    private:
        constexpr auto fields() const { return std::tie(m_range, m_index); }

    public:
        constexpr const_iterator(const Columns *range, int index)
            : m_range{range}
            , m_index{index}
        {}

        constexpr auto index() const { return m_index; }
        constexpr auto page() const { return extendedPage(m_range->m_base); }
        constexpr auto offset() const { return static_cast<quint16>(m_range->m_first + m_index); }
        constexpr auto variable() const { return extendedVariable(variableIndex(m_range->m_base) + static_cast<quint16>(m_index), page()); }
        constexpr operator auto() const { return variable(); }

        auto &operator++() { ++m_index; return *this; }
        auto operator*() const { return *this; }

        constexpr auto operator==(const const_iterator &rhs) const { return fields() == rhs.fields(); }
        constexpr auto operator!=(const const_iterator &rhs) const { return fields() != rhs.fields(); }

    private:
        const Columns *m_range;
        int m_index;
    };

    constexpr auto size() const { return m_size; }
    constexpr auto begin() const { return const_iterator{this, 0}; }
    constexpr auto end() const { return const_iterator{this, size()}; }

private:
    int m_first;
    int m_size;

    ExtendedVariableIndex m_base;
};

constexpr auto functionMappingConditions = core::Range<ExtendedVariableIndex>{extendedVariable(257, 16, 3), extendedVariable(378, 16, 7)};
constexpr auto functionMappingOperations = core::Range<ExtendedVariableIndex>{extendedVariable(257, 16, 8), extendedVariable(378, 16, 12)};

constexpr auto mappingBase(int row) { return VariableIndex{static_cast<quint16>((row * 16) % 256 + 257)}; }

constexpr auto conditionsPage(int row) { return extendedPage(16, static_cast<quint8>(row / 16 + 3)); }
constexpr auto conditionsBase(int row) { return extendedVariable(mappingBase(row), conditionsPage(row)); }

constexpr auto operationsPage(int row) { return extendedPage(16, static_cast<quint8>(row / 16 + 8)); }
constexpr auto outputsBase(int row) { return extendedVariable(mappingBase(row), operationsPage(row)); }
constexpr auto effectsBase(int row) { return outputsBase(row) + 3; }
constexpr auto soundsBase(int row) { return effectsBase(row) + 3; }

static_assert(conditionsBase(0) == functionMappingConditions.first);
static_assert(outputsBase(0) == functionMappingOperations.first);

static_assert(conditionsBase(1) == extendedVariable(273, 16, 3));
static_assert(outputsBase(1) == extendedVariable(273, 16, 8));
static_assert(effectsBase(1) == extendedVariable(276, 16, 8));
static_assert(soundsBase(1) == extendedVariable(279, 16, 8));

static_assert(conditionsBase(71) == extendedVariable(369, 16, 7));
static_assert(outputsBase(71) == extendedVariable(369, 16, 12));
static_assert(effectsBase(71) == extendedVariable(372, 16, 12));
static_assert(soundsBase(71) == extendedVariable(375, 16, 12));

[[nodiscard]] auto variablesFromMappings(QList<Mapping> mappings)
{
    auto variables = VariableValueMap{};

    for (auto row = 0; row < Mapping::MaximumCount; ++row) {

        const auto mapping = row < mappings.size() ? mappings[row] : Mapping{};

        for (const auto &cv: Columns{conditionsBase(row), 'A', 'J'})
            variables[cv] = variableValue(mapping.conditions, cv.index() * 4);
        for (const auto &cv: Columns{outputsBase(row), 'K', 'M'})
            variables[cv] = variableValue(mapping.outputs, cv.index() * 8);
        for (const auto &cv: Columns{effectsBase(row), 'N', 'P'})
            variables[cv] = variableValue(mapping.effects, cv.index() * 8);
        for (const auto &cv: Columns{soundsBase(row), 'Q', 'T'})
            variables[cv] = variableValue(mapping.sounds, cv.index() * 8);
    }

    return variables;
}

[[nodiscard]] auto mappingsFromVariables(VariableValueMap variables)
{
    auto mappings = QList<Mapping>{};
    mappings.reserve(Mapping::MaximumCount);
    mappings.fill({}, Mapping::MaximumCount);

    for (auto row = 0; row < Mapping::MaximumCount; ++row) {
        for (const auto &cv: Columns{conditionsBase(row), 'A', 'J'}) {
            const auto value = variables.value(cv);

            for (auto i = 0; i < 4; ++i) {
                const auto state = static_cast<Condition::State>((value >> (i * 2)) & 3);

                if (state != Condition::State::Ignore) {
                    const auto variable = static_cast<Condition::Variable>(cv.index() * 4 + i);
                    mappings[row].conditions.emplace_back(variable, state);
                }
            }
        }

        for (const auto &cv: Columns{outputsBase(row), 'K', 'M'})
            mappings[row].outputs |= Outputs{static_cast<int>(variables.value(cv)) << (cv.index() * 8)};
        for (const auto &cv: Columns{effectsBase(row), 'N', 'P'})
            mappings[row].effects |= Effects{static_cast<int>(variables.value(cv)) << (cv.index() * 8)};
        for (const auto &cv: Columns{soundsBase(row), 'Q', 'T'})
            mappings[row].sounds |= Sounds{static_cast<int>(variables.value(cv)) << (cv.index() * 8)};
    }

    while (!mappings.isEmpty()
           && mappings.last().isEmpty())
        mappings.removeLast();

    return mappings;
}

} // namespace

class FunctionMappingModel::Private : public core::PrivateObject<FunctionMappingModel>
{
public:
    using PrivateObject::PrivateObject;

    FunctionMappingModel *model() const { return core::checked_cast<FunctionMappingModel *>(parent()); }

    QString displayString(QModelIndex index) const;
    QVariant columnData(QModelIndex index) const;

    void fillLokPilot5(int outputCount);
    void fillLokPilot5Fx(int outputCount);
    void fillLokSound5();

    void reportError(QString errorString);

    QList<Mapping> m_rows;
    QString m_errorString;
};

FunctionMappingModel::FunctionMappingModel(QObject *parent)
    : QAbstractTableModel{parent}
    , d{new Private{this}}
{
    reset(Preset::Lp5);
}

FunctionMappingModel::FunctionMappingModel(VariableValueMap variables, QObject *parent)
    : FunctionMappingModel{parent}
{
    d->m_rows = mappingsFromVariables(std::move(variables));
}

void FunctionMappingModel::reset(Preset preset)
{
    beginResetModel();
    d->m_rows.clear();

    switch (preset) {
    case Preset::Empty:
        break;

    case Preset::Lp5:
        d->fillLokPilot5(12);
        break;

    case Preset::Lp5Micro:
        d->fillLokPilot5(4);
        break;

    case Preset::Lp5MicroN18:
        d->fillLokPilot5(7);
        break;

    case Preset::Lp5Fx:
        d->fillLokPilot5Fx(12);
        break;

    case Preset::Lp5FxMicro:
        d->fillLokPilot5Fx(6);
        break;

    case Preset::Ls5:
        d->fillLokSound5();
        break;
    }

    while (d->m_rows.size() < Mapping::MaximumCount)
        d->m_rows += Mapping{};

    endResetModel();
}

FunctionMappingModel::VariableValueMap FunctionMappingModel::variables() const
{
    return variablesFromMappings(d->m_rows);
}

constexpr std::optional<int> extendedVariableOffset(core::Range<ExtendedVariableIndex> range, ExtendedVariableIndex variable)
{
    if (range.contains(variable)) {
        const auto page = extendedPage(variable) - extendedPage(range.first);
        return page * 256 + variableIndex(variable - 1) % 256;
    }

    return {};
}

QString description(ExtendedVariableIndex variable)
{
    if (const auto offset = extendedVariableOffset(functionMappingConditions, variable)) {
        const auto column = static_cast<char>(offset.value() % 16 + 'A');
        const auto row = offset.value() / 16 + 1;

        if (column <= 'J')
            return FunctionMappingModel::tr("ESU function mapping row %1, column %2 - conditions").arg(row).arg(column);
    } else if (const auto offset = extendedVariableOffset(functionMappingOperations, variable)) {
        const auto column = static_cast<char>(offset.value() % 16 + 'K');
        const auto row = offset.value() / 16 + 1;

        if (column <= 'M')
            return FunctionMappingModel::tr("ESU function mapping row %1, column %2 - outputs").arg(row).arg(column);
        if (column <= 'P')
            return FunctionMappingModel::tr("ESU function mapping row %1, column %2 - logic").arg(row).arg(column);
        if (column <= 'T')
            return FunctionMappingModel::tr("ESU function mapping row %1, column %2 - sound").arg(row).arg(column);
    }

    return {};
}

class TextFileReaderBase : public FunctionMappingReader
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    using FunctionMappingReader::FunctionMappingReader;

protected:
    bool readVariable(QString variableText, QString valueText);

    ExtendedPageIndex page;
    VariableValueMap variables;
    int lineNumber = 1;
};

class DelimiterSeparatedFileReader : public TextFileReaderBase
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    explicit DelimiterSeparatedFileReader(QString fileName, QChar delimiter = ';'_L1)
        : TextFileReaderBase{std::move(fileName)}
        , m_delimiter{delimiter}
    {}

    ModelPointer read() override;

private:
    QChar m_delimiter;
};

class PlainTextFileReader : public TextFileReaderBase
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    using TextFileReaderBase::TextFileReaderBase;

    ModelPointer read() override;
};

class EsuxFileReader : public FunctionMappingReader
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    using FunctionMappingReader::FunctionMappingReader;

    ModelPointer read() override
    {
        reportError(tr("Not implemented yet"));
        return nullptr;
    }
};

class DelimiterSeparatedFileWriter : public FunctionMappingWriter
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    explicit DelimiterSeparatedFileWriter(QString fileName, QChar delimiter = ';'_L1)
        : FunctionMappingWriter{std::move(fileName)}
        , m_delimiter{delimiter}
    {}

    bool write(const FunctionMappingModel *model) override;

private:
    QChar m_delimiter;
};

class PlainTextFileWriter : public FunctionMappingWriter
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    using FunctionMappingWriter::FunctionMappingWriter;

    bool write(const FunctionMappingModel *model) override;
};

bool TextFileReaderBase::readVariable(QString variableText, QString valueText)
{
    const auto variable = core::parse<VariableIndex>(variableText);
    const auto value = core::parse<VariableValue>(valueText);

    if (!variable.has_value()) {
        reportError(tr("Bad variable index in line %1.").arg(lineNumber));
        return false;
    }

    if (!value.has_value()) {
        reportError(tr("Bad value in line %1.").arg(lineNumber));
        return false;
    }

    if (*variable == 31)
        page = extendedPage(*value, cv32(page));
    else if (*variable == 32)
        page = extendedPage(cv31(page), *value);
    else
        variables[extendedVariable(*variable, page)] = *value;

    return true;
}

DelimiterSeparatedFileReader::ModelPointer DelimiterSeparatedFileReader::read()
{
    auto file = QFile{fileName()};

    if (!file.open(QFile::ReadOnly)) {
        reportError(tr("Could not open file for reading: %1").arg(file.errorString()));
        return nullptr;
    }

    auto stream = QTextStream{&file};

    const auto splitter = QRegularExpression{"\\s*"_L1 + QRegularExpression::escape(m_delimiter) + "\\s*"_L1};
    const auto header = stream.readLine().trimmed().toLower().split(splitter);
    const auto variableColumn = header.indexOf("cv"_L1);
    const auto valueColumn = header.indexOf("value"_L1);

    if (variableColumn < 0 || valueColumn < 0) {
        reportError(tr("Could not find required columns in file header."));
        return nullptr;
    }

    for (lineNumber = 2; !stream.atEnd(); ++lineNumber) {
        const auto line = stream.readLine().trimmed().toLower();

        if (line.isEmpty())
            continue;

        const auto columns = line.split(splitter);

        if (variableColumn >= columns.count() || valueColumn >= columns.count()) {
            reportError(tr("Unexpected number of columns in line %1.").arg(lineNumber));
            return nullptr;
        }

        if (!readVariable(columns[variableColumn], columns[valueColumn]))
            return nullptr;
    }

    return std::make_unique<esu::FunctionMappingModel>(std::move(variables));
}

PlainTextFileReader::ModelPointer PlainTextFileReader::read()
{
    auto file = QFile{fileName()};

    if (!file.open(QFile::ReadOnly)) {
        reportError(tr("Could not open file for reading: %1").arg(file.errorString()));
        return nullptr;
    }

    const auto pattern = QRegularExpression{R"(cv\s*(?<variable>\d+)\s*=\s*(?<value>\d+)\s*(?<continue>,\s*)?)"_L1};

    auto stream = QTextStream{&file};

    for (; !stream.atEnd(); ++lineNumber) {
        const auto line = stream.readLine().trimmed().toLower();

        for (auto offset = qsizetype{0};; ) {
            const auto result = pattern.match(line, offset);

            if (!result.hasMatch())
                break;

            if (!readVariable(result.captured("variable"_L1), result.captured("value"_L1)))
                return nullptr;

            if (result.captured("continue"_L1).isEmpty())
                break;

            offset += result.capturedLength();
        }
    }

    return std::make_unique<esu::FunctionMappingModel>(std::move(variables));
}

bool DelimiterSeparatedFileWriter::write(const FunctionMappingModel *model)
{
    auto file = QFile{std::move(fileName())};

    if (!file.open(QFile::WriteOnly)) {
        reportError(tr("Could not open file for writing: %1").arg(file.errorString()));
        return false;
    }

    auto stream = QTextStream{&file};
    auto currentPage = ExtendedPageIndex{0};
    const auto variables = model->variables();

    stream << "CV" << m_delimiter << "Value" << m_delimiter << "Description" << Qt::endl;

    for (auto it = variables.begin(); it != variables.end(); ++it) {
        const auto page = extendedPage(it.key());

        if (std::exchange(currentPage, page) != page) {
            stream << "31" << m_delimiter << cv31(page) << m_delimiter << tr("switch extended page") << Qt::endl;
            stream << "32" << m_delimiter << cv32(page) << m_delimiter << Qt::endl;
        }

        stream << variableIndex(it.key()) << m_delimiter << it.value() << m_delimiter << description(it.key()) << Qt::endl;
    }

    return true;
}

bool PlainTextFileWriter::write(const FunctionMappingModel *model)
{
    auto file = QFile{fileName()};

    if (!file.open(QFile::WriteOnly)) {
        reportError(tr("Could not open file for writing: %1").arg(file.errorString()));
        return false;
    }

    auto stream = QTextStream{&file};
    auto currentPage = ExtendedPageIndex{0};
    const auto variables = model->variables();

    for (auto it = variables.begin(); it != variables.end(); ++it) {
        const auto page = extendedPage(it.key());

        if (std::exchange(currentPage, page) != page) {
            if (it != variables.begin())
                stream << Qt::endl;

            stream << "CV31 = " << cv31(page) << ", CV32 = " << cv32(page) << Qt::endl;
            stream << QString{22, '-'_L1} << Qt::endl;
        }

        const auto valueStr = QString::number(it.value()).rightJustified(3);
        stream << "CV" << variableIndex(it.key()) << " = " << valueStr;

        if (const auto hint = description(it.key()); !hint.isEmpty())
            stream << " (" << hint << ")";

        stream << Qt::endl;
    }

    return true;
}

QString FunctionMappingModel::errorString() const
{
    return d->m_errorString; // FIXME: extract features that need an error string
}

void FunctionMappingModel::read(core::VariableControl *variableControl, core::dcc::VehicleAddress address)
{
    // FIXME: this should be a constexpr range
    auto variables = variablesFromMappings(d->m_rows).keys();
    qInfo().verbosity(QDebug::MinimumVerbosity) << variables;

    variableControl->readExtendedVariables(address, variables, [](auto variable, auto result) {
        qInfo() << Q_FUNC_INFO << variable << result;
        return core::Continuation::Abort;
    });
    }

void FunctionMappingModel::write(core::VariableControl */*variableControl*/, core::dcc::VehicleAddress /*address*/)
{
    const auto variables = variablesFromMappings(d->m_rows);
    qInfo().verbosity(QDebug::MinimumVerbosity) << variables;

    const auto mappings = mappingsFromVariables(variables);
    qInfo() << (mappings.size() == d->m_rows.size())
            << (mappings == d->m_rows);

    d->reportError(tr("Not implemented"));
}

int FunctionMappingModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return qMin(static_cast<int>(d->m_rows.size()), Mapping::MaximumCount);
}

int FunctionMappingModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return QMetaEnum::fromType<DataColumn>().keyCount();
}

QString FunctionMappingModel::Private::displayString(QModelIndex index) const
{
    switch (static_cast<DataColumn>(index.column())) {
    case ConditionColumn:
        return esu::displayString(m_rows[index.row()].conditions);

    case OutputsColumn:
        return esu::displayString(m_rows[index.row()].outputs);

    case EffectsColumn:
        return esu::displayString(m_rows[index.row()].effects);

    case SoundsColumn:
        return esu::displayString(m_rows[index.row()].sounds);
    }

    return {};
}

QVariant FunctionMappingModel::Private::columnData(QModelIndex index) const
{
    switch (static_cast<DataColumn>(index.column())) {
    case ConditionColumn:
        return QVariant::fromValue(m_rows[index.row()].conditions);

    case OutputsColumn:
        return QVariant::fromValue(m_rows[index.row()].outputs);

    case EffectsColumn:
        return QVariant::fromValue(m_rows[index.row()].effects);

    case SoundsColumn:
        return QVariant::fromValue(m_rows[index.row()].sounds);
    }

    return {};
}

QVariant FunctionMappingModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (static_cast<DataRole>(role)) {
        case DisplayRole:
            return d->displayString(index);

        case ColumnDataRole:
            return d->columnData(index);

        case MappingRole:
            return QVariant::fromValue(d->m_rows[index.row()]);

        case ConditionsRole:
            return QVariant::fromValue(d->m_rows[index.row()].conditions);

        case OutputsRole:
            return QVariant::fromValue(d->m_rows[index.row()].outputs);

        case EffectsRole:
            return QVariant::fromValue(d->m_rows[index.row()].effects);

        case SoundsRole:
            return QVariant::fromValue(d->m_rows[index.row()].sounds);
        }
    }

    return {};
}

bool FunctionMappingModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (static_cast<DataRole>(role)) {
        case DisplayRole:
            break;

        case ColumnDataRole:
            break;

        case MappingRole:
            if (value.userType() == qMetaTypeId<Mapping>()) {
                d->m_rows[index.row()] = qvariant_cast<Mapping>(value);
                return true;
            }

            break;

        case ConditionsRole:
            if (value.userType() == qMetaTypeId<QList<Condition>>()) {
                d->m_rows[index.row()].conditions = qvariant_cast<QList<Condition>>(value);
                return true;
            }

            break;

        case OutputsRole:
            break;

        case EffectsRole:
            break;

        case SoundsRole:
            break;
        }
    }

    return false;
}

QVariant FunctionMappingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (static_cast<DataColumn>(section)) {
            case ConditionColumn:
                return tr("Conditions");
            case OutputsColumn:
                return tr("Outputs");
            case EffectsColumn:
                return tr("Effects");
            case SoundsColumn:
                return tr("Sounds");
            }
        }
    } else if (orientation == Qt::Vertical) {
        if (role == Qt::DisplayRole) {
            if (section < d->m_rows.count())
                return QString::number(section + 1);
        } else if (role == Qt::TextAlignmentRole) {
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    return {};
}

Qt::ItemFlags FunctionMappingModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsSelectable
            | Qt::ItemIsEditable
            | Qt::ItemIsEnabled
            | Qt::ItemNeverHasChildren;
}

void FunctionMappingModel::Private::fillLokPilot5(int outputCount)
{
    m_rows.emplace_back(QList{forward(), enabled(Condition::Function0)}, Output::FrontLight);
    m_rows.emplace_back(QList{reverse(), enabled(Condition::Function0)}, Output::RearLight);
    m_rows.emplace_back(QList{enabled(Condition::Function1)}, Output::Output1);
    m_rows.emplace_back(QList{enabled(Condition::Function2)}, Output::Output2);
    m_rows.emplace_back(QList{enabled(Condition::Function3)}, Output{}, Effect::Shunting);
    m_rows.emplace_back(QList{enabled(Condition::Function4)}, Output{}, Effect::SkipAccelerationTime);

    for (int i = 0; i < outputCount - 2; ++i) {
        const auto function = static_cast<Condition::Variable>(Condition::Function5 + i);
        const auto output = static_cast<Output>(core::value(Output::Output3) << i);
        m_rows.emplace_back(QList{enabled(function)}, output);
    }
}

void FunctionMappingModel::Private::fillLokPilot5Fx(int outputCount)
{
    m_rows.emplace_back(QList{forward(), enabled(Condition::Function0)}, Output::FrontLight);
    m_rows.emplace_back(QList{reverse(), enabled(Condition::Function0)}, Output::RearLight);

    for (int i = 0; i < outputCount; ++i) {
        const auto function = static_cast<Condition::Variable>(Condition::Function1 + i);
        const auto output = static_cast<Output>(core::value(Output::Output1) << i);
        m_rows.emplace_back(QList{enabled(function)}, output);
    }
}

void FunctionMappingModel::Private::fillLokSound5()
{
    m_rows.emplace_back(QList{disabled(Condition::Driving), forward()});
    m_rows.emplace_back(QList{disabled(Condition::Driving), reverse()});
    m_rows.emplace_back(QList{enabled(Condition::Driving), forward()});
    m_rows.emplace_back(QList{enabled(Condition::Driving), reverse()});
    m_rows.emplace_back(QList{forward(), enabled(Condition::Function0)}, Output::FrontLight);
    m_rows.emplace_back(QList{enabled(Condition::Function0), reverse()}, Output::RearLight);
    m_rows.emplace_back(QList{enabled(Condition::Function1)}, Sound::Slot1 | Sound::Slot2);
    m_rows.emplace_back(QList{enabled(Condition::Function2)}, Sound::Slot3);
    m_rows.emplace_back(QList{enabled(Condition::Function3)}, Sound::Slot4);
    m_rows.emplace_back(QList{enabled(Condition::Function4)}, Sound::Slot5);
    m_rows.emplace_back(QList{enabled(Condition::Function5)}, Effect::HeavyLoad);
    m_rows.emplace_back(QList{enabled(Condition::Function6)}, Effect::Shunting | Effect::Brake3 | Effect::AlternativeLoad);
    m_rows.emplace_back(QList{enabled(Condition::Function7)}, Sound::Slot15);
    m_rows.emplace_back(QList{enabled(Condition::Function8)}, Output::Output1);
    m_rows.emplace_back(QList{enabled(Condition::Function9)}, Sound::Slot9);
    m_rows.emplace_back(QList{enabled(Condition::Function10)}, Sound::Slot10);
    m_rows.emplace_back(QList{enabled(Condition::Function11)}, Sound::Slot8);
    m_rows.emplace_back(QList{enabled(Condition::Function12)}, Effect::Brake1, Sound::Slot22);
    m_rows.emplace_back(QList{disabled(Condition::Function5), enabled(Condition::Function13)}, Effect::Shift2);
    m_rows.emplace_back(QList{enabled(Condition::Function14)}, Sound::Slot7);
    m_rows.emplace_back(QList{enabled(Condition::Function15)}, Effect::SteamGenerator);
    m_rows.emplace_back(QList{enabled(Condition::Function16)}, Sound::Slot12);
    m_rows.emplace_back(QList{enabled(Condition::Function17)}, Sound::Slot17);
    m_rows.emplace_back(QList{enabled(Condition::Function18)}, Sound::Slot14);
    m_rows.emplace_back(QList{enabled(Condition::Function19)}, Sound::Slot16);
    m_rows.emplace_back(QList{enabled(Condition::Function20)}, Sound::Slot18);
    m_rows.emplace_back(QList{enabled(Condition::Function21)}, Sound::Slot19);
    m_rows.emplace_back(QList{enabled(Condition::Function22)}, Sound::Slot20);
    m_rows.emplace_back(QList{enabled(Condition::Function23)}, Sound::Slot21);
    m_rows.emplace_back(QList{enabled(Condition::Function24)}, Sound::Slot6);
    m_rows.emplace_back(QList{disabled(Condition::Function25)}, Sound::Slot13);
    m_rows.emplace_back(QList{enabled(Condition::Function26)}, Output::Output2);
    m_rows.emplace_back(QList{enabled(Condition::Function27)}, Effect::SoundFader);
    m_rows.emplace_back(QList{enabled(Condition::Function28)}, Effect::MuteBrakes);
    m_rows.emplace_back(QList{enabled(Condition::Function29)}, Effect::Brake2);
    m_rows.emplace_back(QList{enabled(Condition::Function30)}, Sound::Slot11);
    m_rows.emplace_back(QList{enabled(Condition::Function31)}, Output::Output3);
}

void FunctionMappingModel::Private::reportError(QString errorString)
{
    m_errorString = errorString;
    emit model()->errorOccured(std::move(errorString));
}

QString displayName(FunctionMappingModel::Preset preset)
{
    switch (preset) {
    case FunctionMappingModel::Preset::Empty:
        return FunctionMappingModel::tr("Empty");
    case FunctionMappingModel::Preset::Lp5:
        return FunctionMappingModel::tr("ESU LokPilot 5");
    case FunctionMappingModel::Preset::Lp5Micro:
        return FunctionMappingModel::tr("ESU LokPilot 5 micro");
    case FunctionMappingModel::Preset::Lp5MicroN18:
        return FunctionMappingModel::tr("ESU LokPilot 5 micro Next18");
    case FunctionMappingModel::Preset::Lp5Fx:
        return FunctionMappingModel::tr("ESU LokPilot 5 Fx");
    case FunctionMappingModel::Preset::Lp5FxMicro:
        return FunctionMappingModel::tr("ESU LokPilot 5 Fx micro");
    case FunctionMappingModel::Preset::Ls5:
        return FunctionMappingModel::tr("ESU LokSound 5");
    }

    return {};
}

QString displayName(Condition::Variable variable)
{
    return displayString<Condition::Variable>(variable);
}

Condition::State Mapping::state(Condition::Variable variable) const
{
    const auto sameVariable = [variable](const auto &condition) { return condition.variable == variable; };
    const auto it = std::find_if(conditions.begin(), conditions.end(), sameVariable);
    return it != conditions.end() ? it->state : Condition::Ignore;
}

} // namespace lmrs::esu

template<>
lmrs::esu::FunctionMappingReader::Registry &lmrs::esu::FunctionMappingReader::registry()
{
    static auto formats = Registry {
        {core::FileFormat::plainText(), std::make_unique<lmrs::esu::PlainTextFileReader, QString>},
        {core::FileFormat::lokProgrammer(), std::make_unique<lmrs::esu::EsuxFileReader, QString>},
        {core::FileFormat::z21Maintenance(), std::make_unique<lmrs::esu::DelimiterSeparatedFileReader, QString>},
    };

    return formats;
}

template<>
lmrs::esu::FunctionMappingWriter::Registry &lmrs::esu::FunctionMappingWriter::registry()
{
    static auto formats = Registry {
        {core::FileFormat::plainText(), std::make_unique<lmrs::esu::PlainTextFileWriter, QString>},
        {core::FileFormat::z21Maintenance(), std::make_unique<lmrs::esu::DelimiterSeparatedFileWriter, QString>},
    };

    return formats;
}

#include "functionmappingmodel.moc"

// YAMORAK - Yet Another Modelrail Toolkit
