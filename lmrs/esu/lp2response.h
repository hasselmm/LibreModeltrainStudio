#ifndef LMRS_ESU_LP2RESPONSE_H
#define LMRS_ESU_LP2RESPONSE_H

#include <lmrs/esu/lp2message.h>

class QDebug;

namespace lmrs::esu::lp2 {

struct InterfaceFlagsResponse : Response::Data<Response::Identifier::DeviceFlagsResponse, 1>
{
    Response::Status status() const;

    Q_GADGET
};

struct ValueResponse : Response::Data<Response::Identifier::ValueResponse, 1, 5>
{
    Response::Status status() const;

    template<typename DataType = QVariant>
    DataType value() const;

    Q_GADGET
};

struct InterfaceInfoResponse : public ValueResponse
{
    InterfaceInfo info;

    template<typename DataType = QVariant>
    DataType value() const;

    Q_GADGET
};

struct DccResponse : Response::Data<Response::Identifier::DccResponse, 1, 256>
{
    enum class Acknowledge {
        None = -1,
        Negative = 0,
        Positive = 1,
    };

    Q_ENUM(Acknowledge)

    Response::Status status() const;
    Acknowledge acknowledge() const;

    Q_GADGET
};

QDebug operator<<(QDebug debug, DccResponse response);
QDebug operator<<(QDebug debug, InterfaceFlagsResponse response);
QDebug operator<<(QDebug debug, InterfaceInfoResponse response);
QDebug operator<<(QDebug debug, ValueResponse response);

} // namespace lmrs::esu::lp2

#endif // LMRS_ESU_LP2RESPONSE_H
