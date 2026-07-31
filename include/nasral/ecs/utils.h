#pragma once

#include <nasral/ecs/entity.h>

namespace nasral::ecs
{
    class Manager;

    /**
     * Увеличивает кол-во ссылок на entity
     * @details Для ситуаций, когда одни entity ссылаются на другие entity
     * @param m ECS менеджер
     * @param entity_id ID entity
     * @param immediate Не использовать отложенные ECS операции
     */
    void inc_entity_refs(Manager* m, const EntityId& entity_id, bool immediate = false);

    /**
     * Уменьшает кол-во ссылок на entity
     * @details Для ситуаций, когда одни entity ссылаются на другие entity
     * @param m ECS менеджер
     * @param entity_id ID entity
     * @param immediate Не использовать отложенные ECS операции
     */
    void dec_entity_refs(Manager* m, const EntityId& entity_id, bool immediate = false);
}
