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

        static_assert(&staticMetaObject == &Base::staticMetaObject);
        static_assert(LMRS_STATIC_METAOBJECT != &Base::staticMetaObject);
        static_assert(LMRS_STATIC_METAOBJECT == &Public::staticMetaObject);
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
