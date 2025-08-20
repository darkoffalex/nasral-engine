#include "pch.h"
#include <nasral/resources/resource_manager.h>
#include <nasral/resources/file.h>
#include <nasral/resources/shader.h>
#include <nasral/resources/material.h>
#include <nasral/resources/mesh.h>
#include <nasral/resources/texture.h>
#include <nasral/engine.h>

#include "loaders/shader_loader.hpp"
#include "loaders/material_loader.hpp"
#include "loaders/mesh_loader.hpp"
#include "loaders/mesh_builtin_loader.hpp"
#include "loaders/texture_loader.hpp"
#include "loaders/texture_builtin_loader.hpp"

namespace fs = std::filesystem;
namespace nasral::resources
{
    ResourceManager::ResourceManager(const Engine* engine, const ResourceConfig& config)
        : engine_(engine)
        , content_dir_(config.content_dir)
    {
        // Начала инициализации, вывод базовой информации
        const std::string cwd = std::filesystem::current_path().string();
        logger()->info("Initializing resource manager...");
        logger()->info("Current working directory: " + cwd);
        logger()->info("Content directory: " + content_dir_);

        // Проверка доступности директории контента
        if (!content_dir_.empty() && !fs::exists(content_dir_)){
            throw ResourceError("Content directory does not exist (" + content_dir_ + ")");
        }

        // Подготовить память массивов
        free_slots_.reserve(MAX_RESOURCE_COUNT);
        active_slots_.reserve(MAX_RESOURCE_COUNT);

        // Список доступных слотов (от большего к меньшему)
        for (size_t i = MAX_RESOURCE_COUNT; i > 0; --i){
            free_slots_.push_back(i - 1);
        }

        // Добавить изначальные ресурсы в список
        for (const auto& [type, path, params] : config.initial_resources){
            add_unsafe(type, path, params);
        }

        // Добавить встроенные ресурсы (ресурсы по умолчанию) в список
        for (size_t i = 0; i < to<size_t>(BuiltinResources::TOTAL); ++i){
            const auto path = builtin_res_path(to<BuiltinResources>(i));
            const auto type = builtin_res_type(path);
            if (type != Type::TOTAL){
                add_unsafe(type, path);
            }
        }

        // Запросить встроенные ресурсы
        request_builtin();
    }

    ResourceManager::~ResourceManager(){
        remove_all_unsafe();
    }

    void ResourceManager::add_unsafe(const Type type, const std::string& path, const std::optional<LoadParams>& params){
        if (indices_.count(std::string_view(path)) > 0){
            logger()->warning("Trying to add resource with duplicate path (" + path + ")");
            return;
        }

        // Если это не встроенный ресурс - проверить наличие файла
        if (path.find("builtin:") == std::string::npos){
            const auto fp = full_path(path);
            if (!fs::exists(fp)){
                throw ResourceError("Resource file not found (" + path + ")");
            }
        }

        // Получить индекс свободного слота в списке ресурсов
        size_t index;
        if (!free_slots_.empty()) {
            index = free_slots_.back();
            free_slots_.pop_back();
        } else {
            throw ResourceError("No free slots in resource manager");
        }

        // Подготовить слот для использования
        auto& [is_used, resource, info, refs, loading] = slots_[index];
        is_used = true;
        resource.reset();
        info.type = type;
        info.path.assign(path);
        refs.count.store(0, std::memory_order_release);
        refs.has_unhandled.store(false, std::memory_order_release);
        refs.unhandled = {};
        refs.unhandled.reserve(DEFAULT_REFS_COUNT);
        loading.params = params;
        loading.in_progress.store(false, std::memory_order_release);
        loading.task = {};

        // Связать путь-строку с индексом
        indices_[info.path.view()] = index;
        // Добавить в список активных слотов
        active_slots_.push_back(index);
    }

    void ResourceManager::remove_unsafe(const std::string& path){
        if (const auto index = res_index(path); index.has_value()){
            auto& [is_used, resource, info, refs, loading] = slots_[index.value()];
            if (!is_used) return;

            // Дождаться завершения задачи загрузки
            if (loading.task.valid()) {
                loading.task.wait();
            }

            // Освободить слот
            is_used = false;
            resource.reset();
            refs.unhandled.clear();
            refs.has_unhandled.store(false, std::memory_order_release);
            refs.count.store(0, std::memory_order_release);
            loading.in_progress.store(false, std::memory_order_release);
            loading.task = {};

            // Удалить из ассоциативного контейнера
            indices_.erase(info.path.view());
            info.path.assign("");

            // Добавить назад в список свободных слотов
            free_slots_.push_back(index.value());
            // Убрать из списка активных слотов
            for (size_t i = 0; i < active_slots_.size(); ++i) {
                if (active_slots_[i] == index.value()) {
                    std::swap(active_slots_[i], active_slots_.back());
                    active_slots_.pop_back();
                    break;
                }
            }
        }
    }

