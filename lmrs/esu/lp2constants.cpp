#include "lp2constants.h"

namespace lmrs::esu::lp2 {

static_assert(QMetaType::fromType<InterfaceApplicationType>().name());
static_assert(QMetaType::fromType<InterfaceApplicationTypes>().name());
static_assert(QMetaType::fromType<InterfaceFlag>().name());
static_assert(QMetaType::fromType<InterfaceFlags>().name());
static_assert(QMetaType::fromType<InterfaceInfo>().name());

} // namespace lmrs::esu::lp2
