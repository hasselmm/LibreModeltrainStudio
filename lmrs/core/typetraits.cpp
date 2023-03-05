#include "typetraits.h"

namespace lmrs::core {

namespace tests {

struct Base
{
    static const QMetaObject staticMetaObject;
};

struct Public
{
    static const QMetaObject staticMetaObject;

    struct Private : public Base
    {
        using PublicObjectType = Public;

        QT_WARNING_PUSH
        QT_WARNING_DISABLE_GCC("-Wtautological-compare")

        static_assert(&staticMetaObject == &Base::staticMetaObject);

        QT_WARNING_POP

#ifndef Q_CC_GNU
        static_assert(LMRS_STATIC_METAOBJECT != &Base::staticMetaObject);
        static_assert(LMRS_STATIC_METAOBJECT == &Public::staticMetaObject);
#endif
    };

    static_assert(IsPrivateObject<Private>);
    static_assert(LMRS_STATIC_METAOBJECT == &Public::staticMetaObject);
};

struct Simple
{
    static_assert(!IsPrivateObject<Private>);
    static const QMetaObject staticMetaObject;
    static_assert(LMRS_STATIC_METAOBJECT == &Simple::staticMetaObject);
};

} // namespace tests

QMetaType metaTypeFromClassName(QByteArrayView className)
{
    auto typeName = QByteArray{};
    typeName.reserve(className.size() + 1);
    typeName.append(std::move(className)).append('*');
    return QMetaType::fromName(std::move(typeName));
}

QMetaType metaTypeFromMetaObject(const QMetaObject *metaObject)
{
    return metaTypeFromClassName(metaObject->className());
}

QMetaType metaTypeFromObject(const QObject *object)
{
    return metaTypeFromMetaObject(object->metaObject());
}

} // namespace lmrs::core
