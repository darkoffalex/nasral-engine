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
        Ref();
        Ref(ResourceManager* manager, Type type, const std::string& path);
        Ref(const Ref& other);
        Ref& operator=(const Ref& other);
        ~Ref();

        void request();
        void release();
        void set_path(const std::string& path);
        void set_callback(std::function<void(IResource*)> callback);

        [[nodiscard]] const IResource* resource() const;

        [[nodiscard]] Type type() const { return type_; }
        [[nodiscard]] const FixedPath& path() const { return path_; }
        [[nodiscard]] const std::optional<size_t>& index() const { return resource_index_; }
        [[nodiscard]] bool is_requested() const { return is_requested_; }
        [[nodiscard]] bool is_handled() const { return is_handled_; }
        [[nodiscard]] SafeHandle<ResourceManager> manager() const { return manager_; }

    protected:
        Type type_;
        FixedPath path_;
        std::optional<size_t> resource_index_;
        bool is_requested_;
        bool is_handled_;
        SafeHandle<ResourceManager> manager_;
        std::function<void(IResource*)> on_ready_;
    };
}
