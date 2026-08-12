#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/ecs/entity.h>

#include <nasral/gfx/components.h>
#include <nasral/ecs/components.h>
#include <nasral/res/components.h>

namespace nasral::gfx
{
    class Manager;
    class PostProcessing : public SubsystemObject<Manager>, public log::Loggable<PostProcessing>
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<PostProcessing> Ptr;

        enum ResIndices : size_t
        {
            eBaseMaterial = 0,
        };

        struct Components
        {
            using Uid = ecs::UidComponent;                                        // Уникальный ID
            using Name = ecs::NameComponent;                                      // Название конкретного instance
            using Handles = PostProcessHandlesComponent;                          // Handles материала (pipeline)
            using HandlesDirty = DirtyHandlesComponent;                           // Нужно обновить handles
            using Resources = res::ResourcesComponent;                            // Ресурсы материала (для запроса)
            using PendingDestroy = ecs::DestroyComponent;                         // Помечен к удалению
            using RefsCount = ecs::RefsCountComponent;                            // Счетчик ссылок (для узлов сцены)
            using RefsChanged = ecs::RefsChangedComponent;                        // Ссылки изменились

            struct View
            {
                const UniqueId& uid;
                const std::string& name;
                const Resources::IdsList& resources;
            };
        };

        ~PostProcessing();

        PostProcessing(const PostProcessing&) = delete;
        PostProcessing& operator=(const PostProcessing&) = delete;

        [[nodiscard]] const ecs::EntityId& entity() const;
        [[nodiscard]] Components::View data_view() const;
        [[nodiscard]] std::string info(bool full = true) const;

        void set_name(const std::string& name) const;

    protected:
        PostProcessing(Manager* manager, const PostProcessingDesc& description);

    private:
        ecs::EntityId entity_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::PostProcessing, "GFX")
