#pragma once
#include <string>
#include <functional>
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    class ResourceManager;
    class Ref final
    {
    public:
        friend class ResourceManager;
        Ref(ResourceManager* manager, Type type, const std::string& path);
        ~Ref();

        Ref(const Ref&) = delete;
        Ref& operator=(const Ref&) = delete;

        void request();
        void release();
        void set_callback(const std::function<void(IResource*)>& callback);

        [[nodiscard]] Type type() const { return type_; }
        [[nodiscard]] const FixedPath& path() const { return path_; }
        [[nodiscard]] const std::optional<size_t>& index() const { return resource_index_; }
        [[nodiscard]] bool is_requested() const { return is_requested_; }

    private:
        Type type_;
        FixedPath path_;
        std::optional<size_t> resource_index_;
        bool is_requested_;
        ResourceManager* manager_ = nullptr;
        std::function<void(IResource*)> on_ready_;
    };
}
