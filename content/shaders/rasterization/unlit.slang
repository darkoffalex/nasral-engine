// Константы
static const uint MAX_MATERIALS = 100;

// ============================================================================
// Входные и промежуточные структуры данных
// ============================================================================

// Входные атрибуты из буфера вершин
struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
    [[vk::location(3)]] float4 color    : COLOR;
};

// Данные от вершинного к фрагментному шейдеру
struct VertexOutput
{
    float4 clipPosition : SV_Position;          // gl_Position
    [[vk::location(0)]] float3 color    : COLOR0;
    [[vk::location(1)]] float3 position : POSITION;
    [[vk::location(2)]] float3 normal   : NORMAL;
    [[vk::location(3)]] float2 uv       : TEXCOORD0;
};

// Выходные цветовые вложения (GBuffer)
struct FragmentOutput
{
    [[vk::location(0)]] float4 color    : SV_Target0;
    [[vk::location(1)]] float4 normal   : SV_Target1;
    [[vk::location(2)]] float4 emissive : SV_Target2;
};

// ============================================================================
// Дескрипторы, буферы и текстуры
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

// Параметры трансформаций объектов (Storage Buffer: set = 1, binding = 0)
struct ObjectTransforms
{
    float4x4 model;
    float4x4 normals;
};
[[vk::binding(0, 1)]] StructuredBuffer<ObjectTransforms> s_objects;

// Массив текстур альбедо объектов (set = 3, binding = 0)
[[vk::binding(0, 3)]] Sampler2D t_color[MAX_MATERIALS];

// ============================================================================
// Вершинный шейдер
// ============================================================================
[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;

    // Матрицы текущего объекта
    float4x4 model = s_objects[pc_push.obj_index].model;
    float3x3 normal_mat = (float3x3)s_objects[pc_push.obj_index].normals;

    // Мировая позиция
    float4 world_pos = mul(model, float4(input.position, 1.0));

    // Выходные данные
    output.uv = input.uv;
    output.color = input.color.rgb;
    output.normal = mul(normal_mat, input.normal);
    output.position = world_pos.xyz;
    output.clipPosition = mul(u_camera.proj, mul(u_camera.view, world_pos));

    return output;
}

// ============================================================================
// Фрагментный шейдер
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    FragmentOutput output;

    // Выборка цвета из массива текстур по индексу материала
    float4 tex_color = t_color[pc_push.mat_index].Sample(input.uv);

    output.color = float4(tex_color.rgb, 1.0);
    output.emissive = float4(0.0, 0.0, 0.0, 1.0);
    output.normal = float4(input.normal * 0.5 + 0.5, 1.0);

    return output;
}