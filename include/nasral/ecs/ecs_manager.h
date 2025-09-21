#pragma once
#include <nasral/ecs/ecs_types.h>
#include <nasral/ecs/ecs_archetype.h>

namespace nasral{class Engine;}
namespace nasral::logging{class Logger;}

namespace nasral::ecs
{
    class EcsManager {
    public:
        typedef std::unique_ptr<EcsManager> Ptr;

        struct EntitySlot {
            EntityId id = {};
            Archetype* archetype = nullptr;
            ComponentMask mask = {};
            size_t archetype_index = 0;
            bool is_alive = false;
        };

        template<typename... Ts>
        class View {
        public:
            class Iterator {
            public:
                using iterator_category = std::forward_iterator_tag;
                using value_type = std::tuple<EntityId, Ts&...>;
                //using difference_type = std::ptrdiff_t;
                using pointer = value_type*;
                using reference = value_type&;

                Iterator() = default;
                Iterator(EcsManager* manager, const size_t arch_index, const size_t ent_index)
                    : manager_(manager)
                    , archetype_index_(arch_index)
                    , entity_index_(ent_index)
                {
                    if (manager_ == nullptr || manager_->archetypes_.empty()){
                        *this = Iterator();
                        return;
                    }

                    advance_archetype();
                }

                value_type operator*() const {
                    auto* archetype = manager_->archetypes_[archetype_index_].get();
                    return std::tuple_cat(
                        std::make_tuple(archetype->entities()[entity_index_]),
                        archetype->get_components<Ts...>(entity_index_)
                    );
                }

                Iterator& operator++() {
                    ++entity_index_;
                    advance_archetype();
                    return *this;
                }

                Iterator operator++(int) {
                    Iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }

                bool operator==(const Iterator& other) const {
                    return manager_ == other.manager_ &&
                        archetype_index_ == other.archetype_index_ &&
                        entity_index_ == other.entity_index_;
                }

                bool operator!=(const Iterator& other) const {
                    return !(*this == other);
                }

            private:
                void advance_archetype() {
                    // Для end итератора
                    if (!manager_ || archetype_index_ >= manager_->archetypes_.size()){
                        return;
                    }
                    // Менять на подходящий архетип в том случае, если индекс entity подошел к концу
                    const auto required_mask = kMaskOf<Ts...>;
                    while (archetype_index_ < manager_->archetypes_.size()) {
                        const auto& arc = manager_->archetypes_[archetype_index_];
                        if ((arc->mask() & required_mask) == required_mask) {
                            if (entity_index_ < arc->entities().size()) {
                                return;
                            }
                        }
                        entity_index_ = 0;
                        ++archetype_index_;
                    }
                    // После прохода по всем архетипам (становление end итератором)
                    *this = Iterator();
                }

            protected:
                EcsManager* manager_ = nullptr;
                size_t archetype_index_ = 0;
                size_t entity_index_ = 0;
            };

            explicit View(EcsManager* manager) : manager_(manager) {}
            Iterator begin() const { return Iterator(manager_, 0, 0); }
            Iterator end() const { return Iterator(); }

        protected:
            EcsManager* manager_ = nullptr;
        };

        EcsManager(const Engine* engine, const EcsConfig& config);

        [[nodiscard]] EntityId create_entity();
        void destroy_entity(const EntityId& id);
        void enable_components(const EntityId& id, const ComponentMask& mask);
        void disable_components(const EntityId& id, const ComponentMask& mask);

        template<typename Component>
        void set_component(const EntityId& id, const Component&& component){
            assert(id.index < entities_.size());
            assert(entities_[id.index].archetype != nullptr);

            const auto& slot = entities_[id.index];
            slot.archetype->get_component<Component>(slot.archetype_index) = std::forward<Component>(component);
        }

        template<typename Component>
        void reset_component(const EntityId& id) const{
            assert(id.index < entities_.size());
            assert(entities_[id.index].archetype != nullptr);

            const auto& slot = entities_[id.index];
            slot.archetype->get_component<Component>(slot.archetype_index) = std::move(Component{});
        }

        template<typename Component>
        Component& get_component(const EntityId& id){
            assert(id.index < entities_.size());
            assert(entities_[id.index].archetype != nullptr);

            const auto& slot = entities_[id.index];
            return slot.archetype->get_component<Component>(slot.archetype_index);
        }

        template<typename... Ts>
        std::tuple<Ts&...> get_components(const EntityId& id){
            assert(id.index < entities_.size());
            assert(entities_[id.index].archetype != nullptr);

            const auto& slot = entities_[id.index];
            return slot.archetype->get_components<Ts...>(slot.archetype_index);
        }

        [[nodiscard]] bool entity_alive(const EntityId& id) const{
            assert(id.index < entities_.size());
            return entities_[id.index].is_alive;
        }

        [[nodiscard]] bool entity_valid(const EntityId& id) const{
            assert(id.index < entities_.size());

            return entities_[id.index].archetype != nullptr
                && entities_[id.index].is_alive
                && entities_[id.index].id == id;
        }

        template<typename... Ts>
        View<Ts...> view() {
            return View<Ts...>(this);
        }

    private:
        [[nodiscard]] const logging::Logger* logger() const;
        [[nodiscard]] Archetype* find_or_create_archetype(const ComponentMask& mask);
        void assign_archetype(EntitySlot& slot, Archetype* dst_archetype);

    protected:
        SafeHandle<const Engine> engine_;
        EcsConfig config_;
        std::vector<EntitySlot> entities_;
        std::vector<size_t> freed_slots_;
        std::vector<Archetype::Ptr> archetypes_;
    };
}
