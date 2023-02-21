#include "algorithms.h"

#include <QString>

namespace lmrs::core {

QString hexString(QVariant value)
{
    {
        switch (value.typeId()) {
        case qMetaTypeId<qint8>():
            return hexString(value.value<qint8>());
        case qMetaTypeId<quint8>():
            return hexString(value.value<quint8>());
        case qMetaTypeId<qint16>():
            return hexString(value.value<qint16>());
        case qMetaTypeId<quint16>():
            return hexString(value.value<quint16>());
        case qMetaTypeId<qint32>():
            return hexString(value.value<qint32>());
        case qMetaTypeId<quint32>():
            return hexString(value.value<quint32>());
        case qMetaTypeId<qint64>():
            return hexString(value.value<qint64>());
        case qMetaTypeId<quint64>():
            return hexString(value.value<quint64>());
        }

        if (const auto metaEnum = metaEnumFromMetaType(value.metaType()); metaEnum.isValid()) {
            if (const auto key = metaEnum.valueToKey(value.toInt()))
                return QString::fromLatin1(key);

            return QString::fromLatin1(metaEnum.enumName()) + '('_L1 + QString::number(value.toInt()) + ')'_L1;
        }

        return QString::fromLatin1(value.typeName());
    }
}

template<class StringType>
std::optional<int> toInt(StringType text, int base)
{
    bool found = false;
    const auto value = text.toInt(&found, base);

    if (Q_LIKELY(found))
        return value;

    return {};
}

template<class StringType>
std::optional<qint64> toInt64(StringType text, int base)
{
    bool found = false;
    const auto value = text.toLongLong(&found, base);

    if (Q_LIKELY(found))
        return value;

    return {};
}

template std::optional<int> toInt(QByteArray, int);
template std::optional<int> toInt(QLatin1String, int);
template std::optional<int> toInt(QString, int);
template std::optional<int> toInt(QStringView, int);

template std::optional<qint64> toInt64(QByteArray, int);
template std::optional<qint64> toInt64(QLatin1String, int);
template std::optional<qint64> toInt64(QString, int);
template std::optional<qint64> toInt64(QStringView, int);

QMetaEnum metaEnumFromMetaType(QMetaType metaType)
{
    if (metaType.flags().testFlag(QMetaType::IsEnumeration))
        if (const auto metaObject = metaType.metaObject())
            if (const auto lastColon = strrchr(metaType.name(), ':'))
                if (const auto enumIndex = metaObject->indexOfEnumerator(lastColon + 1))
                    return metaObject->enumerator(enumIndex);

    return {};
}

} // namespace lmrs::core
