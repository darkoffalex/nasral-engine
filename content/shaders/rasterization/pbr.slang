// Константы
static const float PI = 3.14159265359;
static const uint MAX_MATERIALS = 100;
static const uint MAX_LIGHTS = 100;
static const float3 LUMINANCE_VECTOR = float3(0.2126, 0.7152, 0.0722);
static const float BRIGHT_THRESHOLD = 0.5;

// ============================================================================
// Входные и промежуточные структуры данных
// ============================================================================

// Входные атрибуты из вершинного буфера
struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
    [[vk::location(3)]] float4 color    : COLOR;
};

// Выход вершинного шейдера -> Вход геометрического шейдера
struct VertexOutput
{
    float4 clipPosition : SV_Position;
    [[vk::location(0)]] float3 color    : COLOR0;
    [[vk::location(1)]] float3 position : POSITION;
    [[vk::location(2)]] float3 normal   : NORMAL;
    [[vk::location(3)]] float2 uv       : TEXCOORD0;
};

// Выход геометрического шейдера -> Вход фрагментного шейдера
// Примечание: TBN (float3x3) занимает 3 последовательных location: 6, 7, 8
struct GeometryOutput
{
    float4 clipPosition : SV_Position;
    [[vk::location(0)]] float3 color     : COLOR0;
    [[vk::location(1)]] float3 position  : POSITION;
    [[vk::location(2)]] float3 normal    : NORMAL;
    [[vk::location(3)]] float2 uv        : TEXCOORD0;
    [[vk::location(4)]] float3 tangent   : TANGENT;
    [[vk::location(5)]] float3 bitangent : BITANGENT;
    [[vk::location(6)]] float3x3 TBN     : TBN_MATRIX;
};

// Выходные цветовые вложения (GBuffer)
struct FragmentOutput
{
    [[vk::location(0)]] float4 color    : SV_Target0;
    [[vk::location(1)]] float4 normal   : SV_Target1;
    [[vk::location(2)]] float4 emissive : SV_Target2;
};

// ============================================================================
// Дескрипторы, буферы и Push Constants
// ============================================================================

// Push-константы
struct PushConstants
{
    uint mat_index;
    uint obj_index;
};
[[vk::push_constant]] PushConstants pc_push;

// Камера (Uniform Buffer: set = 0, binding = 0)
struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_inverse;
    float4x4 proj_inverse;
    float4   position;
};
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> u_camera;

// Матрицы трансформаций объектов (set = 1, binding = 0)
struct ObjectTransforms
{
    float4x4 model;
    float4x4 normals;
};
[[vk::binding(0, 1)]] StructuredBuffer<ObjectTransforms> s_objects;

// PBR параметры материала (set = 2, binding = 1)
struct MaterialSettings
{
    float4 color;
    float  roughness;
    float  metallic;
    float  ao;
    float  emission;
};
[[vk::binding(1, 2)]] StructuredBuffer<MaterialSettings> s_materials;

// Параметры источника света (set = 4, binding = 0)
struct LightSource
{
    float4   position;
    float4   direction;
    float4   color;
    float4x4 space;
    float    quadratic;
    float    radius;
    float    intensity;
};
[[vk::binding(0, 4)]] StructuredBuffer<LightSource> s_lights;

// Индексы активных источников света (set = 4, binding = 1)
struct LightIndices
{
    uint count;
    uint active_indices[MAX_LIGHTS];
};
[[vk::binding(1, 4)]] ConstantBuffer<LightIndices> s_light_indices;

// Текстуры PBR материала (set = 3)
[[vk::binding(0, 3)]] Sampler2D t_albedo[MAX_MATERIALS];
[[vk::binding(1, 3)]] Sampler2D t_normal[MAX_MATERIALS];
[[vk::binding(2, 3)]] Sampler2D t_roughness[MAX_MATERIALS];
[[vk::binding(3, 3)]] Sampler2D t_height[MAX_MATERIALS];
[[vk::binding(4, 3)]] Sampler2D t_metallic[MAX_MATERIALS];
[[vk::binding(5, 3)]] Sampler2D t_ao[MAX_MATERIALS];

// ============================================================================
// ВЕРШИННЫЙ ШЕЙДЕР
// ============================================================================
[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;

    float4x4 model = s_objects[pc_push.obj_index].model;
    float3x3 normal_mat = (float3x3)s_objects[pc_push.obj_index].normals;

    float4 world_pos = mul(model, float4(input.position, 1.0));

    output.uv = input.uv;
    output.color = input.color.rgb;
    output.normal = mul(normal_mat, input.normal);
    output.position = world_pos.xyz;
    output.clipPosition = mul(u_camera.proj, mul(u_camera.view, world_pos));

    return output;
}

// ============================================================================
// ГЕОМЕТРИЧЕСКИЙ ШЕЙДЕР
// ============================================================================
[maxvertexcount(3)]
[shader("geometry")]
void geometryMain(
    triangle VertexOutput input[3],
    inout TriangleStream<GeometryOutput> outStream)
{
    float3 edge1 = input[1].position - input[0].position;
    float3 edge2 = input[2].position - input[0].position;
    float2 deltaUV1 = input[1].uv - input[0].uv;
    float2 deltaUV2 = input[2].uv - input[0].uv;

    float f = 1.0 / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    float3 tangent = normalize(float3(
        f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
        f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
        f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
    ));

    float3 bitangent = normalize(float3(
        f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
        f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
        f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z)
    ));

    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        GeometryOutput output;

        // Ортогонализация Грама-Шмидта
        float3 T = normalize(tangent);
        float3 N = normalize(input[i].normal);
        T = normalize(T - dot(T, N) * N);
        float3 B = normalize(cross(N, T));

        output.color = input[i].color;
        output.position = input[i].position;
        output.normal = input[i].normal;
        output.uv = input[i].uv;
        output.tangent = T;
        output.bitangent = B;
        output.TBN = float3x3(T, B, N);
        output.clipPosition = input[i].clipPosition;

        outStream.Append(output);
    }

    outStream.RestartStrip();
}

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
    MaterialSettings material = s_materials[pc_push.mat_index];
    float  alpha     = material.color.a * tex_albedo.a;
    float3 albedo    = material.color.rgb * tex_albedo.rgb;
    float  roughness = clamp(material.roughness * tex_roughness, 0.0, 1.0);
    float  metallic  = clamp(material.metallic * tex_metallic, 0.0, 1.0);
    float3 emission  = material.emission * albedo;

    // Преобразование нормали из Tangent Space в World Space
    float3 normal = normalize(tex_normal * 2.0 - 1.0);
    normal = normalize(mul(input.TBN, normal));

    // Суммарная освещенность точки
    float3 Lo = float3(0.0);

    // Цикл по активным источникам света
    for (uint i = 0; i < s_light_indices.count; ++i)
    {
        uint idx = s_light_indices.active_indices[i];
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

    // Буфер свечения (яркость выше порога)
    float3 bright_pbr = max(Lo - float3(BRIGHT_THRESHOLD), float3(0.0));

    // Вывод во вложения GBuffer
    output.color    = float4(Lo, alpha);
    output.normal   = float4(normal * 0.5 + 0.5, 1.0);
    output.emissive = float4(emission + bright_pbr, 1.0);

    return output;
}