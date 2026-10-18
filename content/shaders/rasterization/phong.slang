// Константы
static const uint MAX_OBJECTS = 1000;
static const uint MAX_MATERIALS = 100;
static const uint MAX_LIGHTS = 100;

// ============================================================================
// Входные и промежуточные структуры данных
// ============================================================================

// Входные вершинные атрибуты из буфера вершин
struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
    [[vk::location(3)]] float4 color    : COLOR;
};

// Выход вершинного шейдера -> Вход геометрического шейдера (бывший VS_OUT)
struct VertexOutput
{
    float4 clipPosition : SV_Position;
    [[vk::location(0)]] float3 color    : COLOR0;
    [[vk::location(1)]] float3 position : POSITION;
    [[vk::location(2)]] float3 normal   : NORMAL;
    [[vk::location(3)]] float2 uv       : TEXCOORD0;
};

// Выход геометрического шейдера -> Вход фрагментного шейдера (бывший GS_OUT)
// Обратите внимание: матрица 3x3 занимает 3 последовательных location (6, 7, 8)
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

// Выход фрагментного шейдера (GBuffer)
struct FragmentOutput
{
    [[vk::location(0)]] float4 color    : SV_Target0;
    [[vk::location(1)]] float4 normal   : SV_Target1;
    [[vk::location(2)]] float4 emissive : SV_Target2;
};

// ============================================================================
// Описание структур данных UBO / SSBO / Push Constants
// ============================================================================

// Push-константы
struct PushConstants
{
    uint mat_index;
    uint obj_index;
};
[[vk::push_constant]] PushConstants pc_push;

// Параметры камеры (set = 0, binding = 0)
struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_inverse;
    float4x4 proj_inverse;
    float4   position;
};
[[vk::binding(0, 0)]] ConstantBuffer<CameraData> u_camera;

// Матрицы объектов (set = 1, binding = 0)
struct ObjectTransforms
{
    float4x4 model;
    float4x4 normals;
};
[[vk::binding(0, 1)]] StructuredBuffer<ObjectTransforms> s_objects;

// Параметры материала (set = 2, binding = 0)
struct MaterialSettings
{
    float4 color;
    float4 ambient;
    float  shininess;
    float  specular;
};
[[vk::binding(0, 2)]] StructuredBuffer<MaterialSettings> s_materials;

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

// Массивы текстур с комбинированными сэмплерами (set = 3)
[[vk::binding(0, 3)]] Sampler2D t_color[MAX_MATERIALS];
[[vk::binding(1, 3)]] Sampler2D t_normal[MAX_MATERIALS];
[[vk::binding(2, 3)]] Sampler2D t_spec[MAX_MATERIALS];

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
    // Вычисление базиса касательного пространства (tangent и bitangent)
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

    // Генерация вершин треугольника
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        GeometryOutput output;

        // Ортогонализация Грама-Шмидта для TBN
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

        // В Slang конструктор float3x3 принимает строки или столбцы;
        // В GLSL было mat3(T, B, N) со столбцами T, B, N:
        output.TBN = float3x3(T, B, N);

        output.clipPosition = input[i].clipPosition;

        // Эквивалент EmitVertex() в Slang:
        outStream.Append(output);
    }

    // Эквивалент EndPrimitive() в Slang:
    outStream.RestartStrip();
}

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
    MaterialSettings material = s_materials[pc_push.mat_index];

    // Нормаль из tangent space в world space
    float3 normal = normalize(tex_normal * 2.0 - 1.0);
    normal = normalize(mul(input.TBN, normal));

    float3 final_color = float3(0.0);
    float3 ambient = material.ambient.rgb;

    // Цикл по активным источникам света
    for (uint i = 0; i < s_light_indices.count; ++i)
    {
        uint idx = s_light_indices.active_indices[i];
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