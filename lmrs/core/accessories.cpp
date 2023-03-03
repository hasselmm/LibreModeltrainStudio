#include "accessories.h"

#include "logging.h"

namespace lmrs::core::accessory {

QDebug operator<<(QDebug debug, const AccessoryInfo &info)
{
    const auto prettyPrinter = PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "address=" << info.address()
          << ", state=" << info.state();

    return debug;
}

QDebug operator<<(QDebug debug, const TurnoutInfo &info)
{
    const auto prettyPrinter = PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "address=" << info.address()
          << ", state=" << info.state();

    return debug;
}

} // namespace lmrs::core::accessory
