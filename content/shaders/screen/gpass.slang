#include "_common.slang"

// Константы
static const float BRIGHT_THRESHOLD = 0.5;

// Выходные цветовые вложения
struct FragmentOutput
{
    [[vk::location(0)]] float4 color    : SV_Target0;
    [[vk::location(1)]] float4 emission : SV_Target1;
};

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    // Получить цвет геометрии (без затухания) и общее затухание в точке
    float4 geometry    = t_geometry.SampleLevel(input.uv, 0.0);
    float3 attenuation = t_attenuation.SampleLevel(input.uv, 0.0).rgb;

    // Итоговый цвет в HDR диапазоне
    float3 hdr_color = geometry.rgb * attenuation;

    // Наиболее яркие области (яркость выше порога)
    float3 bright_color = max(hdr_color - float3(BRIGHT_THRESHOLD), float3(0.0));

    // Вывод во вложения (eColor и eEmission)
    FragmentOutput output;
    output.color    = float4(hdr_color, geometry.a);
    output.emission = float4(bright_color, 1.0);
    return output;
}
