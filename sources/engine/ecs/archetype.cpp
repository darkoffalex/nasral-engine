#include "pch.h"
#include <nasral/ecs/archetype.h>

namespace nasral::ecs
{
    Archetype::Archetype(const ComponentMask& mask, const size_t max_entities)
        : max_entities_(max_entities)
        , mask_(mask)
    {
        // Зарезервировать память для entities и пулов компонентов
        entities_.reserve(max_entities);
        pools_.reserve(std::tuple_size_v<ComponentTypes>);

        // Fold expression.
        // Пройтись по всем типам компонентов, выделить пулы (под каждый используемый тип)
        // Зарезервировать нужное кол-во памяти в пуле (векторе) под кол-во entities (если компонент нужен)
        std::apply([&]([[maybe_unused]] auto... component_dummies){
            [[maybe_unused]] size_t comp_idx = 0;
            ([&]{
                using ComponentType = decltype(component_dummies);
                pools_.emplace_back(std::vector<ComponentType>());
                if (mask_.test(comp_idx)){
                    auto& vec = std::get<std::vector<ComponentType>>(pools_.back());
                    vec.reserve(max_entities_);
                }
                ++comp_idx;
            }(), ...);
        }, ComponentTypes{});
    }

    Archetype::Result Archetype::add(const EntityId& entity)
    {
        if (entities_.size() >= max_entities_){
            throw std::runtime_error("Archetype is full");
        }

        // Добавить entity
        entities_.push_back(entity);
        // Добавить компоненты в пулы для новой entity
        for (auto& pool : pools_){
            std::visit([&](auto& vec){
                vec.emplace_back();
            }, pool);
        }

        Result r{};
        r.new_idx = entities_.size() - 1;
        return r;
    }

    Archetype::Result Archetype::remove(const EntityId& entity)
    {
        const auto idx = entity_index(entity);
        if (!idx.has_value()) throw std::runtime_error("Entity not found in archetype");
        return remove(idx.value());
    }

    Archetype::Result Archetype::remove(size_t index_in_arch)
    {
        // Если индекс за пределами размера архетипа - ничего не делать
        if (index_in_arch >= size()){
            return {};
        }

        // Если удаляем не последний элемент, то используем swap & pop.
        // В таком случае нужно сохранить информацию о перемещенной entity (и её индексе в архетипе)
        Result r{};
        size_t last_entity_idx = entities_.size() - 1;
        if (last_entity_idx != index_in_arch){
            std::swap(entities_[index_in_arch], entities_[last_entity_idx]);
            for (auto& pool : pools_){
                std::visit([&](auto& vec){
                    std::swap(vec[index_in_arch], vec[last_entity_idx]);
                }, pool);
            }

            r.swapped_idx = index_in_arch;
            r.swapped_entity = entities_[index_in_arch];
        }

        // Удаление последнего (pop)
        entities_.pop_back();
        for (auto& pool : pools_){
            std::visit([&](auto& vec){
                vec.pop_back();
            }, pool);
        }

        return r;
    }

    Archetype::Result Archetype::move(Archetype& src, Archetype& dst, const EntityId& entity)
    {
        // Если целевой архетип полон
        if (dst.size() >= dst.max_entities_){
            throw std::runtime_error("Destination archetype is full");
        }

        // Если исходный архетип пуст
        if (src.empty()){
            throw std::runtime_error("Source archetype is empty");
        }

        // Если в архетипе нет искомой entity
        const auto index_in_src = src.entity_index(entity);
        if (!index_in_src.has_value()){
            throw std::runtime_error("Entity not found in source archetype");
        }

        // Добавление entity в целевой архетип
        const Result addition = dst.add(entity);
        assert(addition.new_idx.has_value());

        // Перенос компонентов между архетипами (только общие типы компонентов задействованы)
        std::apply([&]([[maybe_unused]] auto... component_dummies){
            [[maybe_unused]] size_t comp_idx = 0;
            ([&]{
                if (dst.mask_.test(comp_idx) && src.mask_.test(comp_idx)){
                    using ComponentType = decltype(component_dummies);
                    auto& src_vec = std::get<std::vector<ComponentType>>(src.pools_[comp_idx]);
                    auto& dst_vec = std::get<std::vector<ComponentType>>(dst.pools_[comp_idx]);
                    dst_vec[addition.new_idx.value()] = std::move(src_vec[index_in_src.value()]);
                }
                ++comp_idx;
            }(), ...);
        }, ComponentTypes{});

        // Удаление entity из исходного архетипа
        const Result removal = src.remove(index_in_src.value());

        // Результат переноса
        return {
            addition.new_idx,
            removal.swapped_idx,
            removal.swapped_entity
        };
    }

    std::optional<size_t> Archetype::entity_index(const EntityId& entity) const noexcept
    {
        for (size_t i = 0; i < entities_.size(); ++i){
            if (entities_[i] == entity){
                return i;
            }
        }
        return std::nullopt;
    }
}
