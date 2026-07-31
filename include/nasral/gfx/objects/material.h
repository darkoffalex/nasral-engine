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
    class MaterialInstance : public SubsystemObject<Manager>, public log::Loggable<MaterialInstance>
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<MaterialInstance> Ptr;

        enum ResIndices : size_t
        {
            eBaseMaterial       = 0,
            eTexAlbedo          = 1,
            eTexNormal          = 2,
            eTexRoughSpec       = 3,
            eTexHeight          = 4,
            eTexMetalReflect    = 5,
            eTexAO              = 6,
            eTexEmission        = 7,
        };

        static constexpr EnumArray<TextureType, ResIndices> kTexResMap = {
            eTexAlbedo,         // key: TextureType::eAlbedoColor
            eTexNormal,         // key: TextureType::eNormal
            eTexRoughSpec,      // key: TextureType::eRoughOrSpec
            eTexHeight,         // key: TextureType::eHeight
            eTexMetalReflect,   // key: TextureType::eMetalOrReflect
            eTexAO,             // key: TextureType::eAO
            eTexEmission        // key: TextureType::eEmission
        };

        struct Components
        {
            using Uid = ecs::UidComponent;                                        // Уникальный ID
            using Name = ecs::NameComponent;                                      // Название конкретного instance
            using Handles = MaterialHandlesComponent;                             // Handles материала (pipeline, images)
            using Settings = MaterialSettingsComponent;                           // Настройки материала
            using UniformIndex = UniformIndexComponent;                           // Индекс UBO
            using UniformsDirty = DirtyUnformComponent;                           // Нужно обновить UBO
            using TextureDirty = DirtyTexturesComponent;                          // Нужно обновить текстуры
            using HandlesDirty = DirtyHandlesComponent;                           // Нужно обновить handles
            using Resources = res::ResourcesComponent;                            // Ресурсы материала (для запроса)
            using PendingDestroy = ecs::DestroyComponent;                         // Помечен к удалению
            using RefsCount = ecs::RefsCountComponent;                            // Счетчик ссылок (для узлов сцены)
            using RefsChanged = ecs::RefsChangedComponent;                        // Ссылки изменились

            struct View
            {
                const UniqueId& uid;
                const std::string& name;
                const MaterialBaseType& base_type;
                const Resources::IdsList& resources;
                const EnumArray<TextureType, TextureSamplerType>& texture_samplers;
                const uniforms::Material& uniforms;
                const uint32_t uniform_index;
            };
        };

        ~MaterialInstance();

        MaterialInstance(const MaterialInstance&) = delete;
        MaterialInstance& operator=(const MaterialInstance&) = delete;

        [[nodiscard]] const ecs::EntityId& entity() const;
        [[nodiscard]] Components::View data_view() const;
        [[nodiscard]] std::string info(bool full = true) const;

        void set_name(const std::string& name) const;
        void set_uniforms(const uniforms::Material& uniforms) const;
        void set_texture_sampler(TextureType type, TextureSamplerType sampler_type) const;

    protected:
        MaterialInstance(Manager* manager, const MaterialDesc& description);

    private:
        ecs::EntityId entity_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::MaterialInstance, "GFX")
