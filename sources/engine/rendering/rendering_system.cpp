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
        init_test_meshes();
        init_test_cameras();
        init_test_lights();
    }

    RenderingSystem::~RenderingSystem(){
        // Ожидать завершения кадра
        renderer()->cmd_wait_for_frame();

        // Очистка материалов (для корректного освобождения ресурсов)
        for (auto& material : materials_){
            renderer()->material_id_release_unsafe(material.index);
            ecs()->destroy_entity(material);
        }

        // Очистка объектов сцены (для корректного освобождения ресурсов)
        for (auto& mesh : meshes_){
            renderer()->obj_id_release_unsafe(mesh.index);
            ecs()->destroy_entity(mesh);
        }

        // Освобождение источников света
        for (auto& light : lights_){
            renderer()->light_id_release_unsafe(light.index);
            ecs()->destroy_entity(light);
        }
    }

    void RenderingSystem::update([[maybe_unused]] float delta) const{
        // Запрос entity с нужными компонентами материала
        static auto material_view = ecs()->view<ren_cmp::MaterialSettings, ren_cmp::MaterialHandles>();

        // Запрос entity с нужными компонентами объектов
        static auto meshes_view = ecs()->view<ren_cmp::ObjectSettings, ren_cmp::MeshHandles>();

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

        // Итерация по объектам сцены (mesh'ам)
        // ВАЖНО: В перспективе данная часть будет выполняться системой сцены
        for (auto [id, settings, handles] : meshes_view){

            // Обновление матриц
            if (settings.dirty){
                settings.dirty = false;

                ObjectTransformUniforms uniforms = {};
                auto& model = uniforms.model;
                auto& normals = uniforms.normals;
                model = glm::translate(model, settings.position);
                model = glm::rotate(model, glm::radians(settings.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::rotate(model, glm::radians(settings.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(settings.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::scale(model, settings.scale);
                normals = glm::transpose(glm::inverse(glm::mat3(model)));

                renderer()->update_obj_ubo(settings.index, uniforms);
            }

            // Обновление положение объектов (временно)
            static float angle = 0.0f;
            angle += delta * 10.0f;

            settings.dirty = true;
            settings.rotation = glm::vec3(10.0f, angle, 0.0f);
        }

        // Итерация по камерам сцены
        // ВАЖНО: В перспективе данная часть будет выполняться системой сцены
        for (auto [id, settings] : ecs()->view<ren_cmp::CameraSettings>()){

            if (settings.dirty){
                settings.dirty = false;

                CameraUniforms uniforms = {};
                uniforms.position = glm::vec4(settings.position, 1.0f);
                uniforms.view = glm::translate(glm::mat4(1.0f), -glm::vec3(settings.position));
                uniforms.projection = glm::perspective(
                    glm::radians(settings.fov),
                    renderer()->get_rendering_aspect(),
                    0.1f,
                    100.0f);

                renderer()->update_cam_ubo(0, uniforms);
            }
        }

        // Итерация по источникам света
        // ВАЖНО: В перспективе данная часть будет выполняться системой сцены
        for (auto [id, settings] : ecs()->view<ren_cmp::LightSettings>()){
            if (settings.dirty_state){
                settings.dirty_state = false;

                if (settings.active){
                    renderer()->light_ids_activate_unsafe({settings.index});
                }else{
                    renderer()->light_ids_deactivate_unsafe({settings.index});
                }
            }

            if (settings.dirty_settings){
                settings.dirty_settings = false;
                renderer()->update_light_ubo(settings.index, settings.uniforms);
            }
        }
    }

    void RenderingSystem::render() const{
        auto* r = renderer();

        // Начать рисование кадра
        r->cmd_begin_frame();
        r->cmd_bind_frame_descriptors();

        // Итерация по объектам для рисования
        for (auto [id, settings, handles] : ecs()->view<ren_cmp::ObjectSettings, ren_cmp::MeshHandles>()){

            // Информация о материале объекта
            auto [mat_settings, mat_handles] = ecs()->get_components<
                ren_cmp::MaterialSettings,
                ren_cmp::MaterialHandles>(settings.material_entity);

            // Если меш и материал не готов
            if (!handles.mesh_handles || !mat_handles.material_handles){
                continue;
            }

            // Нарисовать объект
            renderer()->cmd_bind_material(mat_handles.material_handles, mat_settings.index);
            renderer()->cmd_draw_mesh(handles.mesh_handles, settings.index);
        }

        // Завершить кадр
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
            settings.index = renderer()->material_id_acquire();
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
            settings.index = renderer()->material_id_acquire();
            settings.dirty = true;
            settings.uniforms = MaterialPhongUniforms{
                glm::vec4(0.7f, 0.7f, 0.7f, 1.0f),
                glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
                32.0f,
                0.5f
            };
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
            settings.index = renderer()->material_id_acquire();
            settings.dirty = true;
            settings.uniforms = MaterialPbrUniforms{
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                1.0f,
                1.0f
            };
            materials_.push_back(m);
        }
    }

    void RenderingSystem::init_test_meshes(){

        // Временное решение
        // В перспективе объекты сцены будут читаться из файла

        // Компоненты mesh'а (формирование архетипа)
        const auto components = ecs::kMaskOf<
            res_cmp::MeshDescriptor,
            res_cmp::MeshRequest,
            ren_cmp::MeshHandles,
            ren_cmp::ObjectSettings>;

        // Объект слева
        {
            const auto m = ecs()->create_entity();
            ecs()->enable_components(m, components);

            auto [descriptors, settings, request] = ecs()->get_components<
                res_cmp::MeshDescriptor,
                ren_cmp::ObjectSettings,
                res_cmp::MeshRequest>(m);

            request.needed = true;
            descriptors.mesh_path = valid_path("meshes/chair/chair.obj");
            settings.index = renderer()->obj_id_acquire();
            settings.material_entity = materials_[1];
            settings.dirty = true;
            settings.position = glm::vec3(-0.6f, -0.2f, 0.0f);
            settings.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            settings.scale = glm::vec3(1.5f, 1.5f, 1.5f);
            meshes_.push_back(m);
        }

        // Объект справа
        {
            const auto m = ecs()->create_entity();
            ecs()->enable_components(m, components);

            auto [descriptors, settings, request] = ecs()->get_components<
                res_cmp::MeshDescriptor,
                ren_cmp::ObjectSettings,
                res_cmp::MeshRequest>(m);

            request.needed = true;
            descriptors.mesh_path = valid_path("meshes/chair/chair.obj");
            settings.index = renderer()->obj_id_acquire();
            settings.material_entity = materials_[2];
            settings.dirty = true;
            settings.position = glm::vec3(0.6f, -0.2f, 0.0f);
            settings.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            settings.scale = glm::vec3(1.5f, 1.5f, 1.5f);
            meshes_.push_back(m);
        }
    }

    void RenderingSystem::init_test_cameras() const{

        // Временное решение
        // В перспективе объекты сцены будут читаться из файла

        // Компоненты камеры (формирование архетипа)
        const auto cam = ecs()->create_entity();
        ecs()->enable_components(cam, ecs::kMaskOf<ren_cmp::CameraSettings>);

        auto& [position, rotation, fov, dirty] = ecs()->get_component<ren_cmp::CameraSettings>(cam);
        dirty = true;
        position = glm::vec3(0.0f, 0.0f, 2.5f);
        rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        fov = 45.0f;
    }

    void RenderingSystem::init_test_lights(){
        {
            const auto l = ecs()->create_entity();
            ecs()->enable_components(l, ecs::kMaskOf<ren_cmp::LightSettings>);

            auto& [id, dirty_state, dirty_settings, active, uniforms] = ecs()->get_component<ren_cmp::LightSettings>(l);
            id = renderer()->light_id_acquire();
            uniforms.position = glm::vec4(0.0, 2.0f, 3.0f, 0.0f);
            uniforms.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            uniforms.intensity = 4.0f;
            dirty_state = true;
            dirty_settings = true;
            active = true;
            lights_.push_back(l);
        }

        {
            const auto l = ecs()->create_entity();
            ecs()->enable_components(l, ecs::kMaskOf<ren_cmp::LightSettings>);

            auto& [id, dirty_state, dirty_settings, active, uniforms] = ecs()->get_component<ren_cmp::LightSettings>(l);
            id = renderer()->light_id_acquire();
            uniforms.position = glm::vec4(0.0f, -2.0f, 3.0f, 0.0f);
            uniforms.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            uniforms.intensity = 3.0f;
            dirty_state = true;
            dirty_settings = true;
            active = true;
            lights_.push_back(l);
        }
    }

    /*
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
    */

    std::string_view RenderingSystem::valid_path(const std::string& path) const{
        const auto view = engine_->resource_manager()->res_path(path);
        if (!view.has_value()){
            throw RenderingError("Can't find resource: " + path);
        }
        return view.value();
    }
}
