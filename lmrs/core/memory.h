#ifndef LMRS_CORE_MEMORY_H
#define LMRS_CORE_MEMORY_H

namespace lmrs::core {

template<typename T>
class ConstPointer
{
public:
    template<class... Args>
    explicit ConstPointer(Args... args) : ConstPointer{new T{args...}} {}
    ConstPointer(T *ptr) noexcept : m_ptr{ptr} {}

    const T *operator->() const { return m_ptr; }
    operator const T *() const { return m_ptr; }
    const T *get() const { return m_ptr; }

    T *operator->() { return m_ptr; }
    operator T *() { return m_ptr; }
    T *get() { return m_ptr; }

    template<typename U>
    U *get() { return static_cast<U *>(m_ptr); }

private:
    T *const m_ptr;
};

} // namespace lmrs::core

#endif // LMRS_CORE_MEMORY_H
