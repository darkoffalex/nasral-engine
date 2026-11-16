#include "_common.slang"

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(GeometryOutput input)
{
    FragmentOutput output;

    output.geometry = float4(input.color, 1.0);
    output.attenuation = float4(1.0, 1.0, 1.0, 1.0);
    output.normal = float4(input.normal * 0.5 + 0.5, 1.0);

    return output;
}