    void ResourceManager::remove_all_unsafe(){
        for (const size_t index : active_slots_){
            auto& [is_used, resource, info, refs, loading] = slots_[index];

            if(loading.task.valid()){
                loading.task.wait();
                loading.task = {};
            }

            is_used = false;
            resource.reset();
            refs.unhandled.clear();
            refs.has_unhandled.store(false, std::memory_order_release);
            refs.count.store(0, std::memory_order_release);
            loading.in_progress.store(false, std::memory_order_release);
            info.path.assign("");
        }

        free_slots_.clear();
        for (size_t i = MAX_RESOURCE_COUNT; i > 0; --i) {
            free_slots_.push_back(i - 1);
        }

        indices_.clear();
        active_slots_.clear();
    }

    std::optional<size_t> ResourceManager::res_index(const std::string_view& path) const noexcept{
        if (indices_.count(path) > 0) return indices_.at(path);
        return std::nullopt;
    }

    void ResourceManager::request(Ref* ref, const bool unsafe){
        // Получить корректный индекс
        const auto index = ref->index().has_value() ? ref->index() : res_index(ref->path().view());
        if (!index.has_value()) {
            const auto message = "Requested resource not found (" + std::string(ref->path().view()) + ")";
            logger()->error(message);
            throw ResourceError(message);
        }

        // Обновить индекс у ссылки
        assert(index.value() < slots_.size());
        ref->resource_index_ = index;

        // Увеличить счетчик, отметить, что есть необработанные ссылки
        auto& slot = slots_[index.value()];
        slot.refs.count.fetch_add(1, std::memory_order_release);

        // Добавить в список необработанных ссылок
        if (!unsafe){
            std::lock_guard lock_guard(slot.refs.mutex);
            slot.refs.unhandled.push_back(ref);
            slot.refs.has_unhandled.store(true, std::memory_order_release);
        }else{
            slot.refs.unhandled.push_back(ref);
            slot.refs.has_unhandled.store(true, std::memory_order_release);
        }
    }

    void ResourceManager::release(const Ref* ref, const bool unsafe){
        // Получить корректный индекс
        const auto index = ref->index().has_value() ? ref->index() : res_index(ref->path().view());
        if (!index.has_value()) {
            const auto message = "Releasing resource not found (" + std::string(ref->path().view()) + ")";
            logger()->warning(message);
            return;
        }

        // Активен ли слот
        assert(index.value() < slots_.size());
        auto& slot = slots_[index.value()];
        if (!slot.is_used) {
            logger()->warning("Trying to release resource from unused slot (" + std::string(ref->path().view()) + ")");
            return;
        }

        // Уменьшить ко-во ссылок на ресурс
        slot.refs.count.fetch_sub(1, std::memory_order_release);

        // Функция удаления существующей необработанной ссылки
        auto remove = [&]{
            auto& unhandled = slot.refs.unhandled;
            for (size_t i = 0; i < unhandled.size(); ++i) {
                if (unhandled[i] == ref) {
                    std::swap(unhandled[i], unhandled.back());
                    unhandled.pop_back();
                    break;
                }
            }
        };

        // Удалить ссылку из списка необработанных ссылок
        if (!unsafe){
            std::lock_guard lock_guard(slot.refs.mutex);
            remove();
        }else{
            remove();
        }
    }

    void ResourceManager::request_builtin(){
        for (size_t i = 0; i < to<size_t>(BuiltinResources::TOTAL); ++i){
            const auto path = builtin_res_path(to<BuiltinResources>(i));
            const auto type = builtin_res_type(path);
            if (type != Type::TOTAL){
                builtin_resources_[i] = make_ref(type, path);
                builtin_resources_[i].request();
            }
        }
    }

    void ResourceManager::release_builtin(){
        for (size_t i = 0; i < to<size_t>(BuiltinResources::TOTAL); ++i){
            builtin_resources_[i].release();
        }
    }

    IResource::Ptr ResourceManager::make_resource(const Slot& slot){
        try {
            std::unique_ptr<IResource> res;
            switch (slot.info.type)
            {
            case Type::eFile:
                {
                    res = std::make_unique<File>(this, slot.info.path.view());
                    break;
                }
            case Type::eShader:
                {
                    res = std::make_unique<Shader>(this,
                        slot.info.path.view(),
                        std::make_unique<ShaderLoader>());
                    break;
                }
            case Type::eMaterial:
                {
                    res = std::make_unique<Material>(this,
                        slot.info.path.view(),
                        std::make_unique<MaterialLoader>());
                    break;
                }
            case Type::eMesh:
                {
                    const auto path = slot.info.path.view();
                    if (path.find("builtin:mesh") != std::string_view::npos){
                        res = std::make_unique<Mesh>(this,
                            slot.info.path.view(),
                            std::make_unique<MeshBuiltinLoader>());
                    }else{
                        //auto& params = slot.loading.params;
                        //auto* mp = params.has_value() ? std::get_if<MeshLoadParams>(&params.value()) : nullptr;

                        res = std::make_unique<Mesh>(this,
                            slot.info.path.view(),
                            std::make_unique<MeshLoader>());
                    }
                    break;
                }
            case Type::eTexture:
                {
                    const auto path = slot.info.path.view();
                    if (path.find("builtin:tex") != std::string_view::npos){
                        res = std::make_unique<Texture>(this,
                            slot.info.path.view(),
                            std::make_unique<TextureBuiltinLoader>());
                    }
                    else{
                        auto& params = slot.loading.params;
                        auto* tp = params.has_value() ? std::get_if<TextureLoadParams>(&params.value()) : nullptr;

                        res = std::make_unique<Texture>(this,
                             slot.info.path.view(),
                             std::make_unique<TextureLoader>(),
                             tp ? *tp : TextureLoadParams());
                    }
                    break;
                }
            default:
                {
                    res = std::make_unique<File>(this, slot.info.path.view());
                    res->status_ = Status::eError;
                    res->err_code_ = ErrorCode::eUnknownResource;
                    break;
                }
            }
            return res;
        }catch (const std::exception& e) {
            logger()->error("Can't create resource (" + std::string(slot.info.path.view()) + ") - " + e.what());
            throw ResourceError("Can't create resource (" + std::string(slot.info.path.view()) + ")");
        }
    }

