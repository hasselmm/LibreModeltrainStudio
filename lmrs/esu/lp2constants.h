#ifndef LMRS_ESU_LP2CONSTANTS_H
#define LMRS_ESU_LP2CONSTANTS_H

#include <lmrs/core/dccconstants.h>

namespace lmrs::esu {
namespace dcc = lmrs::core::dcc;
} // namespace

namespace lmrs::esu::lp2 {

Q_NAMESPACE

enum class InterfaceApplicationType : quint8
{
    None        = 0x00,
    MultiDcc    = 0x01,
    M4          = 0x02,
};

Q_FLAG_NS(InterfaceApplicationType)
Q_DECLARE_FLAGS(InterfaceApplicationTypes, InterfaceApplicationType)

enum class InterfaceFlag : quint8
{
    None                    = 0x00,
    NeedsApplicationCode    = 0x01,
};

Q_FLAG_NS(InterfaceFlag)
Q_DECLARE_FLAGS(InterfaceFlags, InterfaceFlag)

enum class InterfaceInfo : quint8
{
    ManufacturerId  = 0x00,
    ProductId       = 0x01,
    SerialNumber    = 0x02,
    ProductionDate  = 0x03,
    BootloaderCode  = 0x04,
    BootloaderDate  = 0x05,
    ApplicationCode = 0x06,
    ApplicationDate = 0x07,
    ApplicationType = 0x08,
};

Q_ENUM_NS(InterfaceInfo)

} // namespace lmrs::esu::lp2

#endif // LMRS_ESU_LP2CONSTANTS_H
