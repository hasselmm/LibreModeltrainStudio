#ifndef LMRS_CORE_STATICINIT_H
#define LMRS_CORE_STATICINIT_H

namespace lmrs::core {

template<class T>
class StaticInitInjector
{
public:
    StaticInitInjector() { T::staticConstructor(); }
};

template<class T, class BaseType>
class StaticInit
        : public StaticInitInjector<T>
        , public BaseType
{
public:
    using BaseType::BaseType;
};

} // namespace lmrs::core

#endif // LMRS_CORE_STATICINIT_H
