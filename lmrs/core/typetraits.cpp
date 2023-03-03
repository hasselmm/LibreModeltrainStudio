#include "typetraits.h"

namespace lmrs::core::tests {

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

} // namespace lmrs::core::tests
