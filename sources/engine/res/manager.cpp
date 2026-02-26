#include "pch.h"

#include <nasral/res/manager.h>
#include <nasral/res/resources/file.h>
#include <nasral/res/resources/shader.h>
#include <nasral/res/resources/project.h>
#include <nasral/res/resources/scene.h>
#include <nasral/engine.h>

#include "loaders/shader/spv.hpp"
#include "loaders/material/xml.hpp"
#include "loaders/texture/builtins.hpp"
#include "loaders/texture/stb.hpp"
#include "loaders/mesh/assimp.hpp"
#include "loaders/mesh/builtins.hpp"
#include "loaders/project/xml.hpp"
#include "loaders/project/builtins.hpp"
#include "loaders/scene/xml.hpp"
#include "loaders/scene/builtins.hpp"

namespace nasral::res
{
    Manager::Manager(Engine* e, const Config& cfg)
        : Subsystem(e, cfg)
    {
        // Информация о каталогах
        const std::string cwd = std::filesystem::current_path().string();
        log_info("Initializing resource manager...");
        log_info("Current working directory: " + cwd);
        log_info("Content directory: " + config().content_dir + "");

        // Доступность каталога контента
        if (!config().content_dir.empty() && !std::filesystem::exists(config().content_dir)){
            throw std::runtime_error("Content directory does not exist");
        }

        // Зарезервировать память
        free_slots_.reserve(kMaxResourceCount);
        active_slots_.reserve(kMaxResourceCount);

        // Все слоты свободны изначально (полный пул)
        for (size_t i = kMaxResourceCount; i > 0; --i){
            free_slots_.emplace_back(i - 1);
        }

        // Добавить встроенные ресурсы
        add_unsafe(Type::eTexture, kBuiltinTexWhitePixel.data());
        add_unsafe(Type::eTexture, kBuiltinTexBlackPixel.data());
        add_unsafe(Type::eTexture, kBuiltinTexNormPixel.data());
        add_unsafe(Type::eTexture, kBuiltinCheckerboard.data());
        add_unsafe(Type::eMesh, kBuiltinMeshQuad.data());
        add_unsafe(Type::eMesh, kBuiltinMeshCube.data());
        add_unsafe(Type::eMesh, kBuiltinMeshSphere.data());
        add_unsafe(Type::eProject, kBuiltinProjectFile.data());
        add_unsafe(Type::eScene, kBuiltinSceneDefault.data());

        // Добавить изначальные ресурсы (из конфига)
        for (const auto& [type, path, params] : config().initial_resources){
            add_unsafe(type, path, params);
        }

        // Запросить встроенные ресурсы (они должны быть всегда доступны)
        request_builtin();

        // Запросить файл конфигурации проекта (должен быть доступен)
        request_project_config();
    }

    Manager::~Manager() = default;

    void Manager::add_unsafe(const Type type, const std::string& path, const std::optional<LoadParams>& params)
    {
        if (find(path).has_value()){
            throw std::runtime_error("Resource with path " + path + " already exists");
        }

        if (free_slots_.empty()){
            throw std::runtime_error("No free slots in resource manager");
        }

        const size_t slot_index = free_slots_.back();
        free_slots_.pop_back();

        auto& [used, resource, info, refs, loading] = slots_[slot_index];
        used = true;
        resource.reset();
        info.type = type;
        info.path.assign(path);
        refs.count.store(0, std::memory_order_release);
        refs.has_unhandled.store(false, std::memory_order_release);
        refs.unhandled = {};
        refs.unhandled.reserve(kMinRefsCount);
        loading.params = params;
        loading.in_progress.store(false, std::memory_order_release);
        loading.task = {};

        indices_[info.path.view()] = static_cast<ResourceId>(slot_index);
        active_slots_.emplace_back(slot_index);
    }

    void Manager::remove_unsafe(const std::string& path)
    {
        const auto id = find(path);
        if (!id.has_value()){
            return;
        }

        auto& [used, resource, info, refs, loading] = slots_[id.value()];
        if (!used){
            return;
        }

        if (loading.task.valid()){
            loading.task.wait();
        }

        used = false;
        resource.reset();
        refs.unhandled.clear();
        refs.has_unhandled.store(false, std::memory_order_release);
        refs.count.store(0, std::memory_order_release);
        loading.in_progress.store(false, std::memory_order_release);
        loading.params.reset();
        loading.task = {};

        indices_.erase(info.path.view());
        info.path.assign("");

        free_slots_.emplace_back(static_cast<size_t>(id.value()));
        active_slots_.erase(std::find(active_slots_.begin(), active_slots_.end(), id.value()));
    }

