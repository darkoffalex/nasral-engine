#pragma once

// Константы
static const uint MAX_OBJECTS = 1024;
static const uint MAX_MATERIALS = 128;
static const uint MAX_LIGHTS = 64;

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
    uint mat_stride;
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

// Параметры материала (set = 2, binding = 0)
[[vk::binding(0, 2)]] ByteAddressBuffer s_materials;

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
[[vk::binding(1, 4)]] StructuredBuffer<LightIndices> s_light_indices;

// Структура ускорения для ray tracing / ray query (set = 5, binding = 0)
[[vk::binding(0, 5)]] RaytracingAccelerationStructure u_tlas;

// ============================================================================
// ОБЩИЙ ВЕРШИННЫЙ ШЕЙДЕР
// ============================================================================
[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;

    // Матрицы модели и нормалей для текущего объекта
    float4x4 model = s_objects[pc_push.obj_index].model;
    float3x3 normal_mat = (float3x3)s_objects[pc_push.obj_index].normals;

    // Позиция в мировом пространстве
    float4 world_pos = mul(model, float4(input.position, 1.0));

    // Заполнение выходов
    output.uv = input.uv;
    output.color = input.color.rgb;
    output.normal = mul(normal_mat, input.normal);
    output.position = world_pos.xyz;
    output.clipPosition = mul(u_camera.proj, mul(u_camera.view, world_pos));

    return output;
}

// ============================================================================
// ОБЩИЙ ГЕОМЕТРИЧЕСКИЙ ШЕЙДЕР
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
        output.TBN = transpose(float3x3(T, B, N));
        output.clipPosition = input[i].clipPosition;

        outStream.Append(output);
    }

    outStream.RestartStrip();
}
