#pragma once

namespace nasral
{
    template<typename T, typename E>
    T to(E value){
        return static_cast<T>(value);
    }

    template<typename T>
    class SafeHandle
    {
    public:
        SafeHandle() = default;
        explicit SafeHandle(T* ptr) : ptr_(ptr) { assert(ptr_ != nullptr); }
        T* get() const { return ptr_; }
        T& operator*() const { assert(ptr_ != nullptr); return *ptr_; }
        T* operator->() const { assert(ptr_ != nullptr); return ptr_; }
    private:
        T* ptr_ = nullptr;
    };

    class EngineError : public std::runtime_error
    {
    public:
        explicit EngineError(const std::string& message)
        : std::runtime_error(message)
        {}
        ~EngineError() override = default;
    };
}