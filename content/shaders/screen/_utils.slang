#pragma once

// ============================================================================
// УТИЛИТЫ РАБОТЫ С НОРМАЛЯМИ
// ============================================================================

// Извлечь нормаль в мировом пространстве (World Space) из текстуры [0..1] -> [-1..1]
[ForceInline]
float3 fetch_world_normal(Sampler2D normal_tex, float2 uv)
{
    float3 n = normal_tex.Sample(uv).xyz * 2.0 - 1.0;
    return normalize(n);
}

// Извлечь нормаль в пространстве вида (View Space)
[ForceInline]
float3 fetch_view_normal(Sampler2D normal_tex, float2 uv, float4x4 view_matrix)
{
    float3 world_normal = normal_tex.Sample(uv).xyz * 2.0 - 1.0;
    // Отсекаем перенос (translation), приводя к float3x3
    float3 view_normal = mul((float3x3)view_matrix, world_normal);
    return normalize(view_normal);
}

// ============================================================================
// УТИЛИТЫ РАБОТЫ С ГЛУБИНОЙ И ПОЗИЦИЕЙ (Vulkan NDC Z: [0..1])
// ============================================================================

// Конвертировать нелинейную глубину (0..1) в линейную дистанцию (в метрах)
[ForceInline]
float linearize_depth(float ndc_depth, float4x4 proj_matrix)
{
    float proj_a = proj_matrix[2][2];
    float proj_b = proj_matrix[2][3]; // В GLSL было proj[3][2] (4-й столбец, 3-я строка)
    return abs(proj_b / (ndc_depth + proj_a));
}

// Вывод линейной глубины в качестве цвета (для отладки)
[ForceInline]
float3 debug_linear_depth(float d, float n, float f)
{
    float clamped_d = max(0, d - n);
    float clamped_f = max(f - n, 0.01);
    return clamped_d / clamped_f;
}

// Извлечь линейную глубину напрямую из текстуры глубины
[ForceInline]
float fetch_linear_depth(Sampler2D depth_tex, float2 uv, float4x4 proj_matrix)
{
    float depth_sample = depth_tex.Sample(uv).r;
    return linearize_depth(depth_sample, proj_matrix);
}

// Быстрое восстановление 3D-позиции во View Space через аналитический луч (без обратной матрицы)
[ForceInline]
float3 reconstruct_view_pos_fast(Sampler2D depth_tex, float2 uv, float4x4 proj_matrix)
{
    float depth_sample = depth_tex.Sample(uv).r;
    float linear_z = linearize_depth(depth_sample, proj_matrix);

    // Vulkan UV -> NDC [-1..1]
    float2 ndc_xy = float2(uv.x * 2.0 - 1.0, (1.0 - uv.y) * 2.0 - 1.0);
    float2 focal  = float2(proj_matrix[0][0], proj_matrix[1][1]);

    // В правосторонней системе координат (Right-Handed View Space) камера смотрит вдоль -Z
    float2 view_xy = (ndc_xy * linear_z) / focal;
    return float3(view_xy, -linear_z);
}

// Классическое извлечение 3D-позиции во View Space через обратную матрицу проекции
[ForceInline]
float3 fetch_view_pos(Sampler2D depth_tex, float2 uv, float4x4 proj_inverse_matrix)
{
    float depth_sample = depth_tex.Sample(uv).r;

    // Vulkan ZO: X в [-1..1], Y в [-1..1], Z в [0..1]
    float4 ndc = float4(
        uv.x * 2.0 - 1.0,
        (1.0 - uv.y) * 2.0 - 1.0,
        depth_sample,
        1.0
    );

    float4 view_pos = mul(proj_inverse_matrix, ndc);
    return view_pos.xyz / view_pos.w;
}

// Извлечение 3D-позиции в мировом пространстве (World Space)
[ForceInline]
float3 fetch_world_pos(Sampler2D depth_tex, float2 uv, float4x4 proj_inverse_matrix, float4x4 view_inverse_matrix)
{
    float3 view_pos = fetch_view_pos(depth_tex, uv, proj_inverse_matrix);
    return mul(view_inverse_matrix, float4(view_pos, 1.0)).xyz;
}