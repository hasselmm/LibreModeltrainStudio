#include "logging.h"

namespace lmrs::core {

QByteArray logging::categoryName(QMetaType metaType, QMetaType detailType)
{
    if (auto detailName = detailType.name(); detailName && detailType.id() != qMetaTypeId<void>()) {
        if (const auto lastColon = strrchr(detailName, ':'))
            detailName = lastColon + 1;

        const auto suffix = QByteArray::fromRawData(detailName, static_cast<int>(strlen(detailName)));
        return categoryName(std::move(metaType)) + '.' + suffix.toLower();
    }

    if (const auto typeName = metaType.name()) {
        auto categoryName = QByteArray::fromRawData(typeName, static_cast<int>(strlen(typeName)));

#if defined(Q_CC_CLANG)
        categoryName.replace("(anonymous namespace)::", QByteArray{});
#elif defined(Q_CC_GNU)
        categoryName.replace("{anonymous}::", QByteArray{});
#endif

        categoryName.replace("::", "."); // separate by dots instead of colons
        return categoryName.toLower();
    }

    return {};
}

QDebug &operator<<(QDebug &debug, SeparatorState &separator)
{
    if (separator.needed)
        debug << ", ";

    separator.needed |= true;
    return debug;
}

const char *shortTypeName(QMetaType metaType)
{
    const auto fullName = metaType.name();
    const auto shortName = strrchr(fullName, ':');
    return shortName ? shortName + 1 : fullName;
}

logging::PrettyPrinterBase::PrettyPrinterBase(QDebug &debug, QMetaType metaType)
    : m_metaType{std::move(metaType)}
    , m_stateSaver{debug}
    , m_debug{debug}
{
    debug.nospace();

    if (debug.verbosity() > 1)
        debug << m_metaType.name();
    else if (debug.verbosity() > 0)
        debug << shortTypeName(m_metaType);

    debug << '(';
}

logging::PrettyPrinterBase::~PrettyPrinterBase()
{
    m_debug << ')';
}

bool logging::internal::reportFailure(const QLoggingCategory &category,
                                      bool assertion, const char *expression,
                                      const char *file, int line, const char *func)
{
    if (Q_UNLIKELY(!assertion)) {
        if (category.isCriticalEnabled()) {
            auto logger = QMessageLogger{file, line, func, category.categoryName()};
            logger.critical("Assertion has failed: %s", expression);
        }

        return true;
    }

    return false;
}

bool logging::internal::reportFailure(const QLoggingCategory &category, bool assertion,
                                      QString actual, QString expected, const char *expression,
                                      const char *file, int line, const char *func)
{
    if (Q_UNLIKELY(!assertion)) {
        if (category.isCriticalEnabled()) {
            auto logger = QMessageLogger{file, line, func, category.categoryName()};
            logger.critical("Assertion has failed: %s (actual value: %ls, expected value: %ls)",
                            expression, qUtf16Printable(actual.trimmed()), qUtf16Printable(expected.trimmed()));
        }

        return true;
    }

    return false;
}

} // namespace lmrs::core
