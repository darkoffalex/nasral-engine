#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/scn/types.h>
#include <nasral/scn/objects/node.h>

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

        void on_init() const;
        void on_update(float delta);
        void on_finalize() const;

    private:
        std::vector<Node::Ptr> nodes_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(scn::Manager, "SCN")
