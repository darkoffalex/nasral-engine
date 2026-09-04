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
    class ScreenFx : public SubsystemObject<Manager>, public log::Loggable<ScreenFx>
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<ScreenFx> Ptr;

        enum ResIndices : size_t
        {
            eFinalPassMaterial  = 0,
            eAOPassMaterial     = 1,
            eBlurPassMaterial   = 2,
        };

        static constexpr EnumArray<ScreenFxType, ResIndices> kMaterialResMap = {
            eFinalPassMaterial,     // key: ScreenFxType::eFinal
            eAOPassMaterial,        // key: ScreenFxType::eAO
            eBlurPassMaterial,      // key: ScreenFxType::eBlur
        };

        struct Components
        {
            using Uid = ecs::UidComponent;                                        // Уникальный ID
            using Name = ecs::NameComponent;                                      // Название конкретного instance
            using Handles = ScreenFxHandlesComponent;                             // Handles материала (pipeline)
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
                const EnumArray<ScreenFxType, handles::Material>& materials;
            };
        };

        ~ScreenFx();

        ScreenFx(const ScreenFx&) = delete;
        ScreenFx& operator=(const ScreenFx&) = delete;

        [[nodiscard]] const ecs::EntityId& entity() const;
        [[nodiscard]] Components::View data_view() const;
        [[nodiscard]] std::string info(bool full = true) const;

        void set_name(const std::string& name) const;

    protected:
        ScreenFx(Manager* manager, const ScreenFxDesc& description);

    private:
        ecs::EntityId entity_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::ScreenFx, "GFX")
