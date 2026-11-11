#pragma once

// Внимание! Должны совпадать с FrameAttachmentType из gfx::types
#define ATTACHMENT_COLOR     0
#define ATTACHMENT_GEOMETRY  1
#define ATTACHMENT_MATERIAL  2
#define ATTACHMENT_NORMAL    3
#define ATTACHMENT_DEPTH     4
#define ATTACHMENT_EMISSION  5
#define ATTACHMENT_SHADOWS   6
#define ATTACHMENT_AO        7
#define ATTACHMENT_TEMP1     10
#define ATTACHMENT_TEMP2     11

// Битовые маски стадий пост-обработки (должны совпадать с порядком проходов в project.json)
#define STAGE_AO            (1u << 0)
#define STAGE_AO_BLUR_H     (1u << 1)
#define STAGE_AO_BLUR_V     (1u << 2)
#define STAGE_AO_BLUR       (STAGE_AO_BLUR_H | STAGE_AO_BLUR_V)
#define STAGE_BLOOM_BLUR_H  (1u << 3)
#define STAGE_BLOOM_BLUR_V  (1u << 4)
#define STAGE_BLOOM_BLUR    (STAGE_BLOOM_BLUR_H | STAGE_BLOOM_BLUR_V)
#define STAGE_COMPOSITING   (1u << 5)

// Выход вершинного шейдера -> Вход фрагментного шейдера
struct VertexOutput
{
    float4 clipPosition : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

// Выходные цветовые вложения
struct FragmentOutput
{
    [[vk::location(0)]] float4 color : SV_Target0;
};

// ============================================================================
// Дескрипторы, буферы и Push Constants
// ============================================================================

// Push-константы
struct PushConstants
{
    uint pass_index;
    uint pass_stride;
    uint active_stage_flags;
};
[[vk::push_constant]] PushConstants pc_push;

// Экранные буферы (set = 0)
[[vk::binding(ATTACHMENT_COLOR, 0)]] Sampler2D t_color;
[[vk::binding(ATTACHMENT_GEOMETRY, 0)]] Sampler2D t_geometry;
[[vk::binding(ATTACHMENT_MATERIAL, 0)]] Sampler2D t_material;
[[vk::binding(ATTACHMENT_NORMAL, 0)]] Sampler2D t_normal;
[[vk::binding(ATTACHMENT_DEPTH, 0)]] Sampler2D t_depth;
[[vk::binding(ATTACHMENT_EMISSION, 0)]] Sampler2D t_emission;
[[vk::binding(ATTACHMENT_SHADOWS, 0)]] Sampler2D t_shadows;
[[vk::binding(ATTACHMENT_AO, 0)]] Sampler2D t_ao;
[[vk::binding(ATTACHMENT_TEMP1, 0)]] Sampler2D t_temp1;
[[vk::binding(ATTACHMENT_TEMP2, 0)]] Sampler2D t_temp2;

// Камера (Uniform Buffer: set = 1, binding = 0)
struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_inverse;
    float4x4 proj_inverse;
    float4 position;
};
[[vk::binding(0, 1)]] ConstantBuffer<CameraData> u_camera;

// Параметры экранного похода (set = 2, binding = 0)
[[vk::binding(0, 2)]] ByteAddressBuffer s_screen_passes;

// Положения вершин треугольника
static const float2 positions[6] =
{
    float2(-1.0, -1.0),
    float2(-1.0,  1.0),
    float2( 1.0,  1.0),
    float2( 1.0,  1.0),
    float2( 1.0, -1.0),
    float2(-1.0, -1.0)
};

// UV-координаты вершин треугольника
static const float2 uvs[6] =
{
    float2(0.0, 1.0),
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(1.0, 0.0),
    float2(1.0, 1.0),
    float2(0.0, 1.0)
};

// ============================================================================
// ОБЩИЕ УТИЛИТЫ И ВЕРШИННЫЙ ШЕЙДЕР
// ============================================================================

/**
 * @brief Проверяет активность стадий пост-обработки по переданной битовой маске.
 * @param[in] stage_mask Битовая маска проверяемых стадий (STAGE_*).
 * @return true, если все стадии из маски активны, иначе false.
 */
[ForceInline]
bool is_stages_active(uint stage_mask)
{
    return (pc_push.active_stage_flags & stage_mask) == stage_mask;
}

[shader("vertex")]
VertexOutput vertexMain(uint vertex_index : SV_VertexID)
{
    VertexOutput output;
    output.clipPosition = float4(positions[vertex_index], 0.0, 1.0);
    output.uv = uvs[vertex_index];
    return output;
}