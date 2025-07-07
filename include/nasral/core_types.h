#pragma once

namespace nasral
{
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
}