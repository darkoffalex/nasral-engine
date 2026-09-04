#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/common/index_pool.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/gfx/renderer.h>
#include <nasral/gfx/system.h>
#include <nasral/gfx/objects/material.h>
#include <nasral/gfx/objects/screen_fx.h>
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

        void remove_screen_fx(const UniqueId& id);
        void remove_screen_fx(const ecs::EntityId& id);
        [[nodiscard]] ScreenFx* find_screen_fx(const UniqueId& id) const;
        [[nodiscard]] ScreenFx* find_screen_fx(const ecs::EntityId& id) const;

        void on_init();
        void on_update(float delta);
        void on_finalize();
        void on_render() const;

    protected:
        void on_res_registry_changed(const evt::Arg& arg);
        void on_display_surface_changed(const evt::Arg& arg) const;
        void on_screen_fx_changed(const evt::Arg& arg);

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
        evt::Listener::Ptr evl_sfx_chg_;

        // Глобальный реестр материалов сцены (используются при рендеринге в основных проходах)
        std::vector<MaterialInstance::Ptr> materials_;
        // Глобальный реестр эффектов экрана (проходы пост-обработки)
        std::vector<ScreenFx::Ptr> screen_fxs_;

        // Handles активного эффекта материала
        EnumArray<ScreenFxType, handles::Material> screen_fx_materials_;

        // ECS-система
        System::Ptr ecs_system_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::Manager, "GFX")
