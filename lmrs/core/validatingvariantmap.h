#ifndef LMRS_CORE_VALIDATINGVARIANTMAP_H
#define LMRS_CORE_VALIDATINGVARIANTMAP_H

#include "algorithms.h"
#include "typetraits.h"

#include <QLoggingCategory>

namespace lmrs::core {

template<typename T>
using Validator = std::function<bool(const T &)>;

template<typename T>
Validator<T> defaultValidator()
{
    return [](const T &) {
        return true;
    };
}

template<core::HasIsEmpty T>
Validator<T> defaultValidator()
{
    return [](const T &value) {
        return !value.isEmpty();
    };
}

template<core::HasIsNull T>
requires(!core::HasIsEmpty<T>)
Validator<T> defaultValidator()
{
    return [](const T &value) {
        return !value.isNull();
    };
}

template<core::EnumType T>
Validator<T> defaultValidator()
{
    return [](const T &value) {
        return core::key(value);
    };
}

template<typename T>
std::optional<T> findParameter(const QVariantMap *parameters, const QLoggingCategory &logger,
                               QString key, Validator<T> validate = defaultValidator<T>())
{
    const auto it = parameters->find(key);

    if (it == parameters->end()) {
        qCWarning(logger, "Parameter \"%ls\" not found", qUtf16Printable(key));
        return {};
    }

    const auto optional = core::convert_if<T>(*it);

    if (!optional.has_value()) {
        qCWarning(logger, "Parameter \"%ls\" has unsupported value of type \"%s\" (%d)",
                  qUtf16Printable(key), it->typeName(), it->typeId());
        return {};
    }

    if (!validate(optional.value())) {
        qCWarning(logger, "Parameter \"%ls\" has unsupported value", qUtf16Printable(key));
        return {};
    }

    return optional;
}

template<class Context>
class ValidatingVariantMap : public QVariantMap
{
public:
    using QVariantMap::QVariantMap;

    ValidatingVariantMap(QVariantMap &&parameters)
        : QVariantMap{std::move(parameters)}
    {}

    template<typename T>
    std::optional<T> find(QString key, Validator<T> validate = defaultValidator<T>()) const
    {
        return findParameter(this, core::logger<Context>(), std::move(key), std::move(validate));
    }

    template<typename T>
    std::optional<T> find(QByteArrayView key, Validator<T> validate = defaultValidator<T>()) const
    {
        return findParameter(this, core::logger<Context>(), QString::fromLatin1(std::move(key)), std::move(validate));
    }
};

} // namespace lmrs::core

#endif // LMRS_CORE_VALIDATINGVARIANTMAP_H
