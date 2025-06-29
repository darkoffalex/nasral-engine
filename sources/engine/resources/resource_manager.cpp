#include "pch.h"
#include <nasral/resources/resource_manager.h>
#include <nasral/resources/file.h>

namespace nasral::resources
{
    ResourceManager::ResourceManager()
        : slots_({})
    {
        // Создать пустые слоты
        for (size_t i = MAX_RESOURCE_COUNT; i > 0; --i){
            free_slots_.push_back(i - 1);
        }
    }

    ResourceManager::~ResourceManager(){
        remove_all_unsafe();
    }

    void ResourceManager::add_unsafe(const Type type, const std::string& path){
        if (indices_.count(std::string_view(path)) > 0){
            throw std::runtime_error("Resource already exists");
        }

        // Получить индекс свободного слота в списке ресурсов
        size_t index;
        if (!free_slots_.empty()) {
            index = free_slots_.back();
            free_slots_.pop_back();
        } else {
            throw std::runtime_error("Resource limit reached");
        }

        // Подготовить слот для использования
        auto& [is_used, resource, info, refs, loading] = slots_[index];
        is_used = true;
        resource.reset();
        info.type = type;
        info.path.assign(path);
        info.deps = {};
        refs.count.store(0, std::memory_order_release);
        refs.has_unhandled.store(false, std::memory_order_release);
        refs.unhandled = {};
        loading.in_progress.store(false, std::memory_order_release);
        loading.task = {};

        // Связать путь-строку с индексом
        indices_[info.path.view()] = index;
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

            // Добавить в список свободных слотов
            free_slots_.push_back(index.value());
        }
    }

    void ResourceManager::remove_all_unsafe(){
        for (auto& [is_used, resource, info, refs, loading] : slots_) {
            if (is_used) {
                if (loading.task.valid()) {
                    loading.task.wait();
                }
                is_used = false;
                resource.reset();
                refs.unhandled.clear();
                refs.has_unhandled.store(false, std::memory_order_release);
                refs.count.store(0, std::memory_order_release);
                loading.in_progress.store(false, std::memory_order_release);
                loading.task = {};
                info.path.assign("");
            }
        }
        free_slots_.clear();
        for (size_t i = MAX_RESOURCE_COUNT; i > 0; --i) {
            free_slots_.push_back(i - 1);
        }
        indices_.clear();
    }

    std::optional<size_t> ResourceManager::res_index(const std::string_view& path){
        if (indices_.count(path) > 0) return indices_.at(path);
        return std::nullopt;
    }

    void ResourceManager::request(Ref* ref, const bool unsafe){
        // Получить корректный индекс
        const auto index = ref->index().has_value() ? ref->index() : res_index(ref->path().view());
        if (!index.has_value()) throw std::runtime_error("Resource not found");
        assert(index.value() < slots_.size());

        // Обновить индекс у ссылки
        ref->resource_index_ = index;

        // Увеличить счетчик, отметить что есть необработанные ссылки
        auto& slot = slots_[index.value()];
        slot.refs.count.fetch_add(1, std::memory_order_release);
        slot.refs.has_unhandled.store(true, std::memory_order_release);

        // Добавить в список необработанных ссылок
        if (!unsafe){
            std::lock_guard lock_guard(slot.refs.mutex);
            slot.refs.unhandled.push_back(ref);
        }else{
            slot.refs.unhandled.push_back(ref);
        }
    }

    void ResourceManager::release(const Ref* ref, const bool unsafe){
        // Получить корректный индекс
        const auto index = ref->index().has_value() ? ref->index() : res_index(ref->path().view());
        if (!index.has_value()) throw std::runtime_error("Resource not found");
        assert(index.value() < slots_.size());

        // Уменьшить счетчик ссылок
        auto& slot = slots_[index.value()];
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

    IResource::Ptr ResourceManager::make_resource(const Slot& slot){
        std::unique_ptr<IResource> res;
        switch (slot.info.type)
        {
        case Type::eFile:
            {
                res = std::make_unique<File>(slot.info.path.view());
                break;
            }
        case Type::eMaterial:
            {
                // TODO: Создать объект, передав в конструктор slot.dependencies
                break;
            }
        default:
            {
                res = std::make_unique<File>(slot.info.path.view());
                res->status_ = Status::eError;
                res->err_code_ = ErrorCode::eUnknownResource;
                break;
            }
        }

        return res;
    }

    void ResourceManager::await_all_tasks() const{
        for (auto& slot : slots_){
            if (!slot.is_used) continue;;
            if (slot.loading.task.valid()){
                slot.loading.task.wait();
            }
        }
    }

    void ResourceManager::update([[maybe_unused]] float delta){
        for (auto& slot : slots_){
            // Пропуск не используемых слотов
            if (!slot.is_used) continue;

            // Если загрузка была завершена (успешно, либо нет) а также есть необработанные запросы
            if (slot.resource &&
                slot.resource->status_ != Status::eUnloaded &&
                slot.refs.has_unhandled.load(std::memory_order_acquire))
            {
                // Необработанных запросов больше нет
                slot.refs.has_unhandled.store(false, std::memory_order_release);
                // Вызвать функции-обработчики готовности
                std::lock_guard lock(slot.refs.mutex);
                for (const auto* ref : slot.refs.unhandled) {
                    if (ref->on_ready_) {
                        ref->on_ready_(slot.resource.get());
                    }
                }
                // Очищаем список обработанных ссылок
                slot.refs.unhandled.clear();
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
                    slot.loading.task = std::async(std::launch::async, [this, &slot]() {
                        slot.resource->load();
                        slot.loading.in_progress.store(false,std::memory_order_release);
                    });
                }
            }

            // Выгрузка ресурса, если он больше не нужен
            if (slot.refs.count.load(std::memory_order_acquire) == 0 && slot.resource) {
                slot.resource.reset();
            }
        }
    }
}
