#include "_common.slang"
#include "_blur.slang"

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
    float3 bloom = t_emission.Sample(input.uv).rgb;
    //float3 ao = t_ao.Sample(input.uv).rgb;

    uint width;
    uint height;
    t_ao.GetDimensions(width, height);
    float2 texel_size = 1.0 / float2(width, height);
    float3 ao = box_gaussian_blur(t_ao,input.uv,texel_size,1.0);

    // Вывод во вложение
    FragmentOutput output;
    output.color = float4((col.rgb * ao.rgb) + bloom, col.a);
    return output;
}