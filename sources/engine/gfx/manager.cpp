#include "pch.h"
#include <nasral/gfx/manager.h>
#include <nasral/gfx/utils.h>
#include <nasral/evt/utils.h>
#include <nasral/res/objects/project.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    Manager::Manager(Engine* e, const Config& config)
        : Subsystem(e, config)
        , renderer_(std::make_unique<Renderer>(e, config))
        , object_ubo_ids_(kMaxObjects)
        , material_ubo_ids_(kMaxMaterials)
        , light_ubo_ids_(kMaxLights)
        , ecs_system_(std::make_unique<System>(this))
    {
        log_info("Initializing manager...");
    }

    Manager::~Manager(){
        log_info("Manager destroyed");
    }

    void Manager::on_init()
    {
        light_active_ids_.resize(kMaxLights);
        light_states_.resize(kMaxLights);

        // Слушать событие формирования списка ресурсов
        evl_res_reg_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eResourceRegistryChanged,
            evt::bind(this, &Manager::on_res_registry_changed));

        // Инициализация ECS системы
        ecs_system_->init();

        log_info("Manager initialized");
    }

    void Manager::on_update([[maybe_unused]] const float delta)
    {
        // Обновление ECS системы
        ecs_system_->update(delta);
    }

    void Manager::on_finalize()
    {
        // Уничтожение рендерера
        renderer_->cmd_wait_for_all();
        renderer_.reset();

        // Финализация ECS системы
        ecs_system_->finalize();

        // Отписаться от события формирования списка ресурсов (дизлайк, отписка!)
        evl_res_reg_.reset();

        // Уничтожение регистра материалов
        materials_.clear();

        log_info("Manager finalized");
    }

    void Manager::update_cam_uniforms(const uniforms::Camera& uniforms, const uint32_t index) const
    {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eView);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            ubo_offset<uniforms::Camera>(pd, index),
            aligned_ubo<uniforms::Camera>(pd),
            &uniforms);
    }

    void Manager::update_obj_uniforms(const uniforms::Object& uniforms, const uint32_t index) const
    {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eObjects);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            sbo_offset<uniforms::Object>(pd, index),
            aligned_sbo<uniforms::Object>(pd),
            &uniforms);
    }

    void Manager::update_mat_phong_uniforms(const uniforms::MaterialPhong& uniforms, const uint32_t index) const
    {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eObjects);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            sbo_offset<uniforms::MaterialPhong>(pd, index),
            aligned_sbo<uniforms::MaterialPhong>(pd),
            &uniforms);
    }

    void Manager::update_mat_pbr_uniforms(const uniforms::MaterialPbr& uniforms, const uint32_t index) const
    {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eObjects);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            sbo_offset<uniforms::MaterialPbr>(pd, index),
            aligned_sbo<uniforms::MaterialPbr>(pd),
            &uniforms);
    }

    void Manager::update_mat_textures(const TextureBindingInfo& info, const uint32_t index) const
    {
        const auto& ld = renderer()->vk_device().logical_device();
        const auto& ds = renderer()->vk_uniform_d_set(UniformDSetType::eMaterialTextures);
        const auto& ts = renderer()->vk_texture_sampler(info.sampler_type);

        assert(index < kMaxMaterials);
        assert(info.texture);
        assert(ds);

        vk::DescriptorImageInfo image_info{};
        image_info.setSampler(ts)
                  .setImageView(info.texture.image_view)
                  .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::WriteDescriptorSet write{};
        write.setDstSet(ds)
             .setDstBinding(static_cast<uint32_t>(info.type))
             .setDstArrayElement(index) // Индекс объекта в массиве дескрипторов
             .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
             .setDescriptorCount(1)
             .setImageInfo(image_info);

        ld.updateDescriptorSets({write}, {});
    }

    void Manager::update_light_uniforms(const uniforms::LightSettings& uniforms, const uint32_t index) const
    {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eLightSources);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            sbo_offset<uniforms::LightSettings>(pd, index),
            aligned_sbo<uniforms::LightSettings>(pd),
            &uniforms);
    }

    void Manager::update_light_states_unsafe(const std::vector<uint32_t>& ids, const bool active)
    {
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eLightSourcesActive);
        assert(ubo.is_mapped());

        // Обновить таблице состояний источников
        const uint8_t state_val = active ? 1 : 0;
        for (const auto& id : ids){
            if (id < light_states_.size()){
                light_states_[id] = state_val;
            }
        }

        // Пересобрать список активных индексов
        light_active_ids_.clear();
        for (uint32_t i = 0; i < static_cast<uint32_t>(light_states_.size()); ++i){
            if (light_states_[i]){
                light_active_ids_.push_back(i);
            }
        }

        // Обновить GPU storage buffer
        auto* pids = static_cast<uniforms::LightIndices*>(ubo.mapped_ptr());
        pids->count = static_cast<uint32_t>(light_active_ids_.size());
        std::fill_n(pids->indices, kMaxLights, 0);
        std::memcpy(pids->indices, light_active_ids_.data(), light_active_ids_.size() * sizeof(uint32_t));
    }

    void Manager::update_light_states(const std::vector<uint32_t>& ids, const bool active)
    {
        std::lock_guard lock(light_ids_mutex_);
        update_light_states_unsafe(ids, active);
    }

    void Manager::on_res_registry_changed(const evt::Arg& arg)
    {
        const auto reason = evt::from_arg<evt::ChangeReason>(arg);

        if (reason == evt::ChangeReason::eInitial)
        {
            const auto res_id = engine()->res()->find_project().value_or(res::kInvalidResourceId);
            auto* res = engine()->res()->get(res_id);
            const auto* proj = dynamic_cast<res::ProjectFile*>(res);

            assert(res && "Wrong project file resource");
            assert(res->status() == res::Status::eLoaded && "Project file resource is not loaded");
            assert(proj && "Project file resource is not a project file");

            // Сформировать список материалов
            for (const auto& mat_desc : proj->materials()){
                materials_.emplace_back(MaterialInstance::Ptr(new MaterialInstance(this, mat_desc)));
            }

            // Список ресурсов готов
            engine()->events()->send_deferred(
                evt::Type::eMaterialRegistryChanged,
                evt::ChangeReason::eInitial);
        }
    }
}
