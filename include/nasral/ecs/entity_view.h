#pragma once
#include <nasral/ecs/manager.h>
#include <nasral/common/utils.h>

namespace nasral::ecs
{
    template<typename... CTs>
    class EntityView : public SubsystemObject<Manager>
    {
    public:
        using RequiredComponents = std::tuple<CTs...>;
        using UnderlyingView = View<CTs...>;

        EntityView(const EntityId id, Manager* mgr, CTs&... components)
            : SubsystemObject(mgr)
            , entity_(id)
            , components_(components...)
        {}

        [[nodiscard]] EntityId id() const noexcept { return entity_; }

        template<typename T>
        [[nodiscard]] T& get() noexcept {
            return std::get<std::reference_wrapper<T>>(components_).get();
        }

        template<typename T>
        [[nodiscard]] const T& get() const noexcept {
            return std::get<std::reference_wrapper<T>>(components_).get();
        }

        template<typename T>
        [[nodiscard]] std::optional<T> as_view() const noexcept {
            return subsystem()->template as_view<T>(id());
        }

    protected:
        EntityId entity_;
        std::tuple<std::reference_wrapper<CTs>...> components_;
    };
}