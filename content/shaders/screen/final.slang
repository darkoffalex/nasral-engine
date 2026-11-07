#include "_common.slang"

// Параметры экранного прохода (set = 2, binding = 0)
struct ScreenPassSettings
{
    float bloom_blur_lod;
    float bloom_intensity;
    float gamma;
    float exposure;
};

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