    const IResource* ResourceManager::get_resource(const size_t index) const{
        if (index >= slots_.size()
            || !slots_[index].is_used
            || !slots_[index].resource
            || slots_[index].resource->status() != Status::eLoaded)
        {
            return nullptr;
        }

        return slots_[index].resource.get();
    }

    bool ResourceManager::has_pending_unloads() const {
        return std::any_of(active_slots_.begin(), active_slots_.end(),
            [this](const size_t index){
                const auto& slot = slots_[index];
                return slot.refs.count.load(std::memory_order_acquire) == 0
                    && static_cast<bool>(slot.resource);
            });
    }


    void ResourceManager::await_all_tasks() const{
        for (const size_t index : active_slots_){
            if (auto& slot = slots_[index]; slot.loading.task.valid()){
                slot.loading.task.wait();
            }
        }
    }

    std::string ResourceManager::full_path(const std::string& path) const{
        if (path.find("builtin:") != std::string::npos){
            return path;
        }
        try {
            std::string p = path;
            if (p.find(":v") != std::string::npos){
                p = p.substr(0, p.find(":v"));
            }
            const fs::path full = fs::path(content_dir_) / p;
            if (!fs::exists(full)) {
                logger()->error("File not found (" + full.string() + ")");
                throw std::filesystem::filesystem_error("File not found", full.string(), std::error_code());
            }
            return fs::canonical(full).string();
        }
        catch (const std::exception& e) {
            throw ResourceError(e.what());
        }
    }

    size_t ResourceManager::ref_count(const std::string &path) const {
        if(const auto index = res_index(std::string_view(path)); index.has_value()){
            auto& slot = slots_[index.value()];
            return slot.refs.count.load(std::memory_order_acquire);
        }
        return 0;
    }

    Ref ResourceManager::make_ref(const Type type, const std::string& path) const{
        return {const_cast<ResourceManager*>(this), type, path};
    }

    const logging::Logger *ResourceManager::logger() const{
        return engine()->logger();
    }

    void ResourceManager::update([[maybe_unused]] float delta){
        for (const size_t index : active_slots_){
            auto& slot = slots_[index];

            // Если загрузка была завершена (успешно, либо нет) а также есть необработанные запросы
            if (slot.resource &&
                slot.resource->status_ != Status::eUnloaded &&
                slot.refs.has_unhandled.load(std::memory_order_acquire))
            {
                std::lock_guard lock(slot.refs.mutex);
                if (!slot.refs.unhandled.empty()){
                    for (auto* ref : slot.refs.unhandled) {
                        ref->is_handled_ = true;
                        if (ref->on_ready_) {
                            ref->on_ready_(slot.resource.get());
                        }
                    }
                    slot.refs.unhandled.clear();
                }
                slot.refs.has_unhandled.store(false, std::memory_order_release);
            }

            // Инициирование загрузки, если ресурс требуется, но не создан
            if (slot.refs.count.load(std::memory_order_acquire) > 0 &&
                !slot.resource &&
                !slot.loading.in_progress.load(std::memory_order_acquire))
            {
                // Если вдруг предыдущая задача в процессе - ожидаем и очищаем задачу
                if (slot.loading.task.valid() &&
                    slot.loading.task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    slot.loading.task = std::future<void>();
                }
                // Нет задачи загрузки - создать объект ресурса и инициировать загрузку в отдельном потоке
                if (!slot.loading.task.valid()) {
                    slot.loading.in_progress.store(true, std::memory_order_release);
                    slot.resource = make_resource(slot);
                    slot.loading.task = std::async(std::launch::async, [&slot]() {
                        slot.resource->load();
                        slot.loading.in_progress.store(false, std::memory_order_release);
                    });
                }
            }

            // Выгрузка ресурса, если он больше не нужен
            if (slot.refs.count.load(std::memory_order_acquire) == 0 && slot.resource) {
                slot.resource.reset();
            }
        }
    }

    void ResourceManager::finalize(){
        await_all_tasks();
        release_builtin();
        while (has_pending_unloads()){
            update(0.0f);
        }
    }
}
