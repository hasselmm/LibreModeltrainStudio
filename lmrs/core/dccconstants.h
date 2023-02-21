#ifndef LMRS_CORE_DCCCONSTANTS_H
#define LMRS_CORE_DCCCONSTANTS_H

#include "quantities.h"
#include "typetraits.h"

#include <QMetaType>

#include <bitset>

namespace lmrs::core {

template<class T>
inline const QLoggingCategory &logger();

} // namespace lmrs::core

namespace lmrs::core::dcc {

Q_NAMESPACE

// Literals ============================================================================================================

using BasicAddress          = literal<quint8,  struct BasicAddressTag, 0, 127>;
using VehicleAddress        = literal<quint16, struct VehicleAddressTag, 1, (232 - 192) * 256 - 1>;
using AccessoryAddress      = literal<quint16, struct AccessoryAddressTag, 1, (192 - 128) * 256 - 1>;

using VariableIndex         = literal<quint16, struct VariableIndexTag, 1, 1024>;
using VariableValue         = literal<quint8,  struct VariableValueTag>;

using ExtendedVariableIndex = literal<quint32, struct ExtendedVariableIndexTag>;
using ExtendedPageIndex     = literal<quint16, struct ExtendedPageIndexTag>;
using SusiPageIndex         = literal<quint8,  struct SusiPageIndexTag>;

template<quint8 Steps>
using SpeedQuantity         = quantity<quint8, struct SpeedUnit, std::ratio<1, Steps>>;
using Speed14               = SpeedQuantity<15>;
using Speed28               = SpeedQuantity<31>;
using Speed126              = SpeedQuantity<127>;
using SpeedPercentil        = SpeedQuantity<100>;
using Speed                 = std::variant<std::monostate, Speed14, Speed28, Speed126, SpeedPercentil>;


QDebug operator<<(QDebug debug, Speed speed);

// TODO: verify and ensure that speeds representing emergancy stops are converted properly

template<class T>
constexpr T speedCast(const Speed &speed)
{
    if (std::holds_alternative<T>(speed))
        return std::get<T>(speed);
    if (std::holds_alternative<Speed126>(speed))
        return quantityCast<T>(std::get<Speed126>(speed));
    if (std::holds_alternative<Speed28>(speed))
        return quantityCast<T>(std::get<Speed28>(speed));
    if (std::holds_alternative<Speed14>(speed))
        return quantityCast<T>(std::get<Speed14>(speed));
    if (std::holds_alternative<SpeedPercentil>(speed))
        return quantityCast<T>(std::get<SpeedPercentil>(speed));

    return T{};
}

constexpr bool isValid(const Speed &speed)
{
    return !std::holds_alternative<std::monostate>(speed);
}

using Function              = literal<quint8,  struct FunctionTag, 0, 68>;

using AccessoryState        = literal<quint8,  struct AccessoryStateTag>;

// Literal Constructors ================================================================================================

constexpr ExtendedPageIndex extendedPage(VariableValue cv31, VariableValue cv32)
{
    return static_cast<ExtendedPageIndex::value_type>((cv31 << 8) | cv32);
}

static_assert(extendedPage(1, 0) == 256);

// ---------------------------------------------------------------------------------------------------------------------

constexpr ExtendedVariableIndex extendedVariable(VariableIndex variable, ExtendedPageIndex page)
{
    if (variable < 256)
        variable += 257;

    return static_cast<ExtendedVariableIndex::value_type>((page << 12) | (variable & 0x3ff) | 0x400);
}

static_assert(extendedVariable(0, 16) == 0x10501);
static_assert(extendedVariable(0, 256) == 0x100501);
static_assert(extendedVariable(257, 16) == 0x10501);
static_assert(extendedVariable(257, 256) == 0x100501);

constexpr ExtendedVariableIndex extendedVariable(VariableIndex variable, VariableValue cv31, VariableValue cv32)
{
    return extendedVariable(variable, extendedPage(cv31, cv32));
}

static_assert(extendedVariable(0, 0, 16) == 0x10501);
static_assert(extendedVariable(0, 1, 0) == 0x100501);
static_assert(extendedVariable(257, 0, 16) == 0x10501);
static_assert(extendedVariable(257, 1, 0) == 0x100501);

// ---------------------------------------------------------------------------------------------------------------------

constexpr ExtendedVariableIndex susiVariable(VariableIndex variable, SusiPageIndex page)
{
    return static_cast<ExtendedVariableIndex::value_type>((page << 12) | (variable & 0x3ff) | 0x800);
}

static_assert(susiVariable(900, 0) == 0x00b84);
static_assert(susiVariable(940, 1) == 0x01bac);
static_assert(susiVariable(981, 254) == 0xfebd5);

// Enumerations ========================================================================================================

struct Address
{
    static constexpr BasicAddress Broadcast = 0;
    static constexpr BasicAddress IdleAddress = 255;
};

enum class ExtendedPage : ExtendedPageIndex::value_type
{
    RailCom = extendedPage(0, 255),
};

enum class VehicleVariable : ExtendedVariableIndex::value_type
{
    Invalid                 = 0,
    BasicAddress            = 1,
    MinimumSpeed            = 2,
    AccelerationRate        = 3,
    DecelerationRate        = 4,
    MaximumSpeed            = 5,
    MiddleSpeed             = 6,
    DecoderVersion          = 7,
    Manufacturer            = 8,
    TotalPwmPeriod          = 9,
    EmfFeedbackCutout       = 10,
    PacketTimeout           = 11,
    PowerSources            = 12,
    AnalogFunctionsLow      = 13,
    AnalogFunctionsHigh     = 14,
    DecoderLockSelect       = 15,
    DecoderLockConfig       = 16,
    ExtendedAddressHigh     = 17,
    ExtendedAddressLow      = 18,
    ConsistAddress          = 19,
    ConsistFunctionsLow     = 21,
    ConsistFunctionsHigh    = 22,
    AccelerationAdjustment  = 23,
    DecelerationAdjustment  = 24,
    SpeedTable              = 25,
    AutoStop                = 27,
    BiDiConfiguration       = 28,
    Configuration           = 29,
    ErrorInformation        = 30,
    ExtendedPageIndexHigh   = 31,
    ExtendedPageIndexLow    = 32,
    OutputsF0Fwd            = 33,
    OutputsF0Rev            = 34,
    OutputsF1               = 35,
    OutputsF2               = 36,
    OutputsF3               = 37,
    OutputsF4               = 38,
    OutputsF5               = 39,
    OutputsF6               = 40,
    OutputsF7               = 41,
    OutputsF8               = 42,
    OutputsF9               = 43,
    OutputsF10              = 44,
    OutputsF11              = 45,
    OutputsF12              = 46,
    VendorUnique1Begin      = 47,
    VendorUnique1End        = 64,
    KickStartAmount         = 65,
    ForwardTrim             = 66,
    SpeedTableBegin         = 67,
    SpeedTableEnd           = 94,
    ReverseTrim             = 95,
    NrmaReservedBegin       = 96,
    UserIdHigh              = 105,
    UserIdLow               = 106,
    NrmaReservedEnd         = 111,
    VendorUnique2Begin      = 112,
    VendorUnique2End        = 256,
    ExtendedBegin           = 257,
    ExtendedEnd             = 512,
    NrmaDynamicBegin        = 880,
    NrmaDynamicEnd          = 895,
    SusiBegin               = 896,
    SusiModuleId            = 897,
    Susi1Begin              = 900,
    Susi1End                = 939,
    Susi2Begin              = 940,
    Susi2End                = 979,
    Susi3Begin              = 980,
    Susi3End                = 1019,
    SusiStatus              = 1020,
    SusiBankIndex           = 1021,
    SusiEnd                 = 1024,

