#ifndef LMRS_CORE_DEBUG_H
#define LMRS_CORE_DEBUG_H

#include "quantities.h"
#include "staticinit.h"
#include "userliterals.h"

#include <QLoggingCategory>

#define LMRS_FAILED(Logger, Assertion) \
    Q_UNLIKELY(::lmrs::core::logging::internal::reportFailure((Logger), (Assertion), #Assertion, \
                QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC))

#define LMRS_FAILED_COMPARE(Logger, ActualValue, Operation, ExpectedValue) \
    Q_UNLIKELY(::lmrs::core::logging::internal::reportFailure((Logger), (ActualValue), (ExpectedValue), \
                [](const auto &actual, const auto &expected) { return actual Operation expected; }, \
                "(" #ActualValue ") " #Operation " (" #ExpectedValue ")", \
                QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC))

#define LMRS_FAILED_EQUALS(Logger, ActualValue, ExpectedValue) \
    LMRS_FAILED_COMPARE((Logger), (ActualValue), ==, (ExpectedValue))

#define LMRS_CORE_DEFINE_LOGGER(Context) \
    static auto &logger(auto) = delete; \
    static auto &logger() { return core::logger<Context>(); }

#define LMRS_CORE_DEFINE_LOCAL_LOGGER(Context) namespace { \
    struct Context {}; \
    LMRS_CORE_DEFINE_LOGGER(Context) \
} // namespace

#define LMRS_UNIMPLEMENTED() \
    qCritical("Function %s() is not implemented", __func__)
#define LMRS_UNIMPLEMENTED_FOR_KEY(enumValue) \
    qCritical("Function %s() is not implemented for %s", __func__, core::key((enumValue)))
#define LMRS_UNIMPLEMENTED_FOR_METATYPE(metaType) \
    qCritical("Function %s() is not implemented for %s", __func__, (metaType).name())

namespace lmrs::core {

namespace logging {

template<class T>
class StaticInit : public core::StaticInit<StaticInit<T>, T>
{
protected:
    static void staticConstructor()
    {
        qSetMessagePattern("%{time process}/%{pid} "
                           "[%{type}%{if-category} %{category}%{endif}] "
                           "%{message} (%{file}, line %{line})"_L1);
    }

public:
    friend class core::StaticInitInjector<StaticInit<T>>;
    using core::StaticInit<StaticInit<T>, T>::StaticInit;
};

template<class T>
class StaticInitTesting : public core::StaticInit<StaticInitTesting<T>, T>
{
protected:
    static void staticConstructor()
    {
        qSetMessagePattern("%{if-category}[%{category}] %{endif}"
                           "%{message} (%{file}, line %{line})"_L1);
    }

public:
    friend class core::StaticInitInjector<StaticInitTesting<T>>;
    using core::StaticInit<StaticInitTesting<T>, T>::StaticInit;
};

} // logging

const QLoggingCategory &logger(QMetaType metaType, QMetaType detailType = {});

template<class T>
inline const QLoggingCategory &logger()
{
    static const auto &logger = core::logger(QMetaType::fromType<T>());
    return logger;
}

template<class T, typename Detail>
inline const QLoggingCategory &logger()
{
    static const auto &logger = core::logger(QMetaType::fromType<T>(), QMetaType::fromType<Detail>());
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

namespace logging::internal {

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

} // logging::internal

template<typename T>
class PrettyPrinter : public logging::internal::PrettyPrinterBase
{
public:
    PrettyPrinter(QDebug &debug) : PrettyPrinterBase{debug, QMetaType::fromType<T>()} {}
};

struct SeparatorState
{
    bool needed = false;

    QString next(QString separator = ", "_L1);
};

QDebug &operator<<(QDebug &debug, SeparatorState &separator);

namespace logging::internal {

template<typename T>
inline QString toString(const T &value)
{
    auto string = QString{};
    QDebug{&string} << value;
    return string;
}

bool reportFailure(const QLoggingCategory &category, bool assertion, const char *expression,
                   const char *file, int line, const char *func);

bool reportFailure(const QLoggingCategory &category, bool assertion,
                   QString actual, QString expected, const char *expression,
                   const char *file, int line, const char *func);

template<typename T, typename U, typename Compare>
inline bool reportFailure(const QLoggingCategory &category,
                          const T &actualValue, const U &expectedValue,
                          const Compare compare, const char *expression,
                          const char *file, int line, const char *func)
{
    return reportFailure(category, compare(actualValue, expectedValue),
                                   toString(actualValue), toString(expectedValue),
                                   expression, file, line, func);
}

}

} // namespace lmrs::core::logging::internal

inline QDebug operator<<(QDebug debug, const QMetaType &type)
{
    return debug << "QMetaType(" << type.name() << ")";
}

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
