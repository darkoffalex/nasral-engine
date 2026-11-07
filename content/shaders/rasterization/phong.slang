#include "_common.slang"

// Параметры материала (set = 2, binding = 0)
struct MaterialSettings
{
    float4 color;
    float4 ambient;
    float  shininess;
    float  specular;
    float  normal_nul;
};

// Массивы текстур с комбинированными сэмплерами (set = 3)
[[vk::binding(0, 3)]] Sampler2D t_color[MAX_MATERIALS];
[[vk::binding(1, 3)]] Sampler2D t_normal[MAX_MATERIALS];
[[vk::binding(2, 3)]] Sampler2D t_spec[MAX_MATERIALS];

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(GeometryOutput input)
{
    FragmentOutput output;

    // Выборка из массивов текстур
    // В Slang для Sampler2D используется метод .Sample(uv):
    float4 tex_color = t_color[pc_push.mat_index].Sample(input.uv);
    float3 tex_normal = t_normal[pc_push.mat_index].Sample(input.uv).rgb;
    float tex_specular = t_spec[pc_push.mat_index].Sample(input.uv).r;

    // Получаем параметры материала
    MaterialSettings material = s_materials.Load<MaterialSettings>(pc_push.mat_index * pc_push.mat_stride);

    // Нормаль из tangent space в world space
    float3 normal = normalize(tex_normal * 2.0 - 1.0);
    normal = normalize(mul(input.TBN, normal));

    float3 final_color = float3(0.0);
    float3 ambient = material.ambient.rgb;

    // Цикл по активным источникам света
    for (uint i = 0; i < s_light_indices[0].count; ++i)
    {
        uint idx = s_light_indices[0].active_indices[i];
        if (idx >= MAX_LIGHTS) continue;

        LightSource light = s_lights[idx];

        float dist = length(input.position - light.position.xyz);
        float3 light_dir = normalize(light.position.xyz - input.position);

        // Затухание света
        float attenuation = 1.0 / (1.0 + light.quadratic * dist * dist);

        // Диффузная составляющая
        float diff = max(dot(normal, light_dir), 0.0);
        float3 diffuse = diff
            * material.color.rgb
            * tex_color.rgb
            * light.color.rgb
            * light.intensity;

        // Спекулярная составляющая (Блик по Фонгу)
        float3 view_dir = normalize(u_camera.position.xyz - input.position);
        float3 reflect_dir = reflect(-light_dir, normal);
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), material.shininess);

        float3 specular = spec
            * material.specular
            * tex_specular
            * light.color.rgb
            * light.intensity;

        final_color += (diffuse + specular) * attenuation;
    }

    // Формирование GBuffer вложений
    output.color = float4(ambient + final_color, tex_color.a);
    output.normal = float4(normal * 0.5 + 0.5, 1.0);
    output.emissive = float4(0.0, 0.0, 0.0, 1.0);

    return output;
}