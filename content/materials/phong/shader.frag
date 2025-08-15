#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable

// Константы
#define MAX_OBJECTS 1000
#define MAX_LIGHTS 100

// Входные данные фрагмента
layout(location = 0) in GS_OUT {
    vec3 color;
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec3 tangent;
    vec3 bitangent;
    mat3 TBN;
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
    vec4 ambient;
    float shininess;
    float specular;
};

// Параметры одиночного источника света
struct LightSource {
    vec4 position;
    vec4 direction;
    vec4 color;
    mat4 space;
    float quadratic;
    float radius;
    float intensity;
};

// Индексы активных источников света
struct LightIndices {
    uint count;
    uint active_indices[MAX_LIGHTS];
};

// Uniform buffer для матриц камеры
layout(set = 0, binding = 0, std140) uniform UCamera {
    mat4 view;
    mat4 proj;
    vec4 position;
} u_camera;

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
    // Получаем данные из текстур
    vec4 tex_color = texture(t_color[pc_push.obj_index], fs_in.uv);
    vec3 tex_normal = texture(t_normal[pc_push.obj_index], fs_in.uv).rgb;
    float tex_specular = texture(t_spec[pc_push.obj_index], fs_in.uv).r;

    // Получаем настройки материала
    MaterialSettings material = s_materials[pc_push.obj_index];

    // Преобразуем нормаль из пространства касательных в мировое пространство
    vec3 normal = normalize(tex_normal * 2.0 - 1.0); // Из [0,1] в [-1,1]
    normal = normalize(fs_in.TBN * normal);

    // Инициализируем итоговый цвет
    vec3 final_color = vec3(0.0);

    // Параметры окружающего освещения
    vec3 ambient = material.ambient.rgb;

    // Обработка активных источников света
    for (uint i = 0u; i < s_light_indices.count; ++i)
    {
        // Получить источник
        uint idx = s_light_indices.active_indices[i];
        if (idx >= MAX_LIGHTS) continue;
        LightSource light = s_lights[idx];

        // Вычисляем расстояние и направление к источнику света
        float dist = length(fs_in.position - light.position.xyz);
        vec3 light_dir = normalize(light.position.xyz - fs_in.position);

        // Вычисляем затухание
        float attenuation = 1.0 / (1.0 + light.quadratic * dist * dist);

        // Диффузное освещение
        float diff = max(dot(normal, light_dir), 0.0);
        vec3 diffuse = diff
            * material.color.rgb
            * tex_color.rgb
            * light.color.rgb
            * light.intensity;

        // Спекулярное освещение
        vec3 view_dir = normalize(u_camera.position.xyz - fs_in.position);
        vec3 reflect_dir = reflect(-light_dir, normal);
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), material.shininess);

        vec3 specular = spec
            * material.specular
            * tex_specular
            * light.color.rgb
            * light.intensity;

        // Суммируем вклад света с учетом затухания
        final_color += (diffuse + specular) * attenuation;
    }

    // Итоговый цвет: окружающее + вклад от всех источников
    color = vec4(ambient + final_color, tex_color.a);
}