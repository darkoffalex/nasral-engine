#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/common/index_pool.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/gfx/renderer.h>
#include <nasral/gfx/system.h>
#include <nasral/gfx/entities/material.h>
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

        void on_init();
        void on_update(float delta) const;
        void on_finalize();
        void on_render() const;

        void update_cam_uniforms(const uniforms::Camera& uniforms, uint32_t index) const;
        void update_obj_uniforms(const uniforms::Object& uniforms, uint32_t index) const;
        void update_mat_uniforms(const uniforms::Material& uniforms, uint32_t index) const;
        void update_mat_textures(const TextureBindingInfo& info, uint32_t index) const;
        void update_screen_pass_uniforms(const uniforms::ScreenPass& uniforms, uint32_t index) const;
        void update_light_uniforms(const uniforms::LightSettings& uniforms, uint32_t index) const;
        void update_light_states_unsafe(const std::vector<uint32_t>& ids, bool active);
        void update_light_states(const std::vector<uint32_t>& ids, bool active);

        ecs::EntityId add_material(const MaterialDesc& desc);
        [[nodiscard]] ecs::EntityId find_material(const UniqueId& id) const;
        void remove_material(const ecs::EntityId& id);
        void remove_material(const UniqueId& id);
        void clear_materials();

        /*
        ecs::EntityId add_screen_pass(const ScreenPassDesc& desc);
        [[nodiscard]] ecs::EntityId find_screen_pass(const UniqueId& id) const;
        void remove_screen_pass(const ecs::EntityId& id);
        void remove_screen_pass(const UniqueId& id);
        void clear_screen_passes();
        */

        [[nodiscard]] Renderer* renderer() const noexcept{ return renderer_.get(); }
        [[nodiscard]] System* ecs_system() const noexcept{ return ecs_system_.get(); }

        [[nodiscard]] auto& object_ubo_ids(){ return object_ubo_ids_; }
        [[nodiscard]] auto& material_ubo_ids(){ return material_ubo_ids_; }
        [[nodiscard]] auto& light_ubo_ids(){ return light_ubo_ids_; }

    protected:
        void on_res_registry_changed(const evt::Arg& arg);
        void on_display_surface_changed(const evt::Arg& arg) const;

    private:
        /// Рендерер
        Renderer::Ptr renderer_;
        /// ECS-система
        System::Ptr ecs_system_;

        /// Пулы индексов для объектов, материалов, экранных проходов, источников света
        IndexPool<> object_ubo_ids_;
        IndexPool<> material_ubo_ids_;
        IndexPool<> screen_pass_ubo_ids_;
        IndexPool<> light_ubo_ids_;

        /// Активные источники света (их индексы)
        std::vector<uint32_t> light_active_ids_;
        std::vector<uint8_t> light_states_;
        std::mutex light_ids_mutex_;

        /// Слушатели событий (обновление регистра ресурсов, изменение размеров поверхности)
        evt::Listener::Ptr evl_res_reg_;
        evt::Listener::Ptr evl_sfc_chg_;

        /// Глобальный реестр материалов
        std::list<ecs::EntityId> materials_;
        /// Глобальные реестр экранных проходов (пост-эффектов)
        std::list<ecs::EntityId> screen_passes_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::Manager, "GFX")