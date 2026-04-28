#include "pch.h"
#include <nasral/ecs/manager.h>

namespace nasral::ecs
{
    Manager::Manager(Engine* e, const Config& config) : Subsystem(e, config)
    {
        entities_.reserve(config.max_entities);
        free_slots_.reserve(config.max_entities);
    }

    EntityId Manager::spawn(){
        if (!free_slots_.empty()){
            const auto idx = free_slots_.back();
            free_slots_.pop_back();

            auto& [id, archetype, mask, index_in_arch] = entities_[idx];
            archetype = nullptr;
            index_in_arch = 0;
            mask.reset();
            return id;
        }

        if (entities_.size() >= entities_.capacity()){
            throw std::runtime_error("Entity pool is full");
        }

        entities_.emplace_back(
            EntitySlot{
                EntityId{entities_.size(), 0},
                nullptr,
                0,
                0
            });

        return entities_.back().id;
    }

    void Manager::destroy(const EntityId& entity){
        defer([entity](Manager& m){
            m.destroy_immediate(entity);
        });
    }

    void Manager::destroy_immediate(const EntityId& entity){
        if (entity.index >= entities_.size() || !is_valid(entity)){
            return;
        }

        // При удалении из архетипа возможно перемещение (swap & pop)
        auto& slot = entities_[entity.index];
        const auto removal = slot.archetype->remove(entity);

        // Если при удалении было перемещение внутри архетипа (у какой-то entity сменился индекс):
        // Нужно обновить поле "индекс внутри архетипа" у соответствующего слота
        if (removal.swapped_entity.has_value()){
            assert(removal.swapped_idx.has_value() && "Swapped entity index missing");
            const auto& [swapped_idx, swapped_ver] = removal.swapped_entity.value();
            entities_[swapped_idx].index_in_arch = removal.swapped_idx.value();
        }

        slot.archetype = nullptr;
        slot.index_in_arch = 0;
        slot.id.version++;
        slot.mask.reset();

        free_slots_.push_back(entity.index);
    }

    Archetype* Manager::ensure_archetype(const ComponentMask& mask){
        for (const auto& archetype : archetypes_){
            if (archetype->mask() == mask){
                return archetype.get();
            }
        }

        archetypes_.emplace_back(std::make_unique<Archetype>(
            mask,
            config().max_entities));

        return archetypes_.back().get();
    }

    void Manager::assign_archetype(EntitySlot& slot, Archetype* archetype)
    {
        assert(archetype != nullptr && "Archetype is null");
        if (!archetype){
            return;
        }

        // У слота нет архетипа (новая entity)
        if (!slot.archetype){
            const auto addition = archetype->add(slot.id);
            assert(addition.new_idx.has_value() && "New entity index missing");

            slot.archetype = archetype;
            slot.index_in_arch = addition.new_idx.value();
            slot.mask = archetype->mask();
        }

        // Entity принадлежит другому архетипу
        else if (slot.archetype != archetype){
            auto* previous_arch = slot.archetype;
            const auto transition = Archetype::move(*previous_arch, *archetype, slot.id);

            // Если при удалении было перемещение внутри архетипа (у какой-то entity сменился индекс):
            // Нужно обновить поле "индекс внутри архетипа" у соответствующего слота
            if (transition.swapped_entity.has_value()){
                assert(transition.swapped_idx.has_value() && "Swapped entity index missing");
                const auto& [swapped_idx, swapped_ver] = transition.swapped_entity.value();
                entities_[swapped_idx].index_in_arch = transition.swapped_idx.value();
            }

            slot.archetype = archetype;
            slot.index_in_arch = transition.new_idx.value();
            slot.mask = archetype->mask();

            // Если в прежнем архетипе не осталось сущностей - удалить архетип
            if (previous_arch->entities().empty()){
                archetypes_.erase(std::remove_if(archetypes_.begin(), archetypes_.end(), [&](const Archetype::Ptr& a){
                    return a.get() == previous_arch;
                }), archetypes_.end());
            }
        }
    }
}
