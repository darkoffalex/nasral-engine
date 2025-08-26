#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable

// Константы
#define PI 3.14159
#define MAX_OBJECTS 1000
#define MAX_MATERIALS 100
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
    uint mat_index;
    uint obj_index;
} pc_push;

// Параметры одиночного материала
struct MaterialSettings {
    vec4 color;
    float roughness;
    float metallic;
    float ao;
    float emission;
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

// Storage buffer для PBR материалов
layout(set = 2, binding = 1, std430) readonly buffer SMaterials {
    MaterialSettings s_materials[MAX_MATERIALS];
};

// Storage buffer для источников света
layout(set = 4, binding = 0, std430) readonly buffer SLightSources {
    LightSource s_lights[MAX_LIGHTS];
};

// Storage buffer для индексов активных источников
layout(set = 4, binding = 1, std430) readonly buffer SLightIndices {
    LightIndices s_light_indices;
};

// Текстуры объекта
layout(set = 3, binding = 0) uniform sampler2D t_aldeo[MAX_MATERIALS];
layout(set = 3, binding = 1) uniform sampler2D t_normal[MAX_MATERIALS];
layout(set = 3, binding = 2) uniform sampler2D t_roughness[MAX_MATERIALS];
layout(set = 3, binding = 3) uniform sampler2D t_height[MAX_MATERIALS];
layout(set = 3, binding = 4) uniform sampler2D t_metallic[MAX_MATERIALS];
layout(set = 3, binding = 5) uniform sampler2D t_ao[MAX_MATERIALS];

/**
 * @brief Normal Distribution Function
 * @details Описывает, как микронормали поверхности ориентированы относительно нормали.
 * Для гладких поверхностей микрофасеты более упорядочены, что дает резкие блики.
 *
 * @param NoH Скалярное произведение нормали поверхности (N) и полувектора (H).
 * @return Коэффициент D для Cook-Torrance BRDF
 */
float D_GGX(float NoH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;
    float denom = NoH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

/**
 * @brief Geometry Function
 * @details Учитывает самозатенение микрофасет (когда одни неровности закрывают другие).
 * Это делает блики реалистичными на шероховатых поверхностях.
 *
 * @param NoV Скалярное произведение нормали поверхности (N) и направления взгляда (V).
 * @param NoL Скалярное произведение нормали поверхности (N) и направления к источнику света (L).
 * @return Коэффициент G для Cook-Torrance BRDF
 */
float G_Smith(float NoV, float NoL, float roughness)
{
//    float a = roughness * roughness;
//    float k = a * a / 2.0;
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float G1V = NoV / (NoV * (1.0 - k) + k);
    float G1L = NoL / (NoL * (1.0 - k) + k);
    return G1V * G1L;
}

/**
 * @brief Fresnel Function
 * @details Моделирует эффект Френеля, усиливая отражение при взгляде под углом.
 *
 * @param VoH Скалярное произведение направления взгляда (V) и полувектора (H).
 * @param F0 Цвет отраженного света при эффекте Френеля
 * @return Коэффициент F для Cook-Torrance BRDF
 */
vec3 F_Schlick(float VoH, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);
}

void main()
{
    // Данные из текстур объекта
    vec4  tex_albedo = texture(t_aldeo[pc_push.mat_index], fs_in.uv);
    vec3  tex_normal = texture(t_normal[pc_push.mat_index], fs_in.uv).rgb;
    float tex_roughness = texture(t_roughness[pc_push.mat_index], fs_in.uv).r;
    float tex_metallic = texture(t_metallic[pc_push.mat_index], fs_in.uv).r;
    float tex_ao = texture(t_ao[pc_push.mat_index], fs_in.uv).r;

    // Параметры материала объекта
    MaterialSettings material = s_materials[pc_push.mat_index];
    float alpha = material.color.a * tex_albedo.a;
    vec3  albedo = material.color.rgb * tex_albedo.rgb;
    float roughness = clamp(material.roughness * tex_roughness, 0.0f, 1.0f);
    float metallic = clamp(material.metallic * tex_metallic, 0.0f, 1.0f);

    // Преобразуем нормаль из пространства касательных в мировое пространство
    vec3 normal = normalize(tex_normal * 2.0 - 1.0); // Из [0,1] в [-1,1]
    normal = normalize(fs_in.TBN * normal);

    // Сумарная освещенность точки (фрагмента)
    vec3 Lo = vec3(0.0f,0.0f,0.0f);

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

        // Облученность (интенсивность освещенности) точки (фрагмента) конкретным источником
        vec3 radiance = light.color.rgb * attenuation * light.intensity;

        // Коэфициенты для растчета PBR
        vec3 V = normalize(u_camera.position.xyz - fs_in.position); // Направление фрагмент-камера
        vec3 N = normal;                           // Нормаль
        vec3 H = normalize(light_dir + V);         // Полувектор между направлением света (L) и направлением взгляда (V)
        float NoL = max(dot(N, light_dir), 0.0);   // Скалярное произведение нормали поверхности (N) и направления к источнику света (L).
        float NoV = max(dot(N, V), 0.0);           // Скалярное произведение нормали поверхности (N) и направления взгляда (V).
        float NoH = max(dot(N, H), 0.0);           // Скалярное произведение нормали поверхности (N) и полувектора (H).
        float VoH = max(dot(V, H), 0.0);           // Скалярное произведение направления взгляда (V) и полувектора (H).

        // Френель
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = F_Schlick(VoH, F0);

        // Спекулярное
        float D = D_GGX(NoH, roughness);
        float G = G_Smith(NoV, NoL, roughness);
        vec3 specular = (D * G * F) / 4.0 * max(NoV * NoL, 0.0) + 0.0001;

        // Диффузное
        vec3 diffuse = albedo * (1.0 - metallic) * (1.0 - F) / PI;

        // Суммируем вклад света
        Lo += (diffuse + specular) * NoL * radiance * tex_ao;
    }

    // Компрессия (на текущий момент нет HDR)
    vec3 col = Lo;
    col = col / (col + vec3(1.0));
    col = pow(col, vec3(1.0/2.2));

    // Итоговый цвет: вклад от всех источников
    color = vec4(col, alpha);
}