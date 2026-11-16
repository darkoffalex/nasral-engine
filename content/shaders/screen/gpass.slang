#include "_common.slang"
#include "_blur.slang"

// Выходные цветовые вложения
struct FragmentOutput
{
    [[vk::location(0)]] float4 color    : SV_Target0;
    [[vk::location(1)]] float4 emission : SV_Target1;
};

// Параметры экранного прохода (set = 2, binding = 0)
// Структура соответствует gfx::uniforms::ScreenPassEffects в C++ коде движка
struct ScreenPassSettings
{
    float bright_threshold;
};

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    // Получить настройки экранного прохода
    ScreenPassSettings settings = s_screen_passes.Load<ScreenPassSettings>(pc_push.pass_index * pc_push.pass_stride);

    // Получить цвет геометрии (без затухания) и общее затухание/тень в точке
    float4 geometry    = t_geometry.SampleLevel(input.uv, 0.0);
    float3 attenuation = float3(1.0);

    if (is_stages_active(STAGE_AO_BLUR))
    {
        uint width;
        uint height;
        t_shadows.GetDimensions(width, height);
        float2 texel_size = 1.0 / float2(width, height);
        attenuation = box_blur(t_shadows, input.uv, texel_size, 1.0);

        //attenuation = t_shadows.SampleLevel(input.uv, 0.0).rgb;
    }
    else
    {
        attenuation = t_attenuation.SampleLevel(input.uv, 0.0).rgb;
    }

    // Итоговый цвет в HDR диапазоне
    float3 hdr_color = geometry.rgb * attenuation;

    // Наиболее яркие области (яркость выше порога)
    float3 bright_color = max(hdr_color - float3(settings.bright_threshold), float3(0.0));

    // Вывод во вложения (eColor и eEmission)
    FragmentOutput output;
    output.color    = float4(hdr_color, geometry.a);
    output.emission = float4(bright_color, 1.0);
    return output;
}
