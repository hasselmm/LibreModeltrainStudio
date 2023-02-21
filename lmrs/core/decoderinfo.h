#ifndef LMRS_CORE_DECODERINFO_H
#define LMRS_CORE_DECODERINFO_H

#include "dccconstants.h"

#include <QJsonObject>

namespace lmrs::core::dcc {

using DecoderId = literal<quint16, struct DecoderIdTag>;
using VendorId = literal<quint8, struct VendorIdTag>;

class DecoderField
{
public:
    explicit DecoderField(QJsonArray fields, int index);

    QString name() const;
    quint8 value(quint8 value) const;
    QString valueName(quint8 value) const;
    int width() const;

    QJsonArray flags() const;
    bool hasFlags() const;

private:
    QJsonObject d;
    int m_offset;
};

class DecoderVariable
{
public:
    DecoderVariable() = default;
    explicit DecoderVariable(QJsonObject object);

    bool isValid() const;

    QString name() const;
    QString type() const;

    DecoderField field(int index) const;
    qsizetype fieldCount() const;

    QJsonArray flags() const;
    qsizetype flagCount() const;

    QJsonValue values() const;

private:
    QJsonObject d;
};

class DecoderInfo
{
public:
    enum BaseType { Identity, Baseline, Functions, Vehicle, Railcom };

    explicit DecoderInfo(BaseType baseType); // FIXME: maybe fallback to dccconstants.h for building base types
    explicit DecoderInfo(QString decoderId);
    explicit DecoderInfo(DecoderId decoderId);
    explicit DecoderInfo(VariableValue vendorId, VariableValue decoderId);

    static QString id(VariableValue vendorId, VariableValue decoderId);
    static QString id(DecoderId decoderId);
    static QString id(BaseType baseType);

    bool isValid() const;
    QString id() const;

    QString name() const;
    bool hasRailcom() const;
    DecoderInfo parent() const;
    QSet<ExtendedVariableIndex> unsupportedVariableIds() const;

    enum VariableFilter {
        NoParent = (1 << 0),
        NoUnsupported = (1 << 1),
        NoFallback = (1 << 2),
    };

    Q_DECLARE_FLAGS(VariableFilters, VariableFilter)

    DecoderVariable variable(ExtendedVariableIndex index, VariableFilters filters = {}) const;
    QList<ExtendedVariableIndex> variableIds(VariableFilters filters = {}) const;

    enum DecoderFilter {
        NoAliases = (1 << 0),
    };

    Q_DECLARE_FLAGS(DecoderFilters, DecoderFilter)

    static QStringList knownDecoderIds(DecoderFilters filters = {});
    static QString vendorName(VendorId vendorId);

private:
    QJsonObject d;
    QString m_id;
};

} // namespace lmrs::core::dcc

#endif // LMRS_CORE_DECODERINFO_H
