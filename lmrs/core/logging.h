#ifndef LMRS_CORE_DEBUG_H
#define LMRS_CORE_DEBUG_H

#include "quantities.h"

#include <QLoggingCategory>

#define LMRS_FAILED(Logger, Assertion) \
    Q_UNLIKELY(::lmrs::core::logging::internal::reportFailure((Logger), (Assertion), #Assertion, \
               QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC))

#define LMRS_FAILED_COMPARE(Logger, ActualValue, Operation, ExpectedValue) \
    LMRS_FAILED((Logger), (ActualValue) Operation (ExpectedValue))
#define LMRS_FAILED_EQUALS(Logger, ActualValue, ExpectedValue) \
    LMRS_FAILED_COMPARE((Logger), (ActualValue), ==, (ExpectedValue))
#define LMRS_FAILED_LESS_OR_EQUAL(Logger, ActualValue, ExpectedValue) \
    LMRS_FAILED_COMPARE((Logger), (ActualValue), <=, (ExpectedValue))
#define LMRS_FAILED_LESS_THAN(Logger, ActualValue, ExpectedValue) \
    LMRS_FAILED_COMPARE((Logger), (ActualValue), <, (ExpectedValue))

#define LMRS_CORE_DEFINE_LOGGER(Context) \
    static auto &logger(auto) = delete; \
    static auto &logger() { return core::logger<Context>(); }

#define LMRS_CORE_DEFINE_LOCAL_LOGGER(Context) namespace { \
    struct Context {}; \
    LMRS_CORE_DEFINE_LOGGER(Context) \
} // namespace

namespace lmrs::core {

namespace logging {

QByteArray categoryName(QMetaType metaType, QMetaType detailType = {});

template<class T, typename Detail = void>
inline auto categoryName()
{
    return categoryName(QMetaType::fromType<T>(), QMetaType::fromType<Detail>());
}

} // logging

template<class T>
inline const QLoggingCategory &logger()
{
    static const auto loggerName = logging::categoryName<T>();
    static const auto logger = QLoggingCategory{loggerName.constData()};
    return logger;
}

template<class T, typename Detail>
inline const QLoggingCategory &logger()
{
    static const auto loggerName = logging::categoryName<T, Detail>();
    static const auto logger = QLoggingCategory{loggerName.constData()};
    return logger;
}

template<class T>
inline const QLoggingCategory &logger(T *)
{
    return logger<T>();
}

const char *shortTypeName(QMetaType metaType);

template<class T>
const char *shortTypeName()
{
    static const auto shortName = shortTypeName(QMetaType::fromType<T>());
    return shortName;
}

namespace logging {

class PrettyPrinterBase
{
public:
    PrettyPrinterBase(QDebug &debug, QMetaType metaType);
    ~PrettyPrinterBase();

private:
    QMetaType m_metaType;
    QDebugStateSaver m_stateSaver;
    QDebug m_debug;
};

} // logging

template<typename T>
class PrettyPrinter : public logging::PrettyPrinterBase
{
public:
    PrettyPrinter(QDebug &debug) : PrettyPrinterBase{debug, QMetaType::fromType<T>()} {}
};

struct SeparatorState
{
    bool needed = false;
};

QDebug &operator<<(QDebug &debug, SeparatorState &separator);

namespace logging::internal {

bool reportFailure(const QLoggingCategory &category,
                   bool assertion, const char *expression,
                   const char *file, int line, const char *func);

}

} // namespace lmrs::core::logging::internal

template <typename T, typename Period>
inline QDebug operator<<(QDebug debug, std::chrono::duration<T, Period> duration)
{
    return debug << duration.count() << lmrs::core::unit<decltype(duration)>;
}

template <typename T, class Unit, class Ratio>
inline QDebug operator<<(QDebug debug, lmrs::core::quantity<T, Unit, Ratio> quantity)
{
    return debug << quantity.count() << lmrs::core::unit<decltype(quantity)>;
}

template <size_t i, typename... Args>
inline void print(QDebug debug, const std::tuple<Args...> &tuple)
{
    if constexpr(i < std::tuple_size<std::tuple<Args...>>()) {
        debug << (i == 0 ? "(" : ", ");
        debug << std::get<i>(tuple);
        print<i + 1>(debug, tuple);
    } else {
        debug << ')';
    }
}

template <typename... Args>
inline QDebug operator<<(QDebug debug, const std::tuple<Args...> &tuple)
{
    auto debugState = QDebugStateSaver{debug};
    print<0>(debug.nospace().verbosity(QDebug::MinimumVerbosity), tuple);
    return debug;
}

inline QDebug operator<<(QDebug debug, const std::monostate)
{
    return debug << "std::monostate{}";
}

#endif // LMRS_CORE_DEBUG_H
