#pragma once
#include <future>
#include <nasral/core_types.h>
#include <nasral/resources/resource_types.h>
#include <nasral/resources/ref.h>

namespace nasral{class Engine;}
namespace nasral::logging{class Logger;}
namespace nasral::resources
{
    class ResourceManager
    {
    public:
        friend class Ref;
        typedef std::unique_ptr<ResourceManager> Ptr;
        struct Slot
        {
            bool is_used = false;                         // Задействован ли слот
            IResource::Ptr resource = nullptr;            // Указатель на ресурс

            struct Info {
                Type type = Type::eFile;                  // Тип ресурса
                FixedPath path = {};                      // Путь к файлу ресурса
            } info;

            struct Refs {
                std::atomic<size_t> count = 0;            // Общее кол-вл ссылок на ресурс
                std::atomic<bool> has_unhandled{false};   // Есть необработанные запросы
                std::vector<Ref*> unhandled = {};         // Список необработанных запросов
                std::mutex mutex;                         // Мьютекс для безопасности списка
            } refs;

            struct Loading {
                std::atomic<bool> in_progress{false};     // В процессе ли загрузка
                std::future<void> task;                   // Задача (поток) загрузки
            } loading;
        };

        explicit ResourceManager(const Engine* engine, const ResourceConfig& config);
        ~ResourceManager();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        void add_unsafe(Type type, const std::string& path);
        void remove_unsafe(const std::string& path);
        void remove_all_unsafe();
        void await_all_tasks() const;
        void update(float delta);

        [[nodiscard]] size_t ref_count(const std::string& path) const;
        [[nodiscard]] Ref make_ref(Type type, const std::string& path) const;
        [[nodiscard]] std::string full_path(const std::string& path) const;
        [[nodiscard]] const SafeHandle<const Engine>& engine() const { return engine_; }

    private:
        void request(Ref* ref, bool unsafe = false);
        void release(const Ref* ref, bool unsafe = false);

        [[nodiscard]] std::optional<size_t> res_index(const std::string_view& path) const noexcept;
        [[nodiscard]] IResource::Ptr make_resource(const Slot& slot);
        [[nodiscard]] const IResource* get_resource(size_t index) const;
        [[nodiscard]] const logging::Logger* logger() const;

    protected:
        SafeHandle<const Engine> engine_;                       // Указатель на владельца (корневой объект)
        std::string content_dir_;                               // Путь к директории ресурсов
        std::array<Slot, MAX_RESOURCE_COUNT> slots_;            // Фиксированный массив слотов ресурсов
        std::vector<size_t> free_slots_;                        // Индексы свободных (не задействованных) слотов
        std::vector<size_t> active_slots_;                      // Индексы активных (задействованных) слотов
        std::unordered_map<std::string_view, size_t> indices_;  // Карта "путь" -> "индекс", для доступа по пути
    };
}