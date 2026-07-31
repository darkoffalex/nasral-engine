#include "pch.h"
#include <nasral/scn/objects/light.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Light::Light(Manager* manager, const NodeDesc& description)
        : Spatial(manager, description)
    {
        // Параметры источника
        const auto& [
            dynamic,
            is_active,
            type,
            intensity,
            radius,
            quadratic,
            color] = description.light;

        // Добавить необходимые компоненты
        engine()->ecs()->add_components_immediate<
            Components::Light,
            Components::UniformIndex>(
                entity(),
                {type, intensity, radius, quadratic, color},
                {engine()->gfx()->light_ubo_ids().acquire()});

        // По динамическим объектам итерируемся всегда и проверяем не нужно ли пересчитать матрицы (dirty == true)
        // По статическим итерируемся лишь в том случае, если есть компонент DirtyUniform (редкие изменения)
        if (dynamic){
            engine()->ecs()->add_components_immediate<Components::UniformState>(entity(), {true});
        }else{
            engine()->ecs()->add_components_immediate<Components::DirtyUniform>(entity(), {});
        }

        // Если источник света должен быть видим (активирован)
        if (is_active){
            engine()->ecs()->add_components_immediate<Components::Activate>(entity(), {});
        }
    }

    Light::~Light()
    {
        // Деактивация и уничтожение
        engine()->ecs()->add_components_immediate<Components::Deactivate, ecs::DestroyComponent>(entity(), {}, {});
    }

    data::NodeView Light::data_view() const
    {
        const auto [id, name, node, spatial, light] = engine()->ecs()->get_components<
            Components::Uid,
            Components::Name,
            Components::Node,
            Components::Spatial,
            Components::Light
        >(entity());

        return data::LightNodeView{
            {
                {id.id,name.name,node.type},
                spatial.position,
                spatial.rotation,
                spatial.scale,
            },
            light.type,
            light.intensity,
            light.radius,
            light.quadratic,
            light.color
        };
    }

    Node::Ptr Light::clone() const
    {
        const auto data = std::get<data::LightNodeView>(Light::data_view());
        return Node::Ptr{new Light(subsystem(), {
            UniqueId::generate(),
            data.type,
            data.name,
            {data.position, data.rotation, data.scale},
            {},
            {
                is_dynamic(),
                true,
                data.light_type,
                data.intensity,
                data.radius,
                data.quadratic,
                data.color},
            {}
        })};
    }

    bool Light::is_dynamic() const
    {
        return engine()->ecs()->has<Components::UniformState>(entity());
    }

    void Light::set_position(const glm::vec3& position) const
    {
        Spatial::set_position(position);
        invalidate_ubo();
    }

    void Light::set_rotation(const glm::vec3& rotation) const
    {
        Spatial::set_rotation(rotation);
        invalidate_ubo();
    }

    void Light::set_scale(const glm::vec3& scale) const
    {
        Spatial::set_scale(scale);
        invalidate_ubo();
    }

    void Light::set_light_type(const gfx::LightType type) const
    {
        auto& settings = engine()->ecs()->get_component<LightComponent>(entity());
        settings.type = type;
        invalidate_ubo();
    }

    void Light::set_light_color(const glm::vec3& color) const
    {
        auto& settings = engine()->ecs()->get_component<LightComponent>(entity());
        settings.color = {color.x, color.y, color.z, 1.0f};
        invalidate_ubo();
    }

    void Light::set_light_intensity(const float intensity) const
    {
        auto& settings = engine()->ecs()->get_component<LightComponent>(entity());
        settings.intensity = intensity;
        invalidate_ubo();
    }

    void Light::set_light_radius(const float radius) const
    {
        auto& settings = engine()->ecs()->get_component<LightComponent>(entity());
        settings.radius = radius;
        invalidate_ubo();
    }

    void Light::set_light_quadratic(const float quadratic) const
    {
        auto& settings = engine()->ecs()->get_component<LightComponent>(entity());
        settings.quadratic = quadratic;
        invalidate_ubo();
    }

    void Light::invalidate_ubo() const
    {
        if (is_dynamic()){
            auto [ubo_state] = engine()->ecs()->get_components<Components::UniformState>(entity());
            ubo_state.is_dirty = true;
        }
        else if (!engine()->ecs()->has<Components::DirtyUniform>(entity())){
            engine()->ecs()->add_components<Components::DirtyUniform>(entity(), {});
        }
    }
}
