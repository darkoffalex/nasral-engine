#include "pch.h"
#include <nasral/engine.h>
#include <nasral/resources/resource_system.h>
#include <nasral/resources/resource_components.h>
#include <nasral/rendering/rendering_components.h>
#include <nasral/resources/material.h>
#include <nasral/resources/texture.h>

namespace res_cmp = nasral::resources::components;
namespace ren_cmp = nasral::rendering::components;

namespace nasral::resources
{
    ResourceSystem::ResourceSystem(Engine* engine)
        : engine_(engine)
    {}

    ResourceSystem::~ResourceSystem(){
        manager()->finalize();
    }

    void ResourceSystem::update([[maybe_unused]] const float delta) const{
        // Для сущностей материалов
        update_material_resources();
        // Для сущностей объектов сцены
        update_mesh_resources();
        // Обновление ресурсов
        manager()->update(delta);
    }

    void ResourceSystem::update_material_resources() const {
        // Запрос entity с компонентами материала
        static auto material_view = ecs()->view<
            res_cmp::MaterialDescriptors,
            res_cmp::MaterialRequest,
            ren_cmp::MaterialHandles>();

        // Итерация по материалам
        for (auto [id, descriptors, requests, handles] : material_view)
        {
            auto entity_id = id;

            // Материал нужен
            if (requests.needed){
                // Не был запрошен
                if (!requests.pipeline_request.is_requested()){
                    // Запросить материал
                    requests.pipeline_request = manager()->make_request(
                        descriptors.material_path,
                        [this, entity_id](IResource* resource){
                            if (!ecs()->entity_valid(entity_id)) return;
                            if (const auto* m = dynamic_cast<Material*>(resource)){
                                auto& h = ecs()->get_component<ren_cmp::MaterialHandles>(entity_id);
                                h.material_handles = m->render_handles();
                            }
                        });

                    // Запросить текстуры
                    for (std::size_t i = 0; i < requests.texture_requests.size(); ++i){
                        auto& path = descriptors.texture_paths[i];
                        auto& request = requests.texture_requests[i];
                        if (!path.empty()){
                            request = manager()->make_request(path, [this, i, entity_id](IResource* resource){
                                if (!ecs()->entity_valid(entity_id)) return;
                                if (const auto* t = dynamic_cast<Texture*>(resource)){
                                    auto& h = ecs()->get_component<ren_cmp::MaterialHandles>(entity_id);
                                    h.texture_handles[i] = t->render_handles();
                                    h.texture_samplers[i] = rendering::TextureSamplerType::eNearest;
                                    h.texture_dirty[i] = true;
                                }
                            });
                        }
                    }
                }
            }
            // Материал не нужен, но был запрошен - освободить
            else if (requests.pipeline_request.is_requested()){
                ecs()->reset_component<ren_cmp::MaterialHandles>(entity_id);
                ecs()->reset_component<res_cmp::MaterialRequest>(entity_id);
            }
        }
    }

    void ResourceSystem::update_mesh_resources() const {
        // TODO: Обработка сущностей объектов сцены
    }

    ecs::EcsManager* ResourceSystem::ecs() const{
        return engine_->ecs();
    }

    const logging::Logger* ResourceSystem::logger() const{
        return engine_->logger();
    }

    ResourceManager* ResourceSystem::manager() const{
        return engine_->resource_manager();
    }
}
