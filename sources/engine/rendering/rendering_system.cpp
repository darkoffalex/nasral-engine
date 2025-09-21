#include "pch.h"
#include <nasral/rendering/rendering_system.h>
#include <nasral/resources/resource_types.h>
#include <nasral/engine.h>

namespace res_cmp = nasral::resources::components;
namespace ren_cmp = nasral::rendering::components;

namespace nasral::rendering
{
    RenderingSystem::RenderingSystem(Engine* engine)
        : engine_(engine)
    {
        init_test_materials();
    }

    RenderingSystem::~RenderingSystem(){
        for (auto& material : materials_){
            ecs()->destroy_entity(material);
        }
    }

    void RenderingSystem::update([[maybe_unused]] float delta) const{
        // Запрос entity с нужными компонентами материала
        static auto material_view = ecs()->view<ren_cmp::MaterialSettings, ren_cmp::MaterialHandles>();

        // Итерация по материалам
        for (auto [id, settings, handles] : material_view){

            // Индекс материала в глобальном UBO
            auto index = to<uint32_t>(settings.index);

            // Обновление настроек материала (если нужно)
            if (settings.dirty){
                settings.dirty = false;
                std::visit([this, index](auto& uniforms) {
                    renderer()->update_material_ubo(index, uniforms);
                }, settings.uniforms);
            }

            // Обновление дескрипторов текстур (если нужно)
            for (size_t tex = 0; tex < to<size_t>(TextureType::TOTAL); ++tex){
                if (handles.texture_handles[tex] && handles.texture_dirty[tex]){
                    handles.texture_dirty[tex] = false;
                    renderer()->update_material_tex(index,{
                        to<TextureType>(tex),
                        handles.texture_samplers[tex],
                        handles.texture_handles[tex]
                    });
                }
            }
        }
    }

    void RenderingSystem::render() const{
        auto* r = renderer();

        r->cmd_begin_frame();
        r->cmd_bind_frame_descriptors();
        r->cmd_end_frame();
    }

    ecs::EcsManager* RenderingSystem::ecs() const{
        return engine_->ecs();
    }

    const logging::Logger* RenderingSystem::logger() const{
        return engine_->logger();
    }

    Renderer* RenderingSystem::renderer() const{
        return engine_->renderer();
    }

    void RenderingSystem::init_test_materials()
    {
        // Временное решение
        // В перспективе материалы сцены будут читаться из файла

        // Компоненты материала (формирование архетипа)
        const auto components = ecs::kMaskOf<
            res_cmp::MaterialDescriptors,
            res_cmp::MaterialRequest,
            ren_cmp::MaterialHandles,
            ren_cmp::MaterialSettings>;

        // Vertex colored material
        {
            const auto m = ecs()->create_entity();
            ecs()->enable_components(m, components);

            auto [descriptors, settings, request] = ecs()->get_components<
                res_cmp::MaterialDescriptors,
                ren_cmp::MaterialSettings,
                res_cmp::MaterialRequest>(m);

            request.needed = true;
            descriptors.material_path = valid_path("materials/vertex-colored/material.xml");
            descriptors.texture_paths = {};
            settings.index = materials_.size();
            settings.dirty = true;
            settings.uniforms = {};
            materials_.push_back(m);
        }

        // Phong material
        {
            const auto m = ecs()->create_entity();
            ecs()->enable_components(m, components);

            auto [descriptors, settings, request] = ecs()->get_components<
                res_cmp::MaterialDescriptors,
                ren_cmp::MaterialSettings,
                res_cmp::MaterialRequest>(m);

            request.needed = true;
            descriptors.material_path = valid_path("materials/phong/material.xml");
            descriptors.texture_paths[to<size_t>(TextureType::eAlbedoColor)] = valid_path("textures/chair/chair_diff_1k.png:v0");
            descriptors.texture_paths[to<size_t>(TextureType::eNormal)] = valid_path("textures/chair/chair_nor_gl_1k.png");
            descriptors.texture_paths[to<size_t>(TextureType::eRoughnessOrSpecular)] = valid_path("textures/chair/chair_spec_1k.png");
            settings.index = materials_.size();
            settings.dirty = true;
            settings.uniforms = MaterialPhongUniforms{};
            materials_.push_back(m);
        }

        // PBR material
        {
            const auto m = ecs()->create_entity();
            ecs()->enable_components(m, components);

            auto [descriptors, settings, request] = ecs()->get_components<
                res_cmp::MaterialDescriptors,
                ren_cmp::MaterialSettings,
                res_cmp::MaterialRequest>(m);

            request.needed = true;
            descriptors.material_path = valid_path("materials/pbr/material.xml");
            descriptors.texture_paths[to<size_t>(TextureType::eAlbedoColor)] = valid_path("textures/chair/chair_diff_1k.png:v1");
            descriptors.texture_paths[to<size_t>(TextureType::eNormal)] = valid_path("textures/chair/chair_nor_gl_1k.png");
            descriptors.texture_paths[to<size_t>(TextureType::eRoughnessOrSpecular)] = valid_path("textures/chair/chair_rough_1k.png");
            descriptors.texture_paths[to<size_t>(TextureType::eMetallicOrReflection)] = valid_path("textures/chair/chair_metal_1k.png");
            descriptors.texture_paths[to<size_t>(TextureType::eAmbientOcclusion)] = valid_path("textures/chair/chair_ao_1k.png");
            settings.index = materials_.size();
            settings.dirty = true;
            settings.uniforms = MaterialPbrUniforms{};
            materials_.push_back(m);
        }
    }

    std::string RenderingSystem::default_tex_path(const MaterialType m_type, const TextureType t_type){
        if (m_type == MaterialType::eDummy || m_type == MaterialType::eVertexColored){
            return "";
        }

        std::array<std::string, static_cast<size_t>(TextureType::TOTAL)> tex_paths = {
            resources::builtin_res_path(resources::BuiltinResources::eWhitePixel),  // albedo
            resources::builtin_res_path(resources::BuiltinResources::eNormalPixel), // normal
            resources::builtin_res_path(resources::BuiltinResources::eWhitePixel),  // roughness-specular
            resources::builtin_res_path(resources::BuiltinResources::eBlackPixel),  // height
            resources::builtin_res_path(resources::BuiltinResources::eBlackPixel),  // metallic-reflection
            resources::builtin_res_path(resources::BuiltinResources::eWhitePixel),  // ao
            resources::builtin_res_path(resources::BuiltinResources::eBlackPixel),  // emission
        };

        return tex_paths[static_cast<size_t>(t_type)];
    }

    std::string_view RenderingSystem::valid_path(const std::string& path) const{
        const auto view = engine_->resource_manager()->res_path(path);
        if (!view.has_value()){
            throw RenderingError("Can't find resource: " + path);
        }
        return view.value();
    }
}
