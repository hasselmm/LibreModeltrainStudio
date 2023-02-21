#ifndef LMRS_CORE_CONTINUATION_H
#define LMRS_CORE_CONTINUATION_H

#include <functional>

namespace lmrs::core {

enum class Continuation {
    Proceed,
    Retry,
    Abort,
    Done = Abort,
};

template<typename... Args>
using ContinuationCallbackBase = std::function<Continuation(Args...)>;

template<typename... Args>
class ContinuationCallback : public ContinuationCallbackBase<Args...>
{
public:
    static constexpr int DefaultRetryLimit = 3;

    using ContinuationCallbackBase<Args...>::ContinuationCallbackBase;

    ContinuationCallback(const ContinuationCallback &rhs, int retryCount, int retryLimit = DefaultRetryLimit)
        : ContinuationCallbackBase<Args...>{rhs}
        , m_retryCount{retryCount}
        , m_retryLimit{retryLimit}
    {}

    ContinuationCallback retry() const
    {
        if (*this && m_retryCount < m_retryLimit)
            return {*this, m_retryCount + 1, m_retryLimit};

        return {};
    }

    int retryCount() const { return m_retryCount; }
    int retryLimit() const { return m_retryLimit; }

private:
    int m_retryCount = 0;
    int m_retryLimit = DefaultRetryLimit;
};

template <typename Result, typename... Args>
Result callIfDefined(Result defaultResult, const std::function<Result(Args...)> &callable, Args... args)
{
    if (callable) // TODO: Figure out why ContinuationCallback doesn't get copied/moved properly
        return callable(args...);

    return defaultResult;
}

template <typename... Args>
void callIfDefined(const std::function<void(Args...)> &callable, Args... args)
{
    if (callable)
        callable(args...);
}

} // namespace lmrs::core

#endif // LMRS_CORE_CONTINUATION_H
