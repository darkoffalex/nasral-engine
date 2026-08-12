#include "pch.h"
#include <nasral/gfx/objects/post_processing.h>
#include <nasral/gfx/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    PostProcessing::PostProcessing(Manager* manager, const PostProcessingDesc& description)
        : SubsystemObject(manager)
        , entity_(ecs::EntityId::invalid())
    {
        // Активные ресурсы
        Components::Resources::IdsList resources_ids{};
        Components::Resources::ActiveList resources_active{};

        // Ресурс материала
        if (const auto mat_res_id = engine()->res()->find(description.material_path); mat_res_id.has_value()){
            resources_ids[eBaseMaterial] = mat_res_id.value();
            resources_active[eBaseMaterial] = true;
        }else{
            throw std::runtime_error("Material resource not found (" + description.material_path + ")");
        }

        // Создать Entity
        entity_ = engine()->ecs()->spawn();

        // Добавить компоненты
        engine()->ecs()->add_components_immediate<
            Components::Uid,
            Components::Name,
            Components::Handles,
            Components::HandlesDirty,
            Components::Resources,
            Components::RefsCount>(entity_,
                {description.unique_id},
                {description.name},
                {},
                {},
                {resources_ids, resources_active, {res::Status::eUnloaded}},
                {});

        // Информация о добавлении
        log_info("Post-processing instance registered (" + info() + ")");
    }

    PostProcessing::~PostProcessing()
    {
        // Если есть загруженные ресурсы на момент уничтожения объекта:
        // - Добавить в список освобождаемых
        // - Добавить в список уничтожаемых
        if (engine()->ecs()->has_any<res::LoadedComponent, res::LoadingComponent>(entity_))
        {
            engine()->ecs()->add_components_immediate<res::ReleaseComponent, ecs::DestroyComponent>(entity_, {}, {});
        }
        // Если нет загруженных ресурсов на момент уничтожения:
        // - Добавить в список уничтожаемых
        else
        {
            engine()->ecs()->add_components_immediate<ecs::DestroyComponent>(entity_, {});
        }

        log_info("Post-processing instance unregistered (" + info(false) + ")");
    }

    const ecs::EntityId& PostProcessing::entity() const{
        return entity_;
    }

    PostProcessing::Components::View PostProcessing::data_view() const
    {
        const auto* ecs = subsystem()->engine()->ecs();
        const auto [id, name, res] = ecs->get_components<
            Components::Uid,
            Components::Name,
            Components::Resources
        >(entity_);

        return {
            id.id,
            name.name,
            res.ids
        };
    }

    std::string PostProcessing::info(const bool full) const
    {
        const auto data = data_view();

        std::stringstream ss;
        ss << "UID: " << data.uid.to_string();

        if (full){
            ss  << ", Name: " << data.name
                << ", Resource ID: " << data.resources[eBaseMaterial];
        }

        return ss.str();
    }

    void PostProcessing::set_name(const std::string& name) const
    {
        const auto* ecs = subsystem()->engine()->ecs();
        auto& [name_c] = ecs->get_component<Components::Name>(entity_);
        name_c = name;
    }
}
