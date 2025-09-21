#pragma once

#include <string_view>
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    class ResourceManager;
    class Request
    {
    public:
        friend class ResourceManager;
        Request() = default;
        Request(ResourceManager* manager, const std::string& path, RequestCallback on_ready);
        Request(ResourceManager* manager, const std::string_view& path, RequestCallback on_ready);
        ~Request();

        Request(Request&& other) noexcept;
        Request(const Request&) = delete;
        Request& operator=(const Request&) = delete;
        Request& operator=(Request&& other) noexcept;

        [[nodiscard]] bool is_requested() const noexcept;
        [[nodiscard]] bool is_unhandled() const noexcept;

    private:
        RequestIdOpt id_ = std::nullopt;
        std::string_view path_= {};
        SafeHandle<ResourceManager> manager_ = {};
    };
}