    RailComManufacturer     = extendedVariable(0, extendedPage(0, 255)),
    RailComProductId        = extendedVariable(4, extendedPage(0, 255)),
    RailComSerialNumber     = extendedVariable(8, extendedPage(0, 255)),
    RailComProductionDate   = extendedVariable(12, extendedPage(0, 255)),

    RailComPlusIcon         = extendedVariable(0, extendedPage(1, 0)),
    RailComPlusNameBegin    = extendedVariable(4, extendedPage(1, 0)),
    RailComPlusNameEnd      = extendedVariable(31, extendedPage(1, 0)),
    RailComPlusName         = RailComPlusNameBegin,
    RailComPlusKeysBegin    = extendedVariable(0, extendedPage(1, 1)),
    RailComPlusKeysEnd      = extendedVariable(32, extendedPage(1, 1)),
    RailComPlusKeys         = RailComPlusKeysBegin,

    EsuFunctionConditionBegin   = extendedVariable(0, extendedPage(16, 3)),
    EsuFunctionConditionEnd     = extendedVariable(127, extendedPage(16, 7)),
    EsuFunctionCondition        = EsuFunctionConditionBegin,
    EsuFunctionOperationBegin   = extendedVariable(0, extendedPage(16, 8)),
    EsuFunctionOperationEnd     = extendedVariable(127, extendedPage(16, 12)),
    EsuFunctionOperation        = EsuFunctionOperationBegin,

