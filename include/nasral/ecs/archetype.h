#pragma once

#include <optional>
#include <nasral/ecs/entity.h>
#include <nasral/ecs/types.h>

namespace nasral::ecs
{
    class Archetype
    {
    public:
        typedef std::unique_ptr<Archetype> Ptr;

        /**
         * @brief Результат операции (добавление, удаление, перемещение)
         * @brief Удаление и перенос между архетипами осуществляется при помощи swap-&-pop.
         * Нередко менеджеру нужно значит индекс entity в архетипе после выполнения операции.
         * Это может быть нужно, чтобы обновить индекс в спике слотов у самого менеджера.
         */
        struct Result
        {
            std::optional<size_t> new_idx = std::nullopt;
            std::optional<size_t> swapped_idx = std::nullopt;
            std::optional<EntityId> swapped_entity = std::nullopt;
        };

        Archetype(const ComponentMask& mask, size_t max_entities);
        Archetype(const Archetype& archetype) = delete;
        Archetype& operator=(const Archetype& archetype) = delete;

        Result add(const EntityId& entity);
        Result remove(const EntityId& entity);
        Result remove(size_t index_in_arch);
        static Result move(Archetype& src, Archetype& dst, const EntityId& entity);

        [[nodiscard]] size_t size() const noexcept{ return entities_.size(); }
        [[nodiscard]] bool empty() const noexcept{ return entities_.empty(); }
        [[nodiscard]] const ComponentMask& mask() const noexcept{ return mask_; }
        [[nodiscard]] const std::vector<EntityId>& entities() const noexcept{ return entities_; }
        [[nodiscard]] EntityId& operator[](const size_t index) noexcept{ return entities_[index]; }

        template<typename ComponentType>
        [[nodiscard]] bool has() const noexcept{
            const size_t component_id = kComponentId<ComponentType>;
            return mask_.test(component_id);
        }

        template<typename ComponentType>
        [[nodiscard]] ComponentType& component(size_t index_in_arch){
            const auto cmp_idx = kComponentId<ComponentType>;
            return std::get<std::vector<ComponentType>>(pools_[cmp_idx])[index_in_arch];
        }

        template<typename... CTs>
        [[nodiscard]] std::tuple<CTs&...> components(size_t index){
            return std::tie(component<CTs>(index)...);
        }

    private:
        [[nodiscard]] std::optional<size_t> entity_index(const EntityId& entity) const noexcept;

    protected:
        size_t max_entities_;
        ComponentMask mask_{};
        std::vector<EntityId> entities_{};
        std::vector<ComponentPool> pools_{};
    };
}