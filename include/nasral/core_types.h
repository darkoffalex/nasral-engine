#pragma once
#include <string>
#include <string_view>
#include <optional>
#include <cassert>
#include <stdexcept>

namespace nasral
{
    template<typename T, typename E>
    T to(E value){
        return static_cast<T>(value);
    }

    template<typename EnumType, typename Array>
    const std::string& name_of(
        const EnumType& value,
        const Array& names,
        const std::string& default_name = "")
    {
        const auto index = static_cast<size_t>(value);
        if (index >= names.size()) return default_name;
        return names[index];
    }

    template<typename EnumType, typename Array>
    std::optional<EnumType> enum_of(
        const std::string_view& name,
        const Array& names)
    {
        for (size_t i = 0; i < names.size(); ++i){
            if (names[i] == name) return static_cast<EnumType>(i);
        }
        return std::nullopt;
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