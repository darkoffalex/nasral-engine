#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/res/types.h>
#include <nasral/ecs/entity.h>

#include <nasral/ecs/components/gfx.h>
#include <nasral/ecs/components/common.h>
#include <nasral/ecs/components/res.h>

namespace nasral::gfx
{
    class Renderer;
    class MaterialInstance : public SubsystemObject<Renderer>, public log::Loggable<MaterialInstance>
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<MaterialInstance> Ptr;

        struct Components
        {
            using Uid = ecs::UidComponent;                                        // Уникальный ID
            using Name = ecs::NameComponent;                                      // Название конкретного instance
            using Handles = ecs::MaterialHandlesComponent;                        // Хендлы материала (pipeline, images)
            using Settings = ecs::MaterialSettingsComponent;                      // Настройки материала
            using UniformIndex = ecs::UniformIndexComponent;                      // Индекс UBO
            using UniformsDirty = ecs::DirtyUnformComponent;                      // Нужно обновить UBO
            using TextureDirty = ecs::DirtyTexturesComponent;                     // Нужно обновить текстуры
            using MaterialResource = ecs::ResourceComponent;                      // Ресурсы материала (для запроса)
            using TextureResources = ecs::ResourceListComponent<TextureType>;     // Ресурсы текстур (для запроса)
            using PendingDestroy = ecs::DestroyComponent;                         // Помечен к удалению
        };

        ~MaterialInstance();

        MaterialInstance(const MaterialInstance&) = delete;
        MaterialInstance& operator=(const MaterialInstance&) = delete;

        [[nodiscard]] const auto& entity() const {return entity_;}

        [[nodiscard]] const UniqueId& uid() const;
        [[nodiscard]] const std::string& name() const;
        [[nodiscard]] const MaterialBaseType& material_base_type() const;
        [[nodiscard]] const res::ResourceId& material_resource() const;
        [[nodiscard]] const EnumArray<TextureType, res::ResourceId>& texture_resources() const;
        [[nodiscard]] const EnumArray<TextureType, TextureSamplerType>& texture_samplers() const;
        [[nodiscard]] const uniforms::Material& uniforms() const;
        [[nodiscard]] uint32_t uniform_index() const;

        void set_name(const std::string& name) const;
        void set_uniforms(const uniforms::Material& uniforms) const;
        void set_texture_resource(TextureType type, res::ResourceId id) const;
        void set_texture_sampler(TextureType type, TextureSamplerType sampler_type) const;

    protected:
        MaterialInstance(Renderer* renderer, const MaterialDesc& description, const UniqueId& id);

    private:
        ecs::EntityId entity_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::MaterialInstance)