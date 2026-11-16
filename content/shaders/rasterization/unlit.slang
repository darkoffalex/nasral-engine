#include "_common.slang"

// Массив текстур альбедо объектов (set = 3, binding = 0)
[[vk::binding(0, 3)]] Sampler2D t_color[MAX_MATERIALS];

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(GeometryOutput input)
{
    FragmentOutput output;

    // Выборка цвета из массива текстур по индексу материала
    float4 tex_color = t_color[pc_push.mat_index].Sample(input.uv);

    output.geometry = float4(tex_color.rgb, 1.0);
    output.attenuation = float4(1.0, 1.0, 1.0, 1.0);
    output.normal = float4(input.normal * 0.5 + 0.5, 1.0);

    return output;
}