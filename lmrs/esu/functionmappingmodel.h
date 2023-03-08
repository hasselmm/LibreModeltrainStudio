#ifndef LMRS_ESU_DCC_FUNCTIONMAPPINGMODEL_H
#define LMRS_ESU_DCC_FUNCTIONMAPPINGMODEL_H

#include <lmrs/core/dccconstants.h>
#include <lmrs/core/fileformat.h>

#include <QAbstractTableModel>

namespace lmrs::core {
struct FileFormat;
class VariableControl;
}

namespace lmrs::esu {
namespace functionmapping {

Q_NAMESPACE

struct Condition
{
    enum Variable {
        Driving,
        Direction,
        Function0,
        Function1,
        Function2,
        Function3,
        Function4,
        Function5,
        Function6,
        Function7,
        Function8,
        Function9,
        Function10,
        Function11,
        Function12,
        Function13,
        Function14,
        Function15,
        Function16,
        Function17,
        Function18,
        Function19,
        Function20,
        Function21,
        Function22,
        Function23,
        Function24,
        Function25,
        Function26,
        Function27,
        Function28,
        Function29,
        Function30,
        Function31,
        WheelSensor,
        Unused,
        Sensor1,
        Sensor2,
        Sensor3,
        Sensor4,
    };

    Q_ENUM(Variable)

    enum State {
        Ignore = 0,
        Enabled = 1,
        Disabled = 2,
        Forward = Enabled,
        Reverse = Disabled,
    };

    Q_ENUM(State)

    Variable variable;
    State state;

    constexpr Condition(Variable variable, State state) noexcept
        : variable{variable}
        , state{state}
    {}

    [[nodiscard]] constexpr auto operator<=>(const Condition &rhs) const = default;

    Q_GADGET
};

enum class Output {
    FrontLight      = 1 << 0,
    RearLight       = 1 << 1,
    Output1         = 1 << 2,
    Output2         = 1 << 3,
    Output3         = 1 << 4,
    Output4         = 1 << 5,
    Output5         = 1 << 6,
    Output6         = 1 << 7,
    Output7         = 1 << 8,
    Output8         = 1 << 9,
    Output9         = 1 << 10,
    Output10        = 1 << 11,
    Output11        = 1 << 12,
    Output12        = 1 << 13,
    Output13        = 1 << 14,
    Output14        = 1 << 15,
    Output15        = 1 << 16,
    Output16        = 1 << 17,
    Output17        = 1 << 18,
    Output18        = 1 << 19,
    FrontLightAlt   = 1 << 20,
    ReadLightAlt    = 1 << 21,
    Output1Alt      = 1 << 22,
    Output2Alt      = 1 << 23,
};

Q_FLAG_NS(Output)
Q_DECLARE_FLAGS(Outputs, Output)
Q_DECLARE_OPERATORS_FOR_FLAGS(Outputs)

enum class Effect {
    AlternativeLoad         = 1 << 0,
    Shunting                = 1 << 1,
    Brake1                  = 1 << 2,
    Brake2                  = 1 << 3,
    Brake3                  = 1 << 4,
    HeavyLoad               = 1 << 5,
    UncouplingCycle         = 1 << 6,
    Drivehold               = 1 << 7,
    Firebox                 = 1 << 8,
    Dimmer                  = 1 << 9,
    GradeCrossing           = 1 << 10,
    SkipAccelerationTime    = 1 << 11,
    SteamGenerator          = 1 << 12,
    SoundFader              = 1 << 13,
    MuteBrakes              = 1 << 14,
    VolumeControl           = 1 << 15,
    Shift1                  = 1 << 16,
    Shift2                  = 1 << 17,
    Shift3                  = 1 << 18,
    Shift4                  = 1 << 19,
    Shift5                  = 1 << 20,
    Shift6                  = 1 << 21,
};

Q_FLAG_NS(Effect)
Q_DECLARE_FLAGS(Effects, Effect)
Q_DECLARE_OPERATORS_FOR_FLAGS(Effects)

enum class Sound {
    Slot1  = 1 << 0,
    Slot2  = 1 << 1,
    Slot3  = 1 << 2,
    Slot4  = 1 << 3,
    Slot5  = 1 << 4,
    Slot6  = 1 << 5,
    Slot7  = 1 << 6,
    Slot8  = 1 << 7,
    Slot9  = 1 << 8,
    Slot10 = 1 << 9,
    Slot11 = 1 << 10,
    Slot12 = 1 << 11,
    Slot13 = 1 << 12,
    Slot14 = 1 << 13,
    Slot15 = 1 << 14,
    Slot16 = 1 << 15,
    Slot17 = 1 << 16,
    Slot18 = 1 << 17,
    Slot19 = 1 << 18,
    Slot20 = 1 << 19,
    Slot21 = 1 << 20,
    Slot22 = 1 << 21,
    Slot23 = 1 << 22,
    Slot24 = 1 << 23,
    Slot25 = 1 << 24,
    Slot26 = 1 << 25,
    Slot27 = 1 << 26,
    Slot28 = 1 << 27,
    Slot29 = 1 << 28,
    Slot30 = 1 << 29,
    Slot31 = 1 << 30,
    Slot32 = 1 << 31,
};

Q_FLAG_NS(Sound)
Q_DECLARE_FLAGS(Sounds, Sound)
Q_DECLARE_OPERATORS_FOR_FLAGS(Sounds)

struct Mapping
{
    static constexpr auto MaximumCount = 72;