    void Manager::remove_all_unsafe()
    {
        for (const auto& id : active_slots_){
            auto& [used, resource, info, refs, loading] = slots_[id];

            if (loading.task.valid()){
                loading.task.wait();
            }

            used = false;
            resource.reset();
            refs.unhandled.clear();
            refs.has_unhandled.store(false, std::memory_order_release);
            refs.count.store(0, std::memory_order_release);
            loading.in_progress.store(false, std::memory_order_release);
            loading.params.reset();
            loading.task = {};
        }

        free_slots_.clear();
        for (size_t i = kMaxResourceCount; i > 0; --i){
            free_slots_.emplace_back(i - 1);
        }

        indices_.clear();
        active_slots_.clear();
    }

    void Manager::request(const ResourceId id, std::function<void(IResource*)> callback, const bool safe)
    {
        const auto index = static_cast<size_t>(id);
        if (index >= slots_.size()){
            engine()->logger()->error("Invalid resource ID");
            return;
        }

        auto& slot = slots_[index];
        slot.refs.count.fetch_add(1, std::memory_order_acquire);

        if (safe && callback){
            std::lock_guard lock(slot.refs.mutex);
            slot.refs.unhandled.emplace_back(std::move(callback));
            slot.refs.has_unhandled.store(true, std::memory_order_release);
        }
        else if (callback){
            slot.refs.unhandled.emplace_back(std::move(callback));
            slot.refs.has_unhandled.store(true, std::memory_order_release);
        }
    }

    void Manager::release(const ResourceId id)
    {
        const auto index = static_cast<size_t>(id);
        if (index >= slots_.size()){
            engine()->logger()->error("Invalid resource ID");
            return;
        }

        auto& slot = slots_[index];
        if (!slot.used){
            engine()->logger()->error("Resource slot is not used");
        }

        if (core::kDebugBuild){
            assert(slot.refs.count.load(std::memory_order_acquire) > 0 && "Trying to release already released resource");
        }

        slot.refs.count.fetch_sub(1, std::memory_order_release);
    }

    void Manager::request_builtin()
    {
        static std::vector<std::string> paths = {
            kBuiltinTexWhitePixel.data(),
            kBuiltinTexBlackPixel.data(),
            kBuiltinTexNormPixel.data(),
            kBuiltinCheckerboard.data(),
            kBuiltinMeshQuad.data(),
            kBuiltinMeshCube.data(),
            kBuiltinMeshSphere.data()
        };

        for (const auto& path : paths){
            auto id = find(path);
            assert(id.has_value());

            if (id.has_value()){
                request(id.value());
            }
        }
    }

    void Manager::release_builtin()
    {
        static std::vector<std::string> paths = {
            kBuiltinTexWhitePixel.data(),
            kBuiltinTexBlackPixel.data(),
            kBuiltinTexNormPixel.data(),
            kBuiltinCheckerboard.data(),
            kBuiltinMeshQuad.data(),
            kBuiltinMeshCube.data(),
            kBuiltinMeshSphere.data()
        };

        for (const auto& path : paths){
            auto id = find(path);
            assert(id.has_value());

            if (id.has_value()){
                release(id.value());
            }
        }
    }

    void Manager::request_project_config()
    {
        // Поиск файла проекта среди initial ресурсов (обрабатываем первый попавшийся)
        std::optional<ResourceId> id = std::nullopt;
        for (auto& [type, path, params] : config().initial_resources){
            if (type == Type::eProject){
                id = find(path);
                if (!id.has_value()){
                    log_warn("Can't find project file resource in the list (\"" + path + "\")");
                }
                break;
            }
        }

        // Если файл проекта не указан в изначальных ресурсах - полагаться нв встроенный
        if (!id.has_value()){
            id = find(kBuiltinProjectFile.data());
            if (id.has_value() && id.value() != kInvalidResourceId){
                request(id.value(), [this](IResource* res){
                    if (res->status_ == Status::eError){return;}
                    assert(res->status_ == Status::eLoaded);
                    engine()->events()->send(evt::Type::eProjectResLoaded, res);
                });
            }
        }
    }

