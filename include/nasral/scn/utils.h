#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

namespace nasral::scn
{
    /**
     * Вычислить вектор движения с учетом направления/поворота
     * @param movement Направление движения на плоскости (игнорируем ось Y)
     * @param orientation Ориентация (в градусах, вращение по Y - лево/право и X - вверх/вниз)
     * @return Ориентированный вектор движения
     */
    inline glm::vec3 calc_rot_movement(const glm::vec2& movement, const glm::vec3& orientation)
    {
        // Матрица поворота
        const glm::mat4 rot_m =
            glm::rotate(glm::mat4(1.0f), glm::radians(orientation.y),glm::vec3(0.0f,1.0f,0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(orientation.x),glm::vec3(1.0f,0.0f,0.0f));

        // Игнорируем ось Y при получении (учитывается только движение в горизонтальной плоскости)
        const auto mov_nrm = glm::length2(movement) > 0.0f
            ? glm::normalize(glm::vec3(movement.x, 0.0f, movement.y))
            : glm::vec3(0.0f);

        // Получить направленное движение (в сторону обзора)
        const auto mov_rot = rot_m * glm::vec4(mov_nrm, 0.0f);
        return {mov_rot.x, mov_rot.y, mov_rot.z};
    }
}
