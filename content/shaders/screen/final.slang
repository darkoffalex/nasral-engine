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

// Функция ACES Tone Mapping
float3 tone_map_aces(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    // Получить настройки экранного прохода
    ScreenPassSettings compositing = s_screen_passes.Load<ScreenPassSettings>(pc_push.pass_index * pc_push.pass_stride);

    // Получить основной цвет сцены (HDR)
    float4 col = t_color.Sample(input.uv);

    // Получить bloom (если стадии размытия bloom задействованы)
    float3 bloom = float3(0.0);
    if (is_stages_active(STAGE_BLOOM_BLUR))
    {
        bloom = t_emission.Sample(input.uv).rgb * compositing.bloom_intensity;
    }

    // Получить AO (если стадия AO задействована)
    float3 ao = float3(1.0);
    if (is_stages_active(STAGE_AO))
    {
        uint width;
        uint height;
        t_ao.GetDimensions(width, height);
        float2 texel_size = 1.0 / float2(width, height);
        ao = box_gaussian_blur(t_ao, input.uv, texel_size, 1.0);
    }

    // Композиция в линейном HDR-пространстве с учетом экспозиции
    float3 hdr_color = ((col.rgb * ao) + bloom) * compositing.exposure;

    // Выполняем Tone Mapping (сжатие HDR -> SDR)
    float3 sdr_color = tone_map_aces(hdr_color);

    // Гамма коррекция
    float3 final_color = pow(sdr_color, float3(1.0 / compositing.gamma));

    // Вывод во вложение
    FragmentOutput output;
    output.color = float4(final_color, col.a);
    return output;
}