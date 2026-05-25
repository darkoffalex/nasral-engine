#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/res/types.h>
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

        struct Components
        {
            using Uid = ecs::UidComponent;                                        // Уникальный ID
            using Name = ecs::NameComponent;                                      // Название конкретного instance
            using Handles = MaterialHandlesComponent;                             // Хендлы материала (pipeline, images)
            using Settings = MaterialSettingsComponent;                           // Настройки материала
            using UniformIndex = UniformIndexComponent;                           // Индекс UBO
            using UniformsDirty = DirtyUnformComponent;                           // Нужно обновить UBO
            using TextureDirty = DirtyTexturesComponent;                          // Нужно обновить текстуры
            using MaterialResource = res::IdComponent;                            // Ресурсы материала (для запроса)
            using TextureResources = res::IdListComponent<TextureType>;           // Ресурсы текстур (для запроса)
            using PendingDestroy = ecs::DestroyComponent;                         // Помечен к удалению

            struct View
            {
                const UniqueId& uid;
                const std::string& name;
                const MaterialBaseType& base_type;
                const res::ResourceId& material_resource;
                const EnumArray<TextureType, res::ResourceId>& texture_resources;
                const EnumArray<TextureType, TextureSamplerType>& texture_samplers;
                const uniforms::Material& uniforms;
                const uint32_t uniform_index;
            };
        };

        ~MaterialInstance();

        MaterialInstance(const MaterialInstance&) = delete;
        MaterialInstance& operator=(const MaterialInstance&) = delete;

        [[nodiscard]] const ecs::EntityId& entity() const;
        [[nodiscard]] Components::View components() const;
        [[nodiscard]] std::string info_str(bool full = true) const;

        void set_name(const std::string& name) const;
        void set_uniforms(const uniforms::Material& uniforms) const;
        void set_texture_resource(TextureType type, res::ResourceId id) const;
        void set_texture_sampler(TextureType type, TextureSamplerType sampler_type) const;

    protected:
        MaterialInstance(Manager* renderer, const MaterialDesc& description);

    private:
        ecs::EntityId entity_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::MaterialInstance)
