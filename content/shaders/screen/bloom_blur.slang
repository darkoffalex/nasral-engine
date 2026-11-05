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
    uint pass_index;
    uint samples;
    float base_tex_radius;
    float base_kernel_radius;
    float2 direction;
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

[ForceInline]
float3 linear_blur(Sampler2D tex, float2 uv, float2 dir, float2 texel_size, uint samples)
{
    // 1. Рассчитываем кол-во выборок с двух сторон от центра (радиус выборки)
    int div = samples / 2;
    int from = -div;
    int to = div;

    // 2. Аккумулировать результат
    float3 result = float3(0.0);
    for(int i = from; i <= to; ++i){
        float2 offset = float(i) * dir * texel_size;
        result += tex.Sample(uv + offset).rgb;
    }

    // 3. Усреднение (деление на вес, вес каждой выборки = 1)
    return result / float(samples);
}

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    // Получить настройки экранного прохода
    ScreenPassSettings settings = s_screen_passes.Load<ScreenPassSettings>(pc_push.pass_index * pc_push.pass_stride);

    // Получить нужную текстуру для обработки в зависимости от индекса прохода
    Sampler2D t = settings.pass_index == 0 ? t_emission : t_temp1;

    // Размер пикселя текстуры
    uint width;
    uint height;
    t.GetDimensions(width, height);
    float2 texel_size = 1.0 / float2(width, height);

    // Размытие
    float3 blurred;
    if (settings.pass_index == 0)
    {
        t_emission.GetDimensions(width, height);
        float2 texel_size = 1.0 / float2(width, height);
        blurred = linear_blur(t_emission, input.uv, settings.direction, texel_size, settings.samples);
    }
    else
    {
        t_temp1.GetDimensions(width, height);
        float2 texel_size = 1.0 / float2(width, height);
        blurred = linear_blur(t_temp1, input.uv, settings.direction, texel_size, settings.samples);
    }

    // Вывод во вложение
    FragmentOutput output;
    output.color = float4(blurred, 1.0);
    return output;
}