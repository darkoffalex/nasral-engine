#include "_common.slang"
#include "_blur.slang"

// Выходное цветовое вложение
struct FragmentOutput
{
    [[vk::location(0)]] float4 out0 : SV_Target0;
    [[vk::location(1)]] float4 out1 : SV_Target1;
};

// Параметры экранного прохода (set = 2, binding = 0)
struct ScreenPassSettings
{
    uint pass_index;
    uint samples;
    float base_tex_radius;
    float base_kernel_radius;
    float2 direction;
};

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    // Получить настройки экранного прохода
    ScreenPassSettings settings = s_screen_passes.Load<ScreenPassSettings>(pc_push.pass_index * pc_push.pass_stride);

    // Размеры текстуры
    uint width;
    uint height;

    // Выход шейдера
    FragmentOutput output;

    // Размытие
    float3 blurred_ao;
    float3 blurred_sh;

    if (settings.pass_index == 0)
    {
        // Размер текселя для AO (временный выход)
        t_temp1.GetDimensions(width, height);
        float2 texel_size_ao = 1.0 / float2(width, height);

        // Размер текселя для теней (временный выход)
        t_temp2.GetDimensions(width, height);
        float2 texel_size_sh = 1.0 / float2(width, height);

        // Размытое AO
        blurred_ao = linear_bilateral_blur(
                t_ao,
                t_depth,
                t_normal,
                u_camera.proj,
                input.uv,
                settings.direction,
                texel_size_ao,
                settings.samples,
                settings.base_kernel_radius,
                settings.base_tex_radius,
                0.5,
                true);

        // Размытая тень
        blurred_sh = linear_bilateral_blur(
                t_attenuation,
                t_depth,
                t_normal,
                u_camera.proj,
                input.uv,
                settings.direction,
                texel_size_sh,
                settings.samples,
                settings.base_kernel_radius,
                settings.base_tex_radius,
                0.5,
                true);

        // Вывод во временные вложения
        output.out0 = float4(blurred_ao, 1.0);
        output.out1 = float4(blurred_sh, 1.0);
    }
    else
    {
        // Размер текселя для AO (итоговый выход)
        t_ao.GetDimensions(width, height);
        float2 texel_size_ao = 1.0 / float2(width, height);

        // Размер текселя для теней (итоговый выход)
        t_shadows.GetDimensions(width, height);
        float2 texel_size_sh = 1.0 / float2(width, height);

        // Размытое AO
        blurred_ao = linear_bilateral_blur(
                t_temp1,
                t_depth,
                t_normal,
                u_camera.proj,
                input.uv,
                settings.direction,
                texel_size_ao,
                settings.samples,
                settings.base_kernel_radius,
                settings.base_tex_radius,
                0.5,
                true);

        // Размытая тень
        blurred_sh = linear_bilateral_blur(
                t_temp2,
                t_depth,
                t_normal,
                u_camera.proj,
                input.uv,
                settings.direction,
                texel_size_sh,
                settings.samples,
                settings.base_kernel_radius,
                settings.base_tex_radius,
                0.5,
                true);

        // Вывод во временные вложения
        output.out0 = float4(blurred_ao, 1.0);
        output.out1 = float4(blurred_sh, 1.0);
    }

    // Вывод во вложения
    return output;
}
