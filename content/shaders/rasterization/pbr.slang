#include "_common.slang"

// Константы
static const float PI = 3.14159265359;
static const float3 LUMINANCE_VECTOR = float3(0.2126, 0.7152, 0.0722);
static const float BRIGHT_THRESHOLD = 0.5;

// PBR параметры материала (set = 2, binding = 0)
struct MaterialSettings
{
    float4 color;
    float  roughness;
    float  metallic;
    float  ao;
    float  emission;
    float  normal_mul;
};

// Текстуры PBR материала (set = 3)
[[vk::binding(0, 3)]] Sampler2D t_albedo[MAX_MATERIALS];
[[vk::binding(1, 3)]] Sampler2D t_normal[MAX_MATERIALS];
[[vk::binding(2, 3)]] Sampler2D t_roughness[MAX_MATERIALS];
[[vk::binding(3, 3)]] Sampler2D t_height[MAX_MATERIALS];
[[vk::binding(4, 3)]] Sampler2D t_metallic[MAX_MATERIALS];
[[vk::binding(5, 3)]] Sampler2D t_ao[MAX_MATERIALS];

// ============================================================================
// PBR Функции (Cook-Torrance BRDF)
// ============================================================================

// Normal Distribution Function (Trowbridge-Reitz GGX)
float D_GGX(float NoH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;
    float denom = NoH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Geometric Shadowing (Smith's method with Schlick-GGX)
float G_Smith(float NoV, float NoL, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float G1V = NoV / (NoV * (1.0 - k) + k);
    float G1L = NoL / (NoL * (1.0 - k) + k);
    return G1V * G1L;
}

// Fresnel-Schlick Approximation
float3 F_Schlick(float VoH, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);
}

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(GeometryOutput input)
{
    FragmentOutput output;

    // Сэмплирование текстур
    float4 tex_albedo    = t_albedo[pc_push.mat_index].Sample(input.uv);
    float3 tex_normal    = t_normal[pc_push.mat_index].Sample(input.uv).rgb;
    float  tex_roughness = t_roughness[pc_push.mat_index].Sample(input.uv).r;
    float  tex_metallic  = t_metallic[pc_push.mat_index].Sample(input.uv).r;
    float  tex_ao        = t_ao[pc_push.mat_index].Sample(input.uv).r;

    // Параметры материала
    MaterialSettings material = s_materials.Load<MaterialSettings>(pc_push.mat_index * pc_push.mat_stride);

    float  alpha     = material.color.a * tex_albedo.a;
    float3 albedo    = material.color.rgb * tex_albedo.rgb;
    float  roughness = clamp(material.roughness * tex_roughness, 0.0, 1.0);
    float  metallic  = clamp(material.metallic * tex_metallic, 0.0, 1.0);
    float3 emission  = material.emission * albedo;
    float normal_mul = material.normal_mul;

    // Преобразование нормали из Tangent Space в World Space
    float3 normal = normalize(tex_normal * 2.0 - 1.0);
    normal.xy *= normal_mul;
    normal = normalize(normal);
    normal = normalize(mul(input.TBN, normal));

    // Суммарная освещенность точки
    float3 Lo = float3(0.0);

    // Цикл по активным источникам света
    for (uint i = 0; i < s_light_indices[0].count; ++i)
    {
        uint idx = s_light_indices[0].active_indices[i];
        if (idx >= MAX_LIGHTS) continue;

        LightSource light = s_lights[idx];

        float  dist      = length(input.position - light.position.xyz);
        float3 light_dir = normalize(light.position.xyz - input.position);

        // Затухание
        float attenuation = 1.0 / (1.0 + light.quadratic * dist * dist);
        float3 radiance   = light.color.rgb * attenuation * light.intensity;

        // Векторы и скалярные произведения
        float3 V   = normalize(u_camera.position.xyz - input.position);
        float3 N   = normal;
        float3 H   = normalize(light_dir + V);
        float  NoL = max(dot(N, light_dir), 0.0);
        float  NoV = max(dot(N, V), 0.0);
        float  NoH = max(dot(N, H), 0.0);
        float  VoH = max(dot(V, H), 0.0);

        // Френель при нормальном падении (0.04 для диэлектриков, albedo для металлов)
        float3 F0 = lerp(float3(0.04), albedo, metallic);
        float3 F  = F_Schlick(VoH, F0);

        // Спекулярная часть Cook-Torrance
        float  D        = D_GGX(NoH, roughness);
        float  G        = G_Smith(NoV, NoL, roughness);
        float3 specular = (D * G * F) / (4.0 * max(NoV * NoL, 0.0) + 0.0001);

        // Диффузная часть (энергосбережение Lambertian)
        float3 diffuse = albedo * (1.0 - metallic) * (1.0 - F) / PI;

        // Накопление освещенности
        Lo += (diffuse + specular) * NoL * radiance * tex_ao;
    }

    // Применить emissive (светящиеся поверхности)
    // Если свечение ярче освещенности - мы увидим свечение, в противном случае - освещенность
    Lo = max(Lo, float3(emission));

    // Буфер свечения (яркость выше порога)
    float3 bright_pbr = max( Lo - float3(BRIGHT_THRESHOLD), float3(0.0));

    // Вывод во вложения GBuffer
    output.color    = float4(Lo, alpha);
    output.normal   = float4(normal * 0.5 + 0.5, 1.0);
    output.emissive = float4(bright_pbr, 1.0);

    return output;
}