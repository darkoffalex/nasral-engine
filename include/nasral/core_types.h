#pragma once
#include <string>

namespace nasral
{
    struct EngineConfig
    {
        struct Log {
            std::string file;
            bool console_out = false;
        } log;

        struct Resources {
            std::string content_dir;
        } resources;
    };

    template<typename T>
    class SafeHandle
    {
    public:
        explicit SafeHandle(T* ptr) : ptr_(ptr) { assert(ptr_ != nullptr); }
        T* get() const { return ptr_; }
        T& operator*() const { assert(ptr_ != nullptr); return *ptr_; }
        T* operator->() const { assert(ptr_ != nullptr); return ptr_; }
    private:
        T* ptr_ = nullptr;
    };
}