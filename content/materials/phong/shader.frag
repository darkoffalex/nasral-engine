#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable

// Константы
#define MAX_OBJECTS 1000
#define MAX_LIGHTS 100

// Входные данные фрагмента
layout(location = 0) in VS_OUT {
    vec3 color;
    vec3 position;
    vec3 normal;
    vec2 uv;
} fs_in;

// Выход фрагмента (цветовое вложение 0)
layout(location = 0) out vec4 color;

// Push constants
layout(push_constant) uniform PushConstants {
    uint obj_index;
} pc_push;

// Параметры одиночного материала
struct MaterialSettings {
    vec4 color;
    float shininess;
    float specular;
};

// Параметры одиночного источника света
struct LightSource {
    vec4 position;
    vec4 direction;
    vec4 color;
    mat4 space;
    float radius;
    float intensity;
};

// Индексы актичных источников света
struct LightIndices {
    uint count;
    uint active_indices[MAX_LIGHTS];
};

// Storage buffer для материалов объектов
layout(set = 1, binding = 1, std430) readonly buffer SMaterials {
    MaterialSettings s_materials[MAX_OBJECTS];
};

// Storage buffer для источников света
layout(set = 3, binding = 0, std430) readonly buffer SLightSources {
    LightSource s_lights[MAX_LIGHTS];
};

// Storage buffer для индексов активных источников
layout(set = 3, binding = 1, std430) readonly buffer SLightIndices {
    LightIndices s_light_indices;
};

// Текстуры объекта
layout(set = 2, binding = 0) uniform sampler2D t_color[MAX_OBJECTS];
layout(set = 2, binding = 1) uniform sampler2D t_normal[MAX_OBJECTS];
layout(set = 2, binding = 2) uniform sampler2D t_spec[MAX_OBJECTS];

void main()
{
    // Цвет из текстуры
    vec4 tex_color = texture(t_color[pc_push.obj_index], fs_in.uv);

    // Цвет выхода по умолчанию: из текстуры
    color = vec4(tex_color.rgb, 1.0);

    // Обработать активные источники
    for (uint i = 0u; i < s_light_indices.count; ++i)
    {
        uint idx = s_light_indices.active_indices[i];
        if (idx >= MAX_LIGHTS) continue;

        LightSource light = s_lights[idx];

        // TODO: Реализовать модель Фонга
        float dist = length(fs_in.position - light.position.xyz);
        if (dist <= light.radius)
        {
            color = vec4(1.0, 0.0, 0.0, 1.0);
            break;
        }
    }
}