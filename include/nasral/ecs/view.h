#pragma once
#include <nasral/ecs/manager.h>

namespace nasral::ecs
{
    template<typename... CTs>
    class View
    {
    public:
        class Iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = std::tuple<EntityId, CTs&...>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            Iterator() = default;
            Iterator(Manager* manager, const size_t archetype_idx, const size_t entity_idx)
                : manager_(manager)
                , archetype_idx_(archetype_idx)
                , entity_idx_(entity_idx)
            {
                if (manager_ == nullptr || manager_->archetypes_.empty()){
                    *this = Iterator{};
                    return;
                }

                advance_archetype();
            }

            value_type operator*() const noexcept{
                auto* arc = manager_->archetypes_[archetype_idx_].get();
                return std::tuple_cat(
                    std::make_tuple(arc->entities()[entity_idx_]),
                    arc->get_components<CTs...>(arc->entities()[entity_idx_]));
            }

            Iterator& operator++(){
                ++entity_idx_;
                advance_archetype();
                return *this;
            }

            Iterator operator++(int){
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const Iterator& other) const noexcept{
                return manager_ == other.manager_
                    && archetype_idx_ == other.archetype_idx_
                    && entity_idx_ == other.entity_idx_;
            }

            bool operator!=(const Iterator& other) const noexcept{
                return !(*this == other);
            }

        private:
            void advance_archetype(){
                if (!manager_ || archetype_idx_ >= manager_->archetypes_.size()){
                    return;
                }

                const auto& filter = kMaskOf<CTs...>;
                while (archetype_idx_ < manager_->archetypes_.size()){
                    const auto& archetype = manager_->archetypes_[archetype_idx_];
                    if ((archetype->mask() && filter) == filter){
                        if (entity_idx_ < archetype->entities().size()){
                            return;
                        }
                    }
                    entity_idx_ = 0;
                    ++archetype_idx_;
                }

                *this = Iterator{};
            }

        protected:
            Manager* manager_ = nullptr;
            size_t archetype_idx_ = 0;
            size_t entity_idx_ = 0;
        };

        explicit View(Manager* manager): manager_(manager){}
        Iterator begin() const noexcept{ return Iterator(manager_, 0, 0); }
        Iterator end() const noexcept{ return Iterator{}; }

    private:
        Manager* manager_ = nullptr;
    };


    template<typename... CTs>
    View<CTs...> Manager::view() {
        return View<CTs...>(this);
    }
}
