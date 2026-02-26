#pragma once

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
        enum UnpackFlags : uint16_t {
            eUFStandard      = 0,
            eUFDeferred      = 1 << 0,
            eUFSkipRootComp  = 1 << 1,
            eUFCreate        = 1 << 2
        };

        typedef std::unique_ptr<IOStruct> Ptr;

        explicit IOStruct(Engine* engine) : engine_(engine) {}
        virtual ~IOStruct() = default;

        virtual void unpack_to(const ecs::EntityId& entity_id, UnpackFlags flags) const = 0;
        virtual void pack_from(ecs::EntityId& entity_id) = 0;

        [[nodiscard]] Engine* engine() const { return engine_; }

    private:
        Engine* const engine_;
    };

    inline IOStruct::UnpackFlags operator|(
        const IOStruct::UnpackFlags a,
        const IOStruct::UnpackFlags b)
    {
        using U = std::underlying_type_t<IOStruct::UnpackFlags>;
        return static_cast<IOStruct::UnpackFlags>(
            static_cast<U>(a) | static_cast<U>(b)
        );
    }

    inline IOStruct::UnpackFlags operator&(
        const IOStruct::UnpackFlags a,
        const IOStruct::UnpackFlags b)
    {
        using U = std::underlying_type_t<IOStruct::UnpackFlags>;
        return static_cast<IOStruct::UnpackFlags>(
            static_cast<U>(a) & static_cast<U>(b)
        );
    }

    inline IOStruct::UnpackFlags operator~(const IOStruct::UnpackFlags a)
    {
        using U = std::underlying_type_t<IOStruct::UnpackFlags>;
        return static_cast<IOStruct::UnpackFlags>(~static_cast<U>(a));
    }

    inline IOStruct::UnpackFlags& operator|=(
        IOStruct::UnpackFlags& a,
        const IOStruct::UnpackFlags b)
    {
        a = a | b;
        return a;
    }

    inline IOStruct::UnpackFlags& operator&=(
        IOStruct::UnpackFlags& a,
        const IOStruct::UnpackFlags b)
    {
        a = a & b;
        return a;
    }
}