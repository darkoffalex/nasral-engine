#include "_common.slang"

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================
[shader("fragment")]
FragmentOutput fragmentMain(GeometryOutput input)
{
    FragmentOutput output;

    output.color = float4(input.color, 1.0);
    output.emissive = float4(0.0, 0.0, 0.0, 1.0);
    output.normal = float4(input.normal * 0.5 + 0.5, 1.0);

    return output;
}