    Susi1Manufacturer       = susiVariable(900, 0),
    Susi1ProductId          = susiVariable(900, 1),
    Susi1ManufacturerAlt    = susiVariable(900, 254),
    Susi1MajorVersion       = susiVariable(901, 0),
    Susi1MinorVersion       = susiVariable(901, 1),
    Susi1SusiVersion        = susiVariable(901, 254),
    Susi2Manufacturer       = susiVariable(940, 0),
    Susi2ProductId          = susiVariable(940, 1),
    Susi2ManufacturerAlt    = susiVariable(940, 254),
    Susi2MajorVersion       = susiVariable(941, 0),
    Susi2MinorVersion       = susiVariable(941, 1),
    Susi2SusiVersion        = susiVariable(941, 254),
    Susi3Manufacturer       = susiVariable(980, 0),
    Susi3ProductId          = susiVariable(980, 1),
    Susi3ManufacturerAlt    = susiVariable(980, 254),
    Susi3MajorVersion       = susiVariable(981, 0),
    Susi3MinorVersion       = susiVariable(981, 1),
    Susi3SusiVersion        = susiVariable(981, 254),
};

Q_ENUM_NS(VehicleVariable)

enum class VariableSpace
{
    VendorUnique1,
    SpeedTable,
    NrmaReserved,
    VendorUnique2,
    Extended,
    NrmaDynamic,
    Susi,
    Susi1,
    Susi2,
    Susi3,
    RailComPlusName,
    RailComPlusKeys,
    EsuFunctionCondition,
    EsuFunctionOperation,
};

Q_ENUM_NS(VariableSpace)

constexpr Range<VehicleVariable> range(VariableSpace space)
{
    switch (space) {
    case VariableSpace::Extended:
        return {VehicleVariable::ExtendedBegin, VehicleVariable::ExtendedEnd};
    case VariableSpace::Susi:
        return {VehicleVariable::Susi1Begin, VehicleVariable::Susi3End};
    case VariableSpace::Susi1:
        return {VehicleVariable::Susi1Begin, VehicleVariable::Susi1End};
    case VariableSpace::Susi2:
        return {VehicleVariable::Susi2Begin, VehicleVariable::Susi2End};
    case VariableSpace::Susi3:
        return {VehicleVariable::Susi3Begin, VehicleVariable::Susi3End};
    case VariableSpace::SpeedTable:
        return {VehicleVariable::SpeedTableBegin, VehicleVariable::SpeedTableEnd};
    case VariableSpace::NrmaDynamic:
        return {VehicleVariable::NrmaDynamicBegin, VehicleVariable::NrmaDynamicEnd};
    case VariableSpace::NrmaReserved:
        return {VehicleVariable::NrmaReservedBegin, VehicleVariable::NrmaReservedEnd};
    case VariableSpace::VendorUnique1:
        return {VehicleVariable::VendorUnique1Begin, VehicleVariable::VendorUnique1End};
    case VariableSpace::VendorUnique2:
        return {VehicleVariable::VendorUnique2Begin, VehicleVariable::VendorUnique2End};
    case VariableSpace::RailComPlusName:
        return {VehicleVariable::RailComPlusNameBegin, VehicleVariable::RailComPlusNameEnd};
    case VariableSpace::RailComPlusKeys:
        return {VehicleVariable::RailComPlusKeysBegin, VehicleVariable::RailComPlusKeysEnd};
    case VariableSpace::EsuFunctionCondition:
        return {VehicleVariable::EsuFunctionConditionBegin, VehicleVariable::EsuFunctionConditionEnd};
    case VariableSpace::EsuFunctionOperation:
        return {VehicleVariable::EsuFunctionOperationBegin, VehicleVariable::EsuFunctionOperationEnd};
    }

    return {VehicleVariable::Invalid, VehicleVariable::Invalid};
}

enum class SusiNode
{
    Invalid,
    Node1,
    Node2,
    Node3,
};

Q_ENUM_NS(SusiNode)

enum class FunctionGroup
{
    None,
    Group1,
    Group2,
    Group3,
    Group4,
    Group5,
    Group6,
    Group7,
    Group8,
    Group9,
    Group10,
};

Q_ENUM_NS(FunctionGroup)

enum class Direction {
    Unknown,
    Forward,
    Reverse,
};

Q_ENUM_NS(Direction)

enum class TurnoutState {
    Unknown  = 0b00,
    Branched = 0b01,
    Straight = 0b10,
    Invalid  = 0b11,

