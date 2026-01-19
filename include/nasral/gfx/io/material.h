#pragma once

#include <string>
#include <memory>
#include <nasral/core/io_struct.h>
#include <nasral/core/types.h>
#include <nasral/gfx/types.h>
#include <nasral/log/loggable.h>

namespace nasral::gfx::io
{
    struct Material : core::IOStruct, log::Loggable<Material>
    {
        typedef std::unique_ptr<Material> Ptr;

        struct Data {
            core::UniqueId id;
            MaterialType type;
            std::string material_path;
            core::EnumArray<TextureType, std::string> texture_paths;
            core::EnumArray<TextureType, TextureSamplerType> texture_samplers;
            uniforms::Material material_settings;
        } io_material_data;

        Material(Engine* engine, Data data);
        void unpack_to(const ecs::EntityId& entity_id, UnpackFlags flags) const override;
        void pack_from(ecs::EntityId& entity_id) override;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::io::Material)