    void Manager::release_project_config()
    {
        // Поиск файла проекта среди initial ресурсов (обрабатываем первый попавшийся)
        std::optional<ResourceId> id = std::nullopt;
        for (auto& [type, path, params] : config().initial_resources){
            if (type == Type::eProject){
                id = find(path);
                if (!id.has_value()){
                    log_warn("Can't find project file resource in the list (\"" + path + "\")");
                }
                break;
            }
        }

        // Если файл проекта не указан в изначальных ресурсах - полагаться нв встроенный
        if (!id.has_value()){
            id = find(kBuiltinProjectFile.data());
            if (id.has_value() && id.value() != kInvalidResourceId){
                auto* res_ptr = get(id.value());
                engine()->events()->send(evt::Type::eProjectResReleasing, res_ptr);
                release(id.value());
            }
        }
    }

    void Manager::await_all_tasks() const{
        for (const size_t index : active_slots_){
            if (auto& slot = slots_[index]; slot.loading.task.valid()){
                slot.loading.task.wait();
            }
        }
    }

    void Manager::finalize(){
        await_all_tasks();
        release_project_config();
        release_builtin();
        while (has_pending_unloads()){
            update();
        }
    }

    std::optional<ResourceId> Manager::find(const std::string_view& path) const{
        if (indices_.count(path) == 0) return std::nullopt;
        return indices_.at(path);
    }

    std::string Manager::path(const ResourceId id, const bool full) const{
        const auto index = static_cast<size_t>(id);
        if (index >= slots_.size()){
            return "";
        }

        std::string path = slots_[index].info.path.data();
        if (!full || path.find("builtin:") != std::string::npos){
            return path;
        }

        if (path.find(":v") != std::string::npos){
            path = path.substr(0, path.find(":v"));
        }

        const std::filesystem::path fp = std::filesystem::path(config().content_dir) / path;
        return fp.string();
    }

    size_t Manager::ref_count(const ResourceId id) const{
        const auto index = static_cast<size_t>(id);
        if (index >= slots_.size()){
            return 0;
        }

        return slots_[index].refs.count.load(std::memory_order_acquire);
    }

    void Manager::update()
    {
        for (const size_t index : active_slots_){
            auto& slot = slots_[index];

            // 1. Загрузка завершена (успешно, либо нет) а также есть необработанные callbacks
            if (slot.resource &&
                slot.resource->status_ != Status::eUnloaded &&
                slot.refs.has_unhandled.load(std::memory_order_acquire))
            {
                // Вызвать все обработчики загрузки
                std::lock_guard lock(slot.refs.mutex);
                if (!slot.refs.unhandled.empty()){
                    for (auto& callback : slot.refs.unhandled){
                        if (callback){
                            callback(slot.resource.get());
                        }
                    }
                }
                // Очистить список обработчиков
                slot.refs.unhandled.clear();
            }

            // 2. Ресурс требуется, но не загружен (и не в процессе загрузки) - инициировать загрузку
            if (slot.refs.count.load(std::memory_order_acquire) > 0 &&
                !slot.resource &&
                !slot.loading.in_progress.load(std::memory_order_acquire))
            {
                // Если остался объект задачи с прошлого раза
                if (slot.loading.task.valid() &&
                    slot.loading.task.wait_for(std::chrono::seconds(0)) == std::future_status::ready){
                    slot.loading.task = std::future<void>();
                }

                // Если задача загрузки отсутствует - создать объект ресурса и инициировать загрузку в отдельном потоке
                if (!slot.loading.task.valid()){
                    slot.loading.in_progress.store(true, std::memory_order_release);
                    slot.resource = make_resource(slot);
                    slot.loading.task = std::async(std::launch::async, [&slot](){
                        slot.resource->load();
                        slot.loading.in_progress.store(false, std::memory_order_release);
                    });
                }
            }

            // 3. Ресурс больше не требуется (нет ссылок) - выгрузить
            if (slot.refs.count.load(std::memory_order_acquire) == 0 && slot.resource){
                slot.resource.reset();
            }
        }
    }

    IResource* Manager::get(const ResourceId id) const{
        const auto index = static_cast<size_t>(id);
        if (index >= slots_.size()){
            return nullptr;
        }
        return slots_[index].resource.get();
    }

    bool Manager::has_pending_unloads() const{
        return std::any_of(active_slots_.begin(), active_slots_.end(),
        [this](const size_t index){
            const auto& slot = slots_[index];
            return slot.refs.count.load(std::memory_order_acquire) == 0
                && slot.resource != nullptr;
        });
    }

    /******************************************************************************************************************/