    Green = Straight,
    Red = Branched,
};

Q_ENUM_NS(TurnoutState)

// Accessors ===========================================================================================================

constexpr VariableIndex variableIndex(ExtendedVariableIndex variable)
{
    return ((variable - 1) & 0x3ff) + 1;
}

constexpr VariableIndex variableIndex(VehicleVariable variable)
{
    return variableIndex(value(variable));
}

static_assert(variableIndex(VehicleVariable::Manufacturer) == 8);
static_assert(variableIndex(VehicleVariable::RailComManufacturer) == 257);
static_assert(variableIndex(VehicleVariable::Susi1MajorVersion) == 901);
static_assert(variableIndex(VehicleVariable::Susi2MinorVersion) == 941);
static_assert(variableIndex(VehicleVariable::Susi3SusiVersion) == 981);

// ---------------------------------------------------------------------------------------------------------------------

QString variableString(ExtendedVariableIndex variable);
QString fullVariableName(ExtendedVariableIndex variable);

// ---------------------------------------------------------------------------------------------------------------------

constexpr VehicleVariable vehicleVariable(ExtendedVariableIndex variable)
{
    return static_cast<VehicleVariable>(variable.value);
}

static_assert(vehicleVariable(8) == VehicleVariable::Manufacturer);
static_assert(vehicleVariable(0xff501) == VehicleVariable::RailComManufacturer);

// ---------------------------------------------------------------------------------------------------------------------

enum class VariableType {
    Invalid,
    U8,
    U16H,
    U16L,
    U32H,
    D32H,
    UTF8,
};

constexpr auto variableType(VehicleVariable variable)
{
    switch (variable) {
    case VehicleVariable::Invalid:
        return VariableType::Invalid;

    case VehicleVariable::BasicAddress:
    case VehicleVariable::MinimumSpeed:
    case VehicleVariable::AccelerationRate:
    case VehicleVariable::DecelerationRate:
    case VehicleVariable::MaximumSpeed:
    case VehicleVariable::MiddleSpeed:
    case VehicleVariable::DecoderVersion:
    case VehicleVariable::Manufacturer:
    case VehicleVariable::TotalPwmPeriod:
    case VehicleVariable::EmfFeedbackCutout:
    case VehicleVariable::PacketTimeout:
    case VehicleVariable::PowerSources:
        return VariableType::U8;
    case VehicleVariable::AnalogFunctionsLow:
        return VariableType::U16L;
    case VehicleVariable::AnalogFunctionsHigh:
        return VariableType::U16H;
    case VehicleVariable::DecoderLockSelect:
    case VehicleVariable::DecoderLockConfig:
        return VariableType::U8;
    case VehicleVariable::ExtendedAddressHigh:
        return VariableType::U16H;
    case VehicleVariable::ExtendedAddressLow:
        return VariableType::U16L;
    case VehicleVariable::ConsistAddress:
        return VariableType::U8;
    case VehicleVariable::ConsistFunctionsLow:
        return VariableType::U16L;
    case VehicleVariable::ConsistFunctionsHigh:
        return VariableType::U16L;
    case VehicleVariable::AccelerationAdjustment:
    case VehicleVariable::DecelerationAdjustment:
    case VehicleVariable::SpeedTable:
    case VehicleVariable::AutoStop:
    case VehicleVariable::BiDiConfiguration:
    case VehicleVariable::Configuration:
    case VehicleVariable::ErrorInformation:
        return VariableType::U8;
    case VehicleVariable::ExtendedPageIndexHigh:
        return VariableType::U16H;
    case VehicleVariable::ExtendedPageIndexLow:
        return VariableType::U16L;
    case VehicleVariable::OutputsF0Fwd:
    case VehicleVariable::OutputsF0Rev:
    case VehicleVariable::OutputsF1:
    case VehicleVariable::OutputsF2:
    case VehicleVariable::OutputsF3:
    case VehicleVariable::OutputsF4:
    case VehicleVariable::OutputsF5:
    case VehicleVariable::OutputsF6:
    case VehicleVariable::OutputsF7:
    case VehicleVariable::OutputsF8:
    case VehicleVariable::OutputsF9:
    case VehicleVariable::OutputsF10:
    case VehicleVariable::OutputsF11:
    case VehicleVariable::OutputsF12:
    case VehicleVariable::VendorUnique1Begin:
    case VehicleVariable::VendorUnique1End:
    case VehicleVariable::KickStartAmount:
    case VehicleVariable::ForwardTrim:
    case VehicleVariable::SpeedTableBegin:
    case VehicleVariable::SpeedTableEnd:
    case VehicleVariable::ReverseTrim:
    case VehicleVariable::NrmaReservedBegin:
        return VariableType::U8;
    case VehicleVariable::UserIdHigh:
        return VariableType::U16H;
    case VehicleVariable::UserIdLow:
        return VariableType::U16L;
    case VehicleVariable::NrmaReservedEnd:
    case VehicleVariable::VendorUnique2Begin:
    case VehicleVariable::VendorUnique2End:
    case VehicleVariable::ExtendedBegin:
    case VehicleVariable::ExtendedEnd:
    case VehicleVariable::NrmaDynamicBegin:
    case VehicleVariable::NrmaDynamicEnd:
    case VehicleVariable::SusiBegin:
    case VehicleVariable::SusiModuleId:
    case VehicleVariable::Susi1Begin:
    case VehicleVariable::Susi1End:
    case VehicleVariable::Susi2Begin:
    case VehicleVariable::Susi2End:
    case VehicleVariable::Susi3Begin:
    case VehicleVariable::Susi3End:
    case VehicleVariable::SusiStatus:
    case VehicleVariable::SusiBankIndex:
    case VehicleVariable::SusiEnd:
        return VariableType::U8;

    case VehicleVariable::RailComManufacturer:
        return VariableType::U16H; // FIXME: handle range
    case VehicleVariable::RailComProductId:
        return VariableType::U32H; // FIXME: handle range
    case VehicleVariable::RailComSerialNumber:
        return VariableType::U32H; // FIXME: handle range
    case VehicleVariable::RailComProductionDate:
        return VariableType::D32H; // FIXME: handle range

    case VehicleVariable::RailComPlusIcon:
        return VariableType::U16H;
    case VehicleVariable::RailComPlusNameBegin:
    case VehicleVariable::RailComPlusNameEnd:
        return VariableType::UTF8; // FIXME: handle range
    case VehicleVariable::RailComPlusKeysBegin:
    case VehicleVariable::RailComPlusKeysEnd:
        return VariableType::U16H; // FIXME: handle range

    case VehicleVariable::EsuFunctionConditionBegin:
    case VehicleVariable::EsuFunctionConditionEnd:
        return VariableType::U8;    // FIXME: struct, array, or range
                                    // - column A-J - U80
    case VehicleVariable::EsuFunctionOperationBegin:
    case VehicleVariable::EsuFunctionOperationEnd:
        return VariableType::U8;    // FIXME: struct, array, or range:
                                    // - column K-M (outputs) - U24
                                    // - column N-P (logics)  - U24
                                    // - column Q-T (sounds)  - U32

    case VehicleVariable::Susi1Manufacturer:
    case VehicleVariable::Susi1ProductId:
    case VehicleVariable::Susi1ManufacturerAlt:
    case VehicleVariable::Susi1MajorVersion:
    case VehicleVariable::Susi1MinorVersion:
    case VehicleVariable::Susi1SusiVersion:
    case VehicleVariable::Susi2Manufacturer:
    case VehicleVariable::Susi2ProductId:
    case VehicleVariable::Susi2ManufacturerAlt:
    case VehicleVariable::Susi2MajorVersion:
    case VehicleVariable::Susi2MinorVersion:
    case VehicleVariable::Susi2SusiVersion:
    case VehicleVariable::Susi3Manufacturer:
    case VehicleVariable::Susi3ProductId:
    case VehicleVariable::Susi3ManufacturerAlt:
    case VehicleVariable::Susi3MajorVersion:
    case VehicleVariable::Susi3MinorVersion:
    case VehicleVariable::Susi3SusiVersion:
        return VariableType::U8;
    }

    return VariableType::U8;
}

constexpr auto variableType(ExtendedVariableIndex variable)
{
    return variableType(vehicleVariable(variable));
}

// ---------------------------------------------------------------------------------------------------------------------

constexpr ExtendedVariableIndex extendedVariable(VehicleVariable variable)
{
    return value(variable);
}

// ---------------------------------------------------------------------------------------------------------------------

constexpr bool hasExtendedPage(ExtendedVariableIndex variable)
{
    return (variable & 0x400) && range(VariableSpace::Extended).contains(variableIndex(variable));
}

constexpr bool hasExtendedPage(VehicleVariable variable)
{
    return hasExtendedPage(value(variable));
}

static_assert(hasExtendedPage(VehicleVariable::Manufacturer) == false);
static_assert(hasExtendedPage(VehicleVariable::RailComManufacturer) == true);

// ---------------------------------------------------------------------------------------------------------------------

constexpr ExtendedPageIndex extendedPage(ExtendedVariableIndex variable)
{
    if (range(VariableSpace::Extended).contains(variableIndex(variable)))
        return (variable >> 12) & 0xffff;

    return {};
}

constexpr ExtendedPageIndex extendedPage(VehicleVariable variable)
{
    return extendedPage(value(variable));
}

static_assert(extendedPage(VehicleVariable::Manufacturer) == 0);
static_assert(extendedPage(VehicleVariable::RailComManufacturer) == 255);

// ---------------------------------------------------------------------------------------------------------------------

constexpr VariableValue cv31(ExtendedPageIndex pageIndex)
{
    return static_cast<VariableValue::value_type>(255 & (pageIndex >> 8));
}

constexpr VariableValue cv31(auto variable)
{
    return cv31(extendedPage(variable));
}

constexpr VariableValue cv32(ExtendedPageIndex pageIndex)
{
    return static_cast<VariableValue::value_type>(255 & (pageIndex >> 0));
}

constexpr VariableValue cv32(auto variable)
{
    return cv32(extendedPage(variable));
}

static_assert(cv31(VehicleVariable{0x10101}) == 0);
static_assert(cv31(VehicleVariable{0x100101}) == 1);
static_assert(cv32(VehicleVariable{0x10101}) == 16);
static_assert(cv32(VehicleVariable{0x100101}) == 0);

static_assert(cv31(VehicleVariable::Manufacturer) == 0);
static_assert(cv32(VehicleVariable::Manufacturer) == 0);
static_assert(cv31(VehicleVariable::RailComManufacturer) == 0);
static_assert(cv32(VehicleVariable::RailComManufacturer) == 255);

// ---------------------------------------------------------------------------------------------------------------------

constexpr bool hasSusiPage(ExtendedVariableIndex variable)
{
    return (variable & 0x800) && range(VariableSpace::Susi).contains(variableIndex(variable));
}

constexpr bool hasSusiPage(VehicleVariable variable)
{
    return hasSusiPage(value(variable));
}

static_assert(hasSusiPage(VehicleVariable::Manufacturer) == false);
static_assert(hasSusiPage(VehicleVariable::Susi1MajorVersion) == true);
static_assert(hasSusiPage(VehicleVariable::Susi2MinorVersion) == true);
static_assert(hasSusiPage(VehicleVariable::Susi3SusiVersion) == true);

// ---------------------------------------------------------------------------------------------------------------------

constexpr SusiPageIndex susiPage(ExtendedVariableIndex variable)
{
    if (range(VariableSpace::Susi).contains(variableIndex(variable)))
        return (variable >> 12) & 255;

    return {0};
}

constexpr SusiPageIndex susiPage(VehicleVariable variable)
{
    return susiPage(value(variable));
}

static_assert(susiPage(VehicleVariable::Manufacturer) == 0);
static_assert(susiPage(VehicleVariable::Susi1MajorVersion) == 0);
static_assert(susiPage(VehicleVariable::Susi2MinorVersion) == 1);
static_assert(susiPage(VehicleVariable::Susi3SusiVersion) == 254);

// ---------------------------------------------------------------------------------------------------------------------

constexpr SusiNode susiNode(ExtendedVariableIndex variable)
{
    if (range(VariableSpace::Susi).contains(variableIndex(variable))) {
        const auto index = variableIndex(variable) - value(VehicleVariable::Susi1Begin);
        return static_cast<SusiNode>((index / range(VariableSpace::Susi1).size()) + 1);
    }

    return SusiNode::Invalid;
}

constexpr SusiNode susiNode(VehicleVariable variable)
{
    return susiNode(value(variable));
}

static_assert(susiNode(VehicleVariable::Manufacturer) == SusiNode::Invalid);
static_assert(susiNode(VehicleVariable::Susi1MajorVersion) == SusiNode::Node1);
static_assert(susiNode(VehicleVariable::Susi2MinorVersion) == SusiNode::Node2);
static_assert(susiNode(VehicleVariable::Susi3SusiVersion) == SusiNode::Node3);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr auto FunctionGroup1    = Range<Function>{0, 4};
constexpr auto FunctionGroup2    = Range<Function>{5, 8};
constexpr auto FunctionGroup3    = Range<Function>{9, 12};
constexpr auto FunctionGroup4    = Range<Function>{13, 20};
constexpr auto FunctionGroup5    = Range<Function>{21, 28};
constexpr auto FunctionGroup6    = Range<Function>{29, 36};
constexpr auto FunctionGroup7    = Range<Function>{37, 44};
constexpr auto FunctionGroup8    = Range<Function>{45, 52};
constexpr auto FunctionGroup9    = Range<Function>{53, 60};
constexpr auto FunctionGroup10   = Range<Function>{61, 68};
constexpr auto FunctionGroupAll  = Range<Function>{0, 68};
constexpr auto FunctionGroupNone = Range<Function>{0, 0};

static_assert(FunctionGroup1.size() == 5);
static_assert(FunctionGroup2.size() == 4);
static_assert(FunctionGroup3.size() == 4);
static_assert(FunctionGroup4.size() == 8);
static_assert(FunctionGroup5.size() == 8);
static_assert(FunctionGroup6.size() == 8);
static_assert(FunctionGroup7.size() == 8);
static_assert(FunctionGroup8.size() == 8);
static_assert(FunctionGroup9.size() == 8);
static_assert(FunctionGroup10.size() == 8);
static_assert(FunctionGroupAll.size() == 69);

using FunctionState = std::bitset<FunctionGroupAll.size()>;

quint8 functionMask(core::Range<Function> group, FunctionState state);

constexpr auto functionGroup(Function function)
{
    constexpr std::array ranges = {
        FunctionGroup1,
        FunctionGroup2,
        FunctionGroup3,
        FunctionGroup4,
        FunctionGroup5,
        FunctionGroup6,
        FunctionGroup7,
        FunctionGroup8,
        FunctionGroup9,
        FunctionGroup10,
    };

    const auto containsFunction = [function](const auto &pair) {
        return pair.contains(function);
    };

    const auto it = std::find_if(ranges.begin(), ranges.end(), containsFunction);

    if (it != ranges.end())
        return *it;

    return FunctionGroupNone;
}

constexpr auto functionRange(FunctionGroup group)
{
    switch (group) {
    case FunctionGroup::Group1:
        return FunctionGroup1;
    case FunctionGroup::Group2:
        return FunctionGroup2;
    case FunctionGroup::Group3:
        return FunctionGroup3;
    case FunctionGroup::Group4:
        return FunctionGroup4;
    case FunctionGroup::Group5:
        return FunctionGroup5;
    case FunctionGroup::Group6:
        return FunctionGroup6;
    case FunctionGroup::Group7:
        return FunctionGroup7;
    case FunctionGroup::Group8:
        return FunctionGroup8;
    case FunctionGroup::Group9:
        return FunctionGroup9;
    case FunctionGroup::Group10:
        return FunctionGroup10;
    case FunctionGroup::None:
        break;
    }

    return FunctionGroupNone;
}

static_assert(functionGroup(0)   == FunctionGroup1);
static_assert(functionGroup(1)   == FunctionGroup1);
static_assert(functionGroup(4)   == FunctionGroup1);
static_assert(functionGroup(5)   == FunctionGroup2);
static_assert(functionGroup(68)  == FunctionGroup10);
static_assert(functionGroup(69)  == FunctionGroupNone);
static_assert(functionGroup(255) == FunctionGroupNone);

} // namespace lmrs::core::dcc

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace lmrs::core::rm { // FIXME: this all isn't DCC and doesn't belong into the DCC namespace

namespace can {

using NetworkId = literal<quint16, struct NetworkIdTag>;
using ModuleId  = literal<quint16, struct ModuleIdTag>;
using PortIndex = literal<quint8,  struct PortIndexTag>;

constexpr auto NetworkIdAny = NetworkId{0xd000};

struct ModuleAddress
{
    NetworkId network;
    ModuleId module;

    constexpr auto fields() const noexcept { return std::tie(network, module); }
    constexpr auto operator==(const ModuleAddress &rhs) const noexcept { return fields() == rhs.fields(); }
    constexpr auto operator!=(const ModuleAddress &rhs) const noexcept { return fields() != rhs.fields(); }
};

struct PortAddress
{
    NetworkId network;
    ModuleId module;
    PortIndex port;

    constexpr auto fields() const noexcept { return std::tie(network, module, port); }
    constexpr auto operator==(const PortAddress &rhs) const noexcept { return fields() == rhs.fields(); }
    constexpr auto operator!=(const PortAddress &rhs) const noexcept { return fields() != rhs.fields(); }

    constexpr auto moduleAddress() const { return ModuleAddress{network, module}; }
};

constexpr size_t qHash(const ModuleAddress &address, size_t seed = 0) noexcept
{
    return qHashMulti(seed, address.network, address.module);
}

constexpr size_t qHash(const PortAddress &address, size_t seed = 0) noexcept
{
    return qHashMulti(seed, address.network, address.module, address.port);
}

} // namespace can

namespace lissy {

using FeedbackAddress = core::literal<quint16, struct FeedbackAddressTag>;

} // namespace lissy

namespace loconet {

struct StationaryInterrogate
{
    constexpr auto operator==(const StationaryInterrogate &) const noexcept { return true; }
};

using ReportAddress = core::literal<quint16, struct ReportAddressTag>;

constexpr auto ReportAddressDefault = ReportAddress{1017};

} // namespace loconet

namespace rbus {

constexpr auto ModulesPerGroup = 10;
constexpr auto PortsPerModule = 8;
constexpr auto PortsPerGroup = ModulesPerGroup * PortsPerModule;

using GroupId   = literal<quint8, struct ModuleGroupTag, 0, 1>;
using ModuleId  = literal<quint8, struct ModuleIndexTag, 1, ModulesPerGroup * (GroupId::Maximum + 1)>;
using PortIndex = literal<quint8, struct PortIndexTag,   1, PortsPerModule>;

static_assert(ModuleId::Maximum == 20);
static_assert(PortIndex::Maximum == 8);

struct PortAddress
{
    ModuleId module;
    PortIndex port;

    constexpr auto fields() const noexcept { return std::tie(module, port); }
    constexpr auto operator==(const PortAddress &rhs) const noexcept { return fields() == rhs.fields(); }
    constexpr auto operator!=(const PortAddress &rhs) const noexcept { return fields() != rhs.fields(); }
};

constexpr auto group(ModuleId module) noexcept
{
    return GroupId{static_cast<quint8>((module - 1) / ModulesPerGroup)};
}

constexpr size_t qHash(const PortAddress &address, size_t seed = 0) noexcept
{
    return qHashMulti(seed, address.module, address.port);
}

} // namespace rbus

struct DetectorAddress
{
    Q_GADGET
    Q_PROPERTY(Type type READ type CONSTANT FINAL)

    using value_type = std::variant<std::monostate,
                                    can::NetworkId, can::ModuleAddress, can::PortAddress,
                                    lissy::FeedbackAddress,
                                    loconet::StationaryInterrogate, loconet::ReportAddress,
                                    rbus::GroupId, rbus::ModuleId, rbus::PortAddress>;

public:
    enum class Type {
        Invalid,
        CanNetwork,
        CanModule,
        CanPort,
        LissyModule,
        LoconetSIC,     // "Stationary Interrogate Request" (Digitrax, Blücher...)
        LoconetModule,  // e.g. Uhlenbrock UB63320
        RBusGroup,
        RBusModule,
        RBusPort,
    };

    Q_ENUM(Type)

    constexpr DetectorAddress() noexcept = default;

    auto type() const { return Type{static_cast<int>(m_value.index())}; }

public slots:
    can::NetworkId canNetwork() const;
    can::ModuleId canModule() const;
    can::PortIndex canPort() const;

    can::ModuleAddress canModuleAddress() const;
    can::PortAddress canPortAddress() const;

    lissy::FeedbackAddress lissyModule() const;

    loconet::ReportAddress loconetModule() const;

    rbus::GroupId rbusGroup() const;
    rbus::ModuleId rbusModule() const;
    rbus::PortIndex rbusPort() const;

    rbus::PortAddress rbusPortAddress() const;

    [[nodiscard]] static DetectorAddress forCanNetwork(can::NetworkId network);
    [[nodiscard]] static DetectorAddress forCanModule(can::NetworkId network, can::ModuleId module);
    [[nodiscard]] static DetectorAddress forCanPort(can::NetworkId network, can::ModuleId module, can::PortIndex port);

    [[nodiscard]] static DetectorAddress forLissyModule(lissy::FeedbackAddress address);

    [[nodiscard]] static DetectorAddress forLoconetSIC();
    [[nodiscard]] static DetectorAddress forLoconetModule(loconet::ReportAddress address);

    [[nodiscard]] static DetectorAddress forRBusGroup(rbus::GroupId group);
    [[nodiscard]] static DetectorAddress forRBusModule(rbus::ModuleId module);
    [[nodiscard]] static DetectorAddress forRBusPort(rbus::ModuleId module, rbus::PortIndex port);

public:
    bool operator==(const DetectorAddress &rhs) const;
    bool operator!=(const DetectorAddress &rhs) const { return !(*this == rhs); }
    operator QVariant() const { return QVariant::fromValue(*this); }

private:
    constexpr explicit DetectorAddress(value_type value) noexcept
        : m_value{std::move(value)}
    {}

    template<typename T> bool equals(const DetectorAddress &rhs) const;

    static auto &logger();

    value_type m_value;
};

QDebug operator<<(QDebug debug, const DetectorAddress &address);

constexpr size_t qHash(const DetectorAddress &address, size_t seed = 0) noexcept
{
    using Type = lmrs::core::rm::DetectorAddress::Type;

    switch (address.type()) {
    case Type::CanNetwork:
        return qHashMulti(seed, value(address.type()), address.canNetwork());
    case Type::CanModule:
        return qHashMulti(seed, value(address.type()), address.canModuleAddress());
    case Type::CanPort:
        return qHashMulti(seed, value(address.type()), address.canPortAddress());

    case Type::LissyModule:
        return qHashMulti(seed, value(address.type()), address.lissyModule());

    case Type::LoconetModule:
        return qHashMulti(seed, value(address.type()), address.loconetModule());

    case Type::RBusGroup:
        return qHashMulti(seed, value(address.type()), address.rbusGroup());
    case Type::RBusModule:
        return qHashMulti(seed, value(address.type()), address.rbusModule());
    case Type::RBusPort:
        return qHashMulti(seed, value(address.type()), address.rbusPortAddress());

    case Type::LoconetSIC:
    case Type::Invalid:
        break;
    }

    return qHashMulti(seed, value(address.type()));
}

} // namespace lmrs::core::rm

QDebug operator<<(QDebug debug, const lmrs::core::dcc::FunctionState &functions);

#endif // LMRS_CORE_DCCCONSTANTS_H