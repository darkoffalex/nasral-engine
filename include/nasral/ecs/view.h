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
            Iterator(const View* view, const size_t archetype_idx, const size_t entity_idx)
                : view_(view)
                , archetype_idx_(archetype_idx)
                , entity_idx_(entity_idx)
            {
                if (view_ == nullptr || view_->manager_->archetypes_.empty()){
                    *this = Iterator{};
                    return;
                }

                advance_archetype();
            }

            value_type operator*() const noexcept{
                return dereference_impl(std::index_sequence_for<CTs...>{});
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
                return view_ == other.view_
                    && archetype_idx_ == other.archetype_idx_
                    && entity_idx_ == other.entity_idx_;
            }

            bool operator!=(const Iterator& other) const noexcept{
                return !(*this == other);
            }

            explicit operator bool() const noexcept
            {
                return *this != Iterator{};
            }

        private:
            void advance_archetype(){
                if (!view_ || archetype_idx_ >= view_->manager_->archetypes_.size()){
                    return;
                }

                const auto filter = kMaskOf<CTs...>;
                while (archetype_idx_ < view_->manager_->archetypes_.size()){
                    const auto& archetype = view_->manager_->archetypes_[archetype_idx_];
                    if ((archetype->mask() & filter) == filter && (archetype->mask() & view_->exclusion_) == 0){
                        if (entity_idx_ < archetype->entities().size()){
                            current_pools_ = std::make_tuple(&archetype->template component_pool<CTs>()...);
                            return;
                        }
                    }
                    entity_idx_ = 0;
                    ++archetype_idx_;
                }

                *this = Iterator{};
            }

            template<std::size_t... Is>
            value_type dereference_impl(std::index_sequence<Is...>) const noexcept {
                auto* arc = view_->manager_->archetypes_[archetype_idx_].get();
                const auto& entity_id = arc->entities()[entity_idx_];
                return std::tuple<EntityId, CTs&...>{
                    entity_id,
                    (*std::get<Is>(current_pools_))[entity_idx_]...
                };
            }

        protected:
            const View* view_ = nullptr;
            size_t archetype_idx_ = 0;
            size_t entity_idx_ = 0;
            std::tuple<std::vector<CTs>*...> current_pools_;
        };

        explicit View(Manager* manager, const ComponentMask& exclusion = {}): manager_(manager), exclusion_(exclusion){}
        Iterator begin() const noexcept{ return Iterator(this, 0, 0); }
        Iterator end() const noexcept{ return Iterator{}; }

    private:
        Manager* manager_ = nullptr;
        ComponentMask exclusion_ = {};
    };


    template<typename... CTs>
    View<CTs...> Manager::view(const ComponentMask& exclusion) {
        return View<CTs...>(this, exclusion);
    }
}
