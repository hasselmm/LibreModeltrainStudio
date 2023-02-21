#ifndef LMRS_CORE_STATICINIT_H
#define LMRS_CORE_STATICINIT_H

namespace lmrs::core {

template<class T>
class StaticInit
{
public:
    constexpr StaticInit() { T::staticConstructor(); }
};

}

#endif // LMRS_CORE_STATICINIT_H
