#ifndef LMRS_CORE_DCC_DCCREQUEST_H
#define LMRS_CORE_DCC_DCCREQUEST_H

#include "dccconstants.h"

class QDebug;

namespace lmrs::core::dcc {

class Request
{
public:
    Request() = default;
    explicit Request(QByteArray data) : m_data{std::move(data)} {}

    bool hasExtendedAddress() const;
    quint16 address() const;

    QByteArray toByteArray() const { return m_data; }

    static Request reset();
    static Request setSpeed14(quint16 address, quint8 speed, Direction direction, bool light);
    static Request setSpeed28(quint16 address, quint8 speed, Direction direction);
    static Request setSpeed126(quint16 address, quint8 speed, Direction direction);
    static Request setFunctions(quint16 address, FunctionGroup group, quint8 functions);
    static Request verifyBit(quint16 variable, bool value, quint8 position);
    static Request verifyByte(quint16 variable, quint8 value);
    static Request writeByte(quint16 variable, quint8 value);

private:
    QByteArray m_data;
};

QDebug operator<<(QDebug, Request);

} // namespace lmrs::core::dcc

#endif // LMRS_CORE_DCC_DCCREQUEST_H
