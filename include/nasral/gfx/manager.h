#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/common/index_pool.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/gfx/renderer.h>
#include <nasral/gfx/system.h>
#include <nasral/gfx/objects/material.h>
#include <nasral/evt/objects/listener.h>

namespace nasral::gfx
{
    class Manager final : public Subsystem<Manager, Config>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;
        using CmdGroupType = Renderer::CmdGroupType;

        explicit Manager(Engine* e, const Config& config);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void init();
        void update(float delta);
        void finalize();

        void update_cam_uniforms(const uniforms::Camera& uniforms, uint32_t index) const;
        void update_obj_uniforms(const uniforms::Object& uniforms, uint32_t index) const;
        void update_mat_phong_uniforms(const uniforms::MaterialPhong& uniforms, uint32_t index) const;
        void update_mat_pbr_uniforms(const uniforms::MaterialPbr& uniforms, uint32_t index) const;
        void update_mat_textures(const TextureBindingInfo& info, uint32_t index);
        void update_light_uniforms(const uniforms::LightSettings& uniforms, uint32_t index) const;
        void update_light_states_unsafe(const std::vector<uint32_t>& ids, bool active);
        void update_light_states(const std::vector<uint32_t>& ids, bool active);

        [[nodiscard]] Renderer* renderer() const noexcept{ return renderer_.get(); }
        [[nodiscard]] auto& object_ubo_ids(){ return object_ubo_ids_; }
        [[nodiscard]] auto& material_ubo_ids(){ return material_ubo_ids_; }
        [[nodiscard]] auto& light_ubo_ids(){ return light_ubo_ids_; }

    protected:
        void on_res_registry_changed(const evt::Arg& arg);

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

        // Слушатель события загрузки проекта
        evt::Listener::Ptr evl_res_reg_;

        // Глобальный реестр материалов (общий для проекта)
        std::vector<MaterialInstance::Ptr> materials_;

        // ECS-система
        System::Ptr ecs_system_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::Manager)