    /**
     * @brief Шаблонный фабричный метод создания загрузчика для ресурса
     * @tparam T Тип ресурса
     * @param slot Ссылка на слот в списке слотов ресурсов
     * @param engine Константный указатель на корневой объект (DI)
     * @return Указатель (unique) на загрузчика
     */
    template<typename T>
    typename Loader<typename T::Data>::Ptr make_res_loader(const Manager::Slot& slot, Engine* const engine)
    {
        using Ret = typename Loader<typename T::Data>::Ptr;

        if constexpr (std::is_same_v<T, Texture>){
            assert(slot.info.type == Type::eTexture);
            return slot.info.path.is_builtin()
                ? Ret{std::make_unique<TextureBuiltinLoader>(engine)}
                : Ret{std::make_unique<TextureStbLoader>(engine, slot.loading.params)};
        }
        else if constexpr (std::is_same_v<T, Mesh>){
            assert(slot.info.type == Type::eMesh);
            return slot.info.path.is_builtin()
                ? Ret{std::make_unique<MeshBuiltinLoader>(engine)}
                : Ret{std::make_unique<MeshAssimpLoader>(engine, slot.loading.params)};
        }
        else if constexpr (std::is_same_v<T, Shader>){
            assert(slot.info.type == Type::eShader);
            return Ret{std::make_unique<ShaderSpvLoader>(engine)};
        }
        else if constexpr (std::is_same_v<T, Material>){
            assert(slot.info.type == Type::eMaterial);
            return Ret{std::make_unique<MaterialXmlLoader>(engine)};
        }
        else if constexpr (std::is_same_v<T, Project>){
            assert(slot.info.type == Type::eProject);
            return slot.info.path.is_builtin()
                ? Ret{std::make_unique<ProjectBuiltinLoader>(engine)}
                : Ret{std::make_unique<ProjectXmlLoader>(engine)};
        }
        else if constexpr (std::is_same_v<T, Scene>){
            assert(slot.info.type == Type::eScene);
            return slot.info.path.is_builtin()
                ? Ret{std::make_unique<SceneBuiltinLoader>(engine)}
                : Ret{std::make_unique<SceneXmlLoader>(engine)};
        }

        assert(false && "Unknown resource type");
        return Ret{nullptr};
    }

    /**
     * @brief Фабричный метод создания нужного ресурса
     * @param slot Ссылка на слот в списке слотов ресурсов
     * @return Указатель (unique) на ресурс
     */
    IResource::Ptr Manager::make_resource(const Slot& slot)
    {
        std::unique_ptr<IResource> res = nullptr;
        try
        {
            switch (slot.info.type)
            {
            case Type::eFile:
                {
                    auto id = find(slot.info.path.view());
                    assert(id.has_value());
                    res = std::make_unique<File>(this, id.value());
                    break;
                }
            case Type::eShader:
                {
                    auto id = find(slot.info.path.view());
                    assert(id.has_value());
                    res = std::make_unique<Shader>(this, id.value(), make_res_loader<Shader>(slot, engine()));
                    break;
                }
            case Type::eTexture:
                {
                    auto id = find(slot.info.path.view());
                    assert(id.has_value());
                    res = std::make_unique<Texture>(this, id.value(), make_res_loader<Texture>(slot, engine()));
                    break;
                }
            case Type::eMesh:
                {
                    auto id = find(slot.info.path.view());
                    assert(id.has_value());
                    res = std::make_unique<Mesh>(this, id.value(), make_res_loader<Mesh>(slot, engine()));
                    break;
                }
            case Type::eMaterial:
                {
                    auto id = find(slot.info.path.view());
                    assert(id.has_value());
                    res = std::make_unique<Material>(this, id.value(), make_res_loader<Material>(slot, engine()));
                    break;
                }
            case Type::eProject:
                {
                    auto id = find(slot.info.path.view());
                    assert(id.has_value());
                    res = std::make_unique<Project>(this, id.value(), make_res_loader<Project>(slot, engine()));
                    break;
                }
            case Type::eScene:
                {
                    auto id = find(slot.info.path.view());
                    assert(id.has_value());
                    res = std::make_unique<Scene>(this, id.value(), make_res_loader<Scene>(slot, engine()));
                    break;
                }
            default:
                auto id = find(slot.info.path.view());
                assert(id.has_value());

                res = std::make_unique<File>(this, id.value());
                res->status_ = Status::eError;
                res->error_ = Error::eUnknownResource;
                log_error("Failed to create resource ["+std::to_string(id.value())+"]: unknown resource type");
                break;
            }
        }
        catch (const std::exception& e){
            const auto type = std::string(magic_enum::enum_name(slot.info.type));
            const auto path = slot.info.path.data();
            log_error("Failed to create resource ["+type+"]["+path+"]: " + std::string(e.what()));
        }

        return res;
    }
}
