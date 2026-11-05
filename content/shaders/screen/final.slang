// Константы
static const uint MAX_SCREEN_PASSES = 5;

// ============================================================================
// Входные и промежуточные структуры данных
// ============================================================================

// Выход вершинного шейдера -> Вход геометрического шейдера
struct VertexOutput
{
    float4 clipPosition : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

// Выход фрагментного шейдера
struct FragmentOutput
{
    [[vk::location(0)]] float4 color : SV_Target0;
};

// ============================================================================
// Описание структур данных UBO / SSBO / Push Constants
// ============================================================================

// Push-константы
struct PushConstants
{
    uint pass_index;
    uint pass_stride;
};
[[vk::push_constant]] PushConstants pc_push;

// Параметры камеры (set = 1, binding = 0)
struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_inverse;
    float4x4 proj_inverse;
    float4 position;
};
[[vk::binding(0, 1)]] ConstantBuffer<CameraData> u_camera;

// Параметры экранного прохода (set = 2, binding = 0)
struct ScreenPassSettings
{
    float bloom_blur_lod;
    float bloom_intensity;
    float gamma;
    float exposure;
};
[[vk::binding(0, 2)]] ByteAddressBuffer s_screen_passes;

// Текстуры кадрового буфера (set = 0, bindings = [0 - 11])
[[vk::binding(0, 0)]] Sampler2D t_color;
[[vk::binding(1, 0)]] Sampler2D t_geometry;
[[vk::binding(3, 0)]] Sampler2D t_normal;
[[vk::binding(4, 0)]] Sampler2D t_depth;
[[vk::binding(5, 0)]] Sampler2D t_emission;
[[vk::binding(6, 0)]] Sampler2D t_shadows;
[[vk::binding(7, 0)]] Sampler2D t_ao;
[[vk::binding(10, 0)]] Sampler2D t_temp1;
[[vk::binding(11, 0)]] Sampler2D t_temp2;

// Описываем 2 треугольника (для рисования полноэкранного квадрата)
static const float2 positions[6] =
{
    float2(-1.0, -1.0),
    float2(-1.0,  1.0),
    float2( 1.0,  1.0),
    float2( 1.0,  1.0),
    float2( 1.0, -1.0),
    float2(-1.0, -1.0)
};

static const float2 uvs[6] =
{
    float2(0.0, 1.0),
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(1.0, 0.0),
    float2(1.0, 1.0),
    float2(0.0, 1.0)
};

// ============================================================================
// ВЕРШИННЫЙ ШЕЙДЕР
// ============================================================================
[shader("vertex")]
VertexOutput vertexMain(uint vertex_index : SV_VertexID)
{
    VertexOutput output;
    output.clipPosition = float4(positions[vertex_index], 0.0, 1.0);
    output.uv = uvs[vertex_index];
    return output;
}

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    // Получить настройки экранного прохода
    ScreenPassSettings compositing = s_screen_passes.Load<ScreenPassSettings>(pc_push.pass_index * pc_push.pass_stride);

    // Получить цвет и bloom
    float4 col = t_color.Sample(input.uv);
    float3 bloom = t_temp2.Sample(input.uv).rgb;

    // Вывод во вложение
    FragmentOutput output;
    output.color = float4(col.rgb + bloom, col.a);
    return output;
}