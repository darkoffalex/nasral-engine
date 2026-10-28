#include "pch.h"
#include <nasral/gfx/manager.h>
#include <nasral/gfx/utils.h>
#include <nasral/evt/utils.h>
#include <nasral/res/objects/project.h>
#include <nasral/inp/provider.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    Manager::Manager(Engine *e, const Config &config)
        : Subsystem(e, config)
        , renderer_(std::make_unique<Renderer>(e, config))
        , ecs_system_(std::make_unique<System>(this))
        , object_ubo_ids_(kMaxObjects)
        , material_ubo_ids_(kMaxMaterials)
        , screen_pass_ubo_ids_(kMaxScreenPasses)
        , light_ubo_ids_(kMaxLights)
    {
        log_info("Initializing manager...");
    }

    Manager::~Manager() {
        log_info("Manager destroyed");
    }

    void Manager::on_init()
    {
        // Зарезервировать память для идентификаторов источников света
        light_active_ids_.resize(kMaxLights);
        light_states_.resize(kMaxLights);

        // Слушать событие формирования списка ресурсов
        evl_res_reg_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eResourceRegistryChanged,
            evt::bind(this, &Manager::on_res_registry_changed));

        // Слушать событие изменения поверхности отображения
        evl_sfc_chg_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eDisplaySurfaceChanged,
            evt::bind(this, &Manager::on_display_surface_changed));
    }

    void Manager::on_update(const float delta) const
    {
        if (engine()->run()->state().has_no(run::StateFlags::eRunning)) return;
        ecs_system_->update(delta);
    }

    void Manager::on_finalize() {
        // Сброс слушателей события
        evl_res_reg_.reset();
        evl_sfc_chg_.reset();

        // Очистка материалов и экранных проходов
        clear_materials();
        //clear_screen_passes();

        // Финализация ECS
        ecs_system_->finalize();
        log_info("Manager finalized");
    }

    void Manager::on_render() const {
        // TODO: Implement
    }

    void Manager::update_cam_uniforms(const uniforms::Camera &uniforms, const uint32_t index) const {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eView);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            ubo_offset<uniforms::Camera>(pd, index),
            aligned_ubo<uniforms::Camera>(pd),
            &uniforms);
    }

    void Manager::update_obj_uniforms(const uniforms::Object &uniforms, const uint32_t index) const {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eObjects);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            sbo_offset<uniforms::Object>(pd, index),
            aligned_sbo<uniforms::Object>(pd),
            &uniforms);
    }

    void Manager::update_mat_uniforms(const uniforms::Material &uniforms, const uint32_t index) const {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eMaterials);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            sbo_offset<biggest_of_t<uniforms::Material>>(pd, index),
            aligned_sbo<biggest_of_t<uniforms::Material>>(pd),
            &uniforms);
    }

    void Manager::update_mat_textures(const TextureBindingInfo &info, const uint32_t index) const {
        const auto& ld = renderer()->vk_device().logical_device();
        const auto& ds = renderer()->vk_rasterization_d_set(UniformRasterDSetType::eMaterialTextures);
        const auto& ts = renderer()->vk_texture_sampler(info.sampler_type);

        assert(index < kMaxMaterials);
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

    void Manager::update_screen_pass_uniforms(const uniforms::ScreenPass &uniforms, const uint32_t index) const {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eScreenPasses);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            sbo_offset<biggest_of_t<uniforms::ScreenPass>>(pd, index),
            aligned_sbo<biggest_of_t<uniforms::ScreenPass>>(pd),
            &uniforms);
    }

    void Manager::update_light_uniforms(const uniforms::LightSettings &uniforms, const uint32_t index) const {
        const auto& pd = renderer()->vk_device().physical_device();
        const auto& ubo = renderer()->vk_uniform_buffer(UniformBufferType::eLightSources);
        assert(ubo.is_mapped());

        ubo.update_mapped(
            sbo_offset<uniforms::LightSettings>(pd, index),
            aligned_sbo<uniforms::LightSettings>(pd),
            &uniforms);
    }

    void Manager::update_light_states_unsafe(const std::vector<uint32_t> &ids, const bool active) {
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

    void Manager::update_light_states(const std::vector<uint32_t> &ids, const bool active) {
        std::lock_guard lock(light_ids_mutex_);
        update_light_states_unsafe(ids, active);
    }

    ecs::EntityId Manager::add_material(const MaterialDesc &desc) {
        auto* ecs = engine()->ecs();
        const auto entity = ecs->spawn();
        MaterialView::setup(ecs, entity, desc);
        materials_.emplace_back(entity);
        return entity;
    }

    ecs::EntityId Manager::find_material(const UniqueId &id) const {
        for (auto e : materials_) {
            if (auto node = engine()->ecs()->as_view<MaterialView>(e); node.has_value()) {
                if (node->uid() == id) {
                    return node->id();
                }
            }
        }
        return ecs::EntityId::invalid();
    }

    void Manager::remove_material(const ecs::EntityId &id) {
        materials_.erase(std::remove_if(materials_.begin(), materials_.end(), [&](const auto& n){
            return n == id;
        }), materials_.end());

        engine()->ecs()->add_components<ecs::DestroyComponent>(id, {});
    }

    void Manager::remove_material(const UniqueId &id) {
        const auto to_erase = std::remove_if(materials_.begin(), materials_.end(), [&](const auto& n){
            auto node = engine()->ecs()->as_view<MaterialView>(n);
            return node.has_value() && node->uid() == id;
        });
        materials_.erase(to_erase, materials_.end());
        engine()->ecs()->add_components<ecs::DestroyComponent>(*to_erase, {});
    }

    void Manager::clear_materials() {
        for (auto e : materials_) {
            if (auto node = engine()->ecs()->as_view<MaterialView>(e); node.has_value()) {
                node->destroy();
            }
        }
        materials_.clear();
    }

    void Manager::on_res_registry_changed(const evt::Arg &arg)
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

            // Сформировать список материалов растеризации
            for (const auto& mat_desc : proj->materials()){
                add_material(mat_desc);
            }

            // Сформировать список эффектов экрана
            for (const auto& sp_desc : proj->screen_passes()){
                // TODO: Добавить экранный прохож
            }

            // Список материалов готов
            engine()->events()->send_deferred(
                evt::Type::eMaterialRegistryChanged,
                evt::ChangeReason::eInitial);
        }
    }

    void Manager::on_display_surface_changed([[maybe_unused]] const evt::Arg &arg) const
    {
        renderer()->mark_surface_dirty();
    }
}
