#pragma once

#include <memory>
#include <atomic>
#include <future>
#include <unordered_map>

#include <nasral/common/subsystem.h>
#include <nasral/common/index_pool.h>
#include <nasral/log/loggable.h>
#include <nasral/res/types.h>
#include <nasral/gfx/types.h>
#include <nasral/res/objects/resource.h>
#include <nasral/res/system.h>
#include <nasral/evt/objects/listener.h>

namespace nasral::res
{
    class Manager final : public Subsystem<Manager, Config>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;

        struct Slot
        {
            bool used = false;
            Resource::Ptr resource = nullptr;

            struct Info{
                Type type = Type::eFile;
                Path path = {};
            } info;

            struct Refs{
                std::atomic<size_t> count = 0;
                std::atomic<bool> has_unhandled{false};
                std::vector<std::function<void(Resource*)>> unhandled = {};
                std::mutex mutex;
            } refs;

            struct Loading{
                std::atomic<bool> in_progress{false};
                std::future<void> task;
                std::optional<LoadParams> params = std::nullopt;
            } loading;

            void clean();
            void init(const ResourceDesc& desc);
            void request(std::function<void(Resource*)> callback, bool safe = true);
            void release();
        };

        explicit Manager(Engine* e, const Config& config);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void init();
        void update(float delta);
        void finalize();

        void add(const ResourceDesc& description);
        void remove(const ResourceId& id);
        void remove_all();

        void request(ResourceId id, std::function<void(Resource*)> callback = nullptr);
        void release(ResourceId id);

        [[nodiscard]] std::optional<ResourceId> find(const std::string_view& path) const;
        [[nodiscard]] std::optional<ResourceId> find_project() const;
        [[nodiscard]] std::optional<ResourceId> find_texture_fallback(gfx::TextureType type) const;
        [[nodiscard]] const Slot* slot(const ResourceId& id) const;
        [[nodiscard]] Resource* get(const ResourceId& id) const;
        [[nodiscard]] std::string path(const ResourceId& id, bool full = false) const;
        [[nodiscard]] size_t ref_count(const ResourceId& id) const;

    protected:
        void process_slot_requests(Slot& slot);
        static void process_slot_callbacks(Slot& slot);
        static void process_slot_releases(Slot& slot);

        Resource::Ptr make_resource(const Slot& slot);
        bool has_hanging_resources() const;
        void add_builtins();
        void request_mandatory();
        void release_mandatory();
        void wait_for_loading() const;

        void on_project_loaded(const evt::Arg& arg);

    private:
        /// Фиксированный массив слотов ресурсов (кеш-когерентность)
        std::array<Slot, kMaxResourceCount> slots_;
        /// Пул незадействованных индексов
        IndexPool<size_t> free_slots_;
        /// Индексы задействованных слотов
        std::vector<size_t> active_slots_;
        /// Карта "путь" -> "индекс", для доступа по пути
        std::unordered_map<std::string_view, ResourceId> indices_;
        /// Слушатель события загрузки проекта
        evt::Listener::Ptr evl_on_proj_load_;
        /// ECS-система
        System::Ptr ecs_system_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(res::Manager)
