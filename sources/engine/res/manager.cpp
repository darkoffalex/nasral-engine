#include "pch.h"
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Manager::Manager(Engine* e, const Config& config)
        : Subsystem(e, config)
        , free_slots_(kMaxResourceCount)
    {
        // Информация о каталогах
        const std::string cwd = std::filesystem::current_path().string();
        log_info("Initializing resource manager...");
        log_info("Current working directory: " + cwd);
        log_info("Content directory: " + config.content_dir + "");

        // Доступность каталога контента
        if (!config.content_dir.empty() && !std::filesystem::exists(config.content_dir)){
            throw std::runtime_error("Content directory does not exist");
        }

        // Зарезервировать память
        active_slots_.reserve(kMaxResourceCount);

        // Добавить встроенные по умолчанию ресурсы
        add_builtins();
        
        // Добавить ресурсы инициализации, если есть (из конфига)
        for (const auto& desc : config.initial_resources){
            add(desc);
        }

        // Запросить обязательные ресурсы (подразумевается, что они добавлены)
        // Такие ресурсы должны быть доступны в любой момент времени
        request_mandatory();
    }

    Manager::~Manager() = default;

    void Manager::add(const ResourceDesc& description)
    {
        auto& [type, path, options] = description;

        if (find(path) != std::nullopt){
            throw std::runtime_error("Resource with path " + path + " already exists");
        }

        if (free_slots_.empty()){
            throw std::runtime_error("No free resource slots");
        }

        const auto slot_idx = free_slots_.acquire_unsafe();
        auto& slot = slots_[slot_idx];
        slot.init(description);

        indices_[slot.info.path.view()] = static_cast<ResourceId>(slot_idx);
        active_slots_.push_back(slot_idx);
    }

    void Manager::remove(const ResourceId& id)
    {
        const auto slot_idx = static_cast<size_t>(id);
        if (slot_idx >= slots_.size()){
            throw std::runtime_error("Resource with id " + std::to_string(id) + " does not exist");
        }

        auto& slot = slots_[slot_idx];
        assert(slot.used && "Resource slot is not used");

        indices_.erase(slot.info.path.view());
        slot.clean();

        free_slots_.release_unsafe(slot_idx);
        active_slots_.erase(std::find(active_slots_.begin(), active_slots_.end(), slot_idx));
    }

    void Manager::remove_all()
    {
        for (const auto& slot_idx : active_slots_){
            auto& slot = slots_[slot_idx];
            slot.clean();
        }

        free_slots_.reset();
        indices_.clear();
        active_slots_.clear();
    }

    void Manager::request(const ResourceId id, std::function<void(Resource*)> callback)
    {
        const auto slot_idx = static_cast<size_t>(id);
        if (slot_idx >= slots_.size()){
            log_error("Resource slot with index " + std::to_string(id) + " does not exist");
            return;
        }

        auto& slot = slots_[slot_idx];
        slot.request(std::move(callback));
    }

    void Manager::release(const ResourceId id)
    {
        const auto slot_idx = static_cast<size_t>(id);
        if (slot_idx >= slots_.size()){
            log_error("Resource slot with index " + std::to_string(id) + " does not exist");
            return;
        }

        auto& slot = slots_[slot_idx];
        slot.release();
    }

    std::optional<ResourceId> Manager::find(const std::string_view& path) const
    {
        const auto it = indices_.find(path);
        if (it == indices_.end()) return std::nullopt;
        return it->second;
    }

    std::optional<ResourceId> Manager::find_project() const
    {
        // Попытаться найти ресурс файла проекта в initial ресурсах
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

        // Если файл проекта не указан в initial ресурсах - полагаться нв builtin
        if (!id.has_value()){
            id = find(kBuiltinProjectFile.data());
            assert(id.has_value() && "Wrong builtin project file path");
            assert(id.value() != kInvalidResourceId && "Wrong builtin project file ID");
        }

        return id;
    }

    const Manager::Slot* Manager::slot(const ResourceId& id) const{
        const auto slot_idx = static_cast<size_t>(id);
        return slot_idx < slots_.size() ? &slots_[slot_idx] : nullptr;
    }

    Resource* Manager::get(const ResourceId& id) const
    {
        const auto slot_idx = static_cast<size_t>(id);
        return slot_idx < slots_.size() ? slots_[slot_idx].resource.get() : nullptr;
    }

    std::string Manager::path(const ResourceId& id, const bool full) const
    {
        const auto slot_idx = static_cast<size_t>(id);
        if (slot_idx >= slots_.size()){
            log_warn("Resource slot with index " + std::to_string(id) + " does not exist");
            return "";
        }

        // Базовый путь ресурса
        std::string path = slots_[slot_idx].info.path.data();

        // Если не требуется полный путь, либо если путь на встроенный ресурс - вернуть как есть
        if (!full || path.find("builtin:") != std::string::npos){
            return path;
        }

        // Есть путь содержит тег версии - исключить тег (только путь к файлу)
        if (path.find(":v") != std::string::npos){
            path = path.substr(0, path.find(":v"));
        }

        // Полный путь к файлу
        const std::filesystem::path fp = std::filesystem::path(config().content_dir) / path;
        return fp.string();
    }

    size_t Manager::ref_count(const ResourceId& id) const
    {
        const auto slot_idx = static_cast<size_t>(id);
        if (slot_idx >= slots_.size()){
            log_warn("Resource slot with index " + std::to_string(id) + " does not exist");
            return 0;
        }
        return slots_[slot_idx].refs.count.load(std::memory_order_acquire);
    }

    /* S U B S Y S T E M */

    void Manager::update([[maybe_unused]] float delta)
    {
        for (const size_t index : active_slots_){
            auto& slot = slots_[index];
            // 1. Загрузка была завершена (успешно, либо нет) и есть необработанные callbacks
            process_slot_callbacks(slot);
            // 2. Ресурс требуется, но не загружен (и не в процессе загрузки) - инициировать загрузку
            process_slot_requests(slot);
            // 3. Ресурс больше не требуется (нет ссылок) - выгрузить (уничтожение объекта)
            process_slot_releases(slot);
        }
    }

    void Manager::finalize()
    {
        // Ожидаем всех загрузок (если в процессе)
        wait_for_loading();

        // Освободить обязательные ресурсы
        release_mandatory();

        // Покуда есть свободные ресурсы (без ссылок) - выгружать их
        while (has_hanging_resources()){
            for (const size_t index : active_slots_){
                auto& slot = slots_[index];
                process_slot_releases(slot);
            }
        }
    }

    void Manager::process_slot_callbacks(Slot& slot)
    {
        // Если объект ресурса есть
        // Если загрузка ресурса завершена (успешно или нет)
        // Если есть не вызванные обработчики
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
            slot.refs.has_unhandled.store(false, std::memory_order_release);
            slot.refs.unhandled.clear();
        }
    }

    void Manager::process_slot_requests(Slot& slot)
    {
        // Если количество ссылок положительное
        // Если ресурс не создан
        // Если загрузка ресурса не началась
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
    }

    void Manager::process_slot_releases(Slot& slot)
    {
        // Если ссылок нет
        // Если загрузка не в процессе
        // Если ресурс еще жив (не выгружен)
        if (slot.refs.count.load(std::memory_order_acquire) == 0 &&
            !slot.loading.in_progress.load(std::memory_order_acquire) &&
            slot.resource)
        {
            slot.resource.reset();
        }
    }

    Resource::Ptr Manager::make_resource(const Slot& slot)
    {
        std::unique_ptr<Resource> res = nullptr;

        try
        {
            switch (slot.info.type)
            {
            case Type::eFile:
                {
                    break;
                }
            default:
                {
                    break;
                }
            }
        }
        catch ([[maybe_unused]] const std::exception& e)
        {
            const auto type = std::string(magic_enum::enum_name(slot.info.type));
            const auto path = slot.info.path.data();
            log_error("Failed to create resource ["+type+"]["+path+"]");
        }

        return res;
    }

    bool Manager::has_hanging_resources() const
    {
        return std::any_of(active_slots_.begin(), active_slots_.end(),
        [this](const size_t index){
            const auto& slot = slots_[index];
            return slot.refs.count.load(std::memory_order_acquire) == 0
                && slot.resource != nullptr;
        });
    }

    void Manager::add_builtins()
    {
        // Добавить встроенные ресурсы
        add({Type::eTexture, kBuiltinTexWhitePixel.data(), std::nullopt});
        add({Type::eTexture, kBuiltinTexBlackPixel.data(), std::nullopt});
        add({Type::eTexture, kBuiltinTexNormPixel.data(), std::nullopt});
        add({Type::eTexture, kBuiltinCheckerboard.data(), std::nullopt});
        add({Type::eMesh, kBuiltinMeshQuad.data(), std::nullopt});
        add({Type::eMesh, kBuiltinMeshCube.data(), std::nullopt});
        add({Type::eMesh, kBuiltinMeshSphere.data(), std::nullopt});
        add({Type::eProject, kBuiltinProjectFile.data(), std::nullopt});
        add({Type::eScene, kBuiltinSceneDefault.data(), std::nullopt});
    }

    void Manager::request_mandatory()
    {
        // 1. Запросить встроенные ресурсы
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

        // 2. Запросить ресурс проекта (либо из initial, либо из builtin)
        const std::optional<ResourceId> project_rid = find_project();
        assert(project_rid.has_value());
        request(project_rid.value(), [this](Resource* res){
            if (res->status_ == Status::eError){
                throw std::runtime_error("Failed to load project file");
            }
            assert(res->status_ == Status::eLoaded);
            engine()->events()->send_deferred(evt::Type::eProjectFileLoaded, res);
        });
    }

    void Manager::release_mandatory()
    {
        // 1. Освободить встроенные ресурсы
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

        // 2. Освободить ресурс проекта (либо из initial, либо из builtin)
        const std::optional<ResourceId> project_rid = find_project();
        assert(project_rid.has_value());
        release(project_rid.value());
    }

    void Manager::wait_for_loading() const
    {
        for (const auto& slot : slots_){
            if (slot.loading.in_progress.load(std::memory_order_acquire) &&
                slot.loading.task.valid()){
                slot.loading.task.wait();
            }
        }
    }

    /* S L O T S */

    void Manager::Slot::clean()
    {
        if (loading.task.valid()){
            loading.task.wait();
        }

        used = false;
        resource.reset();
        info.type = Type::eUndefined;
        info.path.assign("");
        refs.count.store(0, std::memory_order_release);
        refs.has_unhandled.store(false, std::memory_order_release);
        refs.unhandled.clear();
        loading.in_progress.store(false, std::memory_order_release);
        loading.params = std::nullopt;
        loading.task = {};
    }

    void Manager::Slot::init(const ResourceDesc& desc)
    {
        auto& [type, path, params] = desc;

        used = true;
        resource.reset();
        info.type = type;
        info.path.assign(path);
        refs.count.store(0, std::memory_order_release);
        refs.has_unhandled.store(false, std::memory_order_release);
        refs.unhandled.clear();
        refs.unhandled.reserve(kMinRefsCount);
        loading.in_progress.store(false, std::memory_order_release);
        loading.params = params;
        loading.task = {};
    }

    void Manager::Slot::request(std::function<void(Resource*)> callback, const bool safe)
    {
        assert(used && "Resource slot is not used");
        refs.count.fetch_add(1, std::memory_order_acquire);

        if (callback && safe){
            std::lock_guard lock(refs.mutex);
            refs.unhandled.emplace_back(std::move(callback));
            refs.has_unhandled.store(true, std::memory_order_release);
        }
        else if (callback){
            refs.unhandled.emplace_back(std::move(callback));
            refs.has_unhandled.store(true, std::memory_order_release);
        }
    }

    void Manager::Slot::release()
    {
        assert(used && "Resource slot is not used");
        refs.count.fetch_sub(1, std::memory_order_release);
    }
}