    Mapping() = default;

    Mapping(QList<Condition> conditions, Outputs outputs = {}, Effects effects = {}, Sounds sounds = {})
        : conditions{std::move(conditions)}
        , outputs{std::move(outputs)}
        , effects{std::move(effects)}
        , sounds{std::move(sounds)}
    {}

    Mapping(QList<Condition> conditions, Effects effects, Sounds sounds = {})
        : conditions{std::move(conditions)}
        , effects{std::move(effects)}
        , sounds{std::move(sounds)}
    {}

    Mapping(QList<Condition> conditions, Sounds sounds)
        : conditions{std::move(conditions)}
        , sounds{std::move(sounds)}
    {}

    [[nodiscard]] bool operator==(const Mapping &rhs) const noexcept = default;
    [[nodiscard]] auto isEmpty() const { return conditions.empty() && !outputs && !effects && !sounds; }

    Condition::State state(Condition::Variable variable) const;

    void set(Outputs newValue) { outputs = newValue; }
    void set(Effects newValue) { effects = newValue; }
    void set(Sounds newValue) { sounds = newValue; }

    QList<Condition> conditions;
    Outputs outputs = {};
    Effects effects = {};
    Sounds sounds = {};
};

} // functionmapping

using namespace functionmapping;

class FunctionMappingModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum DataColumn {
        ConditionColumn,
        OutputsColumn,
        EffectsColumn,
        SoundsColumn,
    };

    Q_ENUM(DataColumn)

    enum DataRole {
        DisplayRole = Qt::DisplayRole,
        ColumnDataRole = Qt::UserRole,
        MappingRole,
        ConditionsRole,
        OutputsRole,
        EffectsRole,
        SoundsRole,
    };

    Q_ENUM(DataRole)

    enum class Preset {
        Empty,
        Ls5,
        Lp5,
        Lp5Micro,
        Lp5MicroN18,
        Lp5Fx,
        Lp5FxMicro,
    };

    Q_ENUM(Preset)

    using VariableValueMap = QMap<core::dcc::ExtendedVariableIndex, core::dcc::VariableValue>;

    explicit FunctionMappingModel(QObject *parent = {});
    explicit FunctionMappingModel(VariableValueMap variables, QObject *parent = {});

    void reset(Preset preset = Preset::Empty);

    void read(core::VariableControl *variableControl, core::dcc::VehicleAddress address);
    void write(core::VariableControl *variableControl, core::dcc::VehicleAddress address);

    QString errorString() const;
    VariableValueMap variables() const;

signals:
    void errorOccured(QString errorString);

public: // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    class Private;
    Private *const d;
};

constexpr auto forward() { return Condition{Condition::Direction, Condition::Forward}; }
constexpr auto reverse() { return Condition{Condition::Direction, Condition::Reverse}; }
constexpr auto enabled(Condition::Variable variable) { return Condition{variable, Condition::Enabled}; }
constexpr auto disabled(Condition::Variable variable) { return Condition{variable, Condition::Disabled}; }

[[nodiscard]] QString displayName(Condition::Variable variable);
[[nodiscard]] QString displayName(FunctionMappingModel::Preset preset);

using FunctionMappingReader = core::FileFormatReader<esu::FunctionMappingModel>;
using FunctionMappingWriter = core::FileFormatWriter<esu::FunctionMappingModel>;

} // namespace lmrs::esu

#endif // LMRS_ESU_DCC_FUNCTIONMAPPINGMODEL_H
