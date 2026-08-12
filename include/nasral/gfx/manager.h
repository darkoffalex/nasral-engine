#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/common/index_pool.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/gfx/renderer.h>
#include <nasral/gfx/system.h>
#include <nasral/gfx/objects/material.h>
#include <nasral/gfx/objects/post_processing.h>
#include <nasral/evt/objects/listener.h>

namespace nasral::gfx
{
    class Manager final : public Subsystem<Manager, Config>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;

        explicit Manager(Engine* e, const Config& config);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void update_cam_uniforms(const uniforms::Camera& uniforms, uint32_t index) const;
        void update_obj_uniforms(const uniforms::Object& uniforms, uint32_t index) const;
        void update_mat_uniforms(const uniforms::MaterialPhong& uniforms, uint32_t index) const;
        void update_mat_uniforms(const uniforms::MaterialPbr& uniforms, uint32_t index) const;
        void update_mat_textures(const TextureBindingInfo& info, uint32_t index) const;
        void update_light_uniforms(const uniforms::LightSettings& uniforms, uint32_t index) const;
        void update_light_states_unsafe(const std::vector<uint32_t>& ids, bool active);
        void update_light_states(const std::vector<uint32_t>& ids, bool active);

        [[nodiscard]] Renderer* renderer() const noexcept{ return renderer_.get(); }
        [[nodiscard]] System* ecs_system() const noexcept{ return ecs_system_.get(); }

        [[nodiscard]] auto& object_ubo_ids(){ return object_ubo_ids_; }
        [[nodiscard]] auto& material_ubo_ids(){ return material_ubo_ids_; }
        [[nodiscard]] auto& light_ubo_ids(){ return light_ubo_ids_; }

        void remove_material(const UniqueId& id);
        void remove_material(const ecs::EntityId& id);
        [[nodiscard]] MaterialInstance* find_material(const UniqueId& id) const;
        [[nodiscard]] MaterialInstance* find_material(const ecs::EntityId& id) const;

        void remove_post_processing(const UniqueId& id);
        void remove_post_processing(const ecs::EntityId& id);
        [[nodiscard]] PostProcessing* find_post_processing(const UniqueId& id) const;
        [[nodiscard]] PostProcessing* find_post_processing(const ecs::EntityId& id) const;

        void on_init();
        void on_update(float delta);
        void on_finalize();
        void on_render() const;

    protected:
        void on_res_registry_changed(const evt::Arg& arg);
        void on_display_surface_changed(const evt::Arg& arg) const;

    private:
        // Рендерер
        Renderer::Ptr renderer_;

        // Индексы и пулы индексов
        IndexPool<> object_ubo_ids_;
        IndexPool<> material_ubo_ids_;
        IndexPool<> light_ubo_ids_;

        // Активные источники света (их индексы)
        std::vector<uint32_t> light_active_ids_;
        std::vector<uint8_t> light_states_;
        std::mutex light_ids_mutex_;

        // Слушатели событий (загрузка проекта, смена размеров поверхности отображения)
        evt::Listener::Ptr evl_res_reg_;
        evt::Listener::Ptr evl_sfc_chg_;

        // Глобальный реестр материалов (конвейеров) растеризации (общий для проекта)
        std::vector<MaterialInstance::Ptr> materials_;
        // Глобальный реестр материалов (конвейеров) пост-процессинга (общий для проекта)
        std::vector<PostProcessing::Ptr> post_process_pipelines_;

        // ECS-система
        System::Ptr ecs_system_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::Manager, "GFX")
