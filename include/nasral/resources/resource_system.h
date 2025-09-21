#pragma once
#include <nasral/ecs/ecs_manager.h>

namespace nasral::resources
{
    class ResourceSystem
    {
    public:
        typedef std::unique_ptr<ResourceSystem> Ptr;

        explicit ResourceSystem(Engine* engine);
        ~ResourceSystem();

        ResourceSystem(const ResourceSystem&) = delete;
        ResourceSystem& operator=(const ResourceSystem&) = delete;

        void update(float delta) const;

    private:
        void update_material_resources() const;
        void update_mesh_resources() const;

        [[nodiscard]] ecs::EcsManager* ecs() const;
        [[nodiscard]] const logging::Logger* logger() const;
        [[nodiscard]] ResourceManager* manager() const;

    protected:
        SafeHandle<Engine> engine_;
    };
}
