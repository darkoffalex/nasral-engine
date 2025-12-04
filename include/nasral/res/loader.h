#pragma once

#include <memory>
#include <type_traits>
#include <nasral/res/types.h>

namespace nasral::res
{
    template<typename Data>
    class Loader
    {
    public:
        typedef std::unique_ptr<Loader> Ptr;

        explicit Loader(const std::optional<LoadParams>& params = std::nullopt)
        : load_params_{params}
        {
            static_assert(std::is_move_constructible_v<Data>, "Data MUST be move-constructible.");
            static_assert(std::is_move_assignable_v<Data>, "Data MUST be move-assignable.");
        }

        virtual ~Loader() = default;

        virtual std::optional<Data> load(const std::string_view& path) = 0;

        [[nodiscard]] Error error() const noexcept{
            return error_;
        }

        [[nodiscard]] const LoadParams* load_params() const noexcept{
            return load_params_.has_value() ? &load_params_.value() : nullptr;
        }

    protected:
        Error error_ = Error::eNone;
        std::optional<LoadParams> load_params_;
    };
}
