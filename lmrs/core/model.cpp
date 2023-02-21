#include "model.h"

#include "logging.h"

namespace lmrs::core {

QDebug operator<<(QDebug debug, const AccessoryInfo &info)
{
    const auto prettyPrinter = PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "address=" << info.address()
          << ", state=" << info.state();

    return debug;
}

QDebug operator<<(QDebug debug, const DetectorInfo &info)
{
    const auto prettyPrinter = PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "module=" << info.address()
          << ", occupancy=" << info.occupancy()
          << ", powerState=" << info.powerState()
          << ", vehicles=" << info.vehicles()
          << ", directions=" << info.directions();

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

QDebug operator<<(QDebug debug, const VehicleInfo &info)
{
    const auto prettyPrinter = PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "address=" << info.address()
          << ", direction=" << info.direction()
          << ", speed=" << info.speed()
          << ", flags=" << info.flags()
          << ", functionState=" << info.functionState();

    return debug;
}

} // namespace lmrs::core
