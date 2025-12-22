#pragma once

#include <memory>
#include <atomic>
#include <future>
#include <unordered_map>

#include <nasral/core/subsystem.h>
#include <nasral/res/types.h>
#include <nasral/res/resource.h>
#include <nasral/log/loggable.h>

namespace nasral::res
{
    class Manager final : public core::Subsystem<Config>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;
        struct Slot
        {
            bool used = false;
            IResource::Ptr resource = nullptr;

            struct Info{
                Type type = Type::eFile;
                Path path = {};
            } info;

            struct Refs{
                std::atomic<size_t> count = 0;
                std::atomic<bool> has_unhandled{false};
                std::vector<std::function<void(IResource*)>> unhandled = {};
                std::mutex mutex;
            } refs;

            struct Loading{
                std::atomic<bool> in_progress{false};
                std::future<void> task;
                std::optional<LoadParams> params = std::nullopt;
            } loading;
        };

        Manager(Engine* e, const Config& cfg);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void add_unsafe(Type type, const std::string& path, const std::optional<LoadParams>& params = std::nullopt);
        void remove_unsafe(const std::string& path);
        void remove_all_unsafe();

        void request(ResourceId id, std::function<void(IResource*)> callback = nullptr, bool safe = true);
        void release(ResourceId id);

        [[nodiscard]] std::optional<ResourceId> find(const std::string_view& path) const;
        [[nodiscard]] IResource* get(ResourceId id) const;
        [[nodiscard]] std::string path(ResourceId id, bool full = false) const;
        [[nodiscard]] size_t ref_count(ResourceId id) const;

        void update();
        void finalize();

    private:
        IResource::Ptr make_resource(const Slot& slot);
        bool has_pending_unloads() const;
        void request_builtin();
        void release_builtin();
        void request_project_config();
        void release_project_config();
        void await_all_tasks() const;

    protected:
        /// Фиксированный массив слотов ресурсов (кеш-когерентность)
        std::array<Slot, kMaxResourceCount> slots_;
        /// Пул незадействованных индексов
        std::vector<size_t> free_slots_;
        /// Индексы задействованных слотов
        std::vector<size_t> active_slots_;
        /// Карта "путь" -> "индекс", для доступа по пути
        std::unordered_map<std::string_view, ResourceId> indices_;
    };

}

namespace nasral::log
{
    class Logger;

    template <typename T>
    struct LoggerAccessor<T, std::enable_if_t<std::is_same_v<res::Manager, T>>> {
        static Logger* get(const T* mgr) {
            return mgr->engine()->logger();
        }
    };
}


