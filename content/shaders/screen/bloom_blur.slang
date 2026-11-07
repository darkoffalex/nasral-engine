#include "_common.slang"

// Параметры экранного прохода (set = 2, binding = 0)
struct ScreenPassSettings
{
    uint pass_index;
    uint samples;
    float base_tex_radius;
    float base_kernel_radius;
    float2 direction;
};

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