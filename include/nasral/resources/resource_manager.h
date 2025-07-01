#pragma once
#include <future>
#include <nasral/resources/resource_types.h>
#include <nasral/resources/ref.h>

namespace nasral::resources
{
    class ResourceManager
    {
    public:
        friend class Ref;

        struct Slot {
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

        ResourceManager();
        ~ResourceManager();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        void add_unsafe(Type type, const std::string& path);
        void remove_unsafe(const std::string& path);
        void remove_all_unsafe();

        void update(float delta);

    protected:
        [[nodiscard]] std::optional<size_t> res_index(const std::string_view& path);
        [[nodiscard]] static IResource::Ptr make_resource(const Slot& slot);

        void request(Ref* ref, bool unsafe = false);
        void release(const Ref* ref, bool unsafe = false);
        void await_all_tasks() const;

    private:
        std::array<Slot, MAX_RESOURCE_COUNT> slots_;            // Фиксированный массив слотов ресурсов
        std::vector<size_t> free_slots_;                        // Индексы свободных (не задействованных) слотов
        std::vector<size_t> active_slots_;                      // Индексы активных (задействованных) слотов
        std::unordered_map<std::string_view, size_t> indices_;  // Карта "путь" -> "индекс", для доступа по пути
    };
}