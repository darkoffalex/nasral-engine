#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/scn/types.h>
#include <nasral/scn/objects/node.h>
#include <nasral/evt/objects/listener.h>

namespace nasral::scn
{
    class Manager final : public Subsystem<Manager, Config>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;

        explicit Manager(Engine* e, const Config& config);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void on_init();
        void on_update(float delta);
        void on_finalize();

        void spawn(const NodeDesc& desc);
        void remove(const Node* node);
        void remove(const UniqueId& id);
        [[nodiscard]] Node* find(const UniqueId& id) const;

    protected:
        void on_session_start(const evt::Arg& arg);
        void load_initial_scene(const std::string& path);

    private:
        std::vector<Node::Ptr> nodes_;
        evt::Listener::Ptr evl_session_start_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(scn::Manager, "SCN")
