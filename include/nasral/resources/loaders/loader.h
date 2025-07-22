#pragma once
#include <type_traits>
#include <string_view>
#include <optional>
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    template<typename DataType>
    class Loader
    {
        static_assert(std::is_move_constructible_v<DataType>, "DataType must be move constructible");
        static_assert(std::is_move_assignable_v<DataType>, "DataType must be move assignable");

    public:
        virtual std::optional<DataType> load(const std::string_view& path) = 0;
        virtual ~Loader() = default;

        [[nodiscard]] ErrorCode err_code() const { return err_code_; }

    protected:
        ErrorCode err_code_ = ErrorCode::eNoError;
    };
}
