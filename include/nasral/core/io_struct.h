#pragma once
#include "nasral/engine.h"

namespace nasral::ecs {
    struct EntityId;
}

namespace nasral {
    class Engine;
}

namespace nasral::core
{
    struct IOStruct
    {
        typedef std::unique_ptr<IOStruct> Ptr;

        explicit IOStruct(Engine* engine) : engine_(engine) {}
        virtual ~IOStruct() = default;

        virtual void unpack_to(const ecs::EntityId& entity_id) const = 0;
        virtual void pack_from(ecs::EntityId& entity_id) = 0;

        [[nodiscard]] Engine* engine() const { return engine_; }

    private:
        Engine* const engine_;
    };
}