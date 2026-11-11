#include "_common.slang"
#include "_blur.slang"

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

    // Размытие
    float3 blurred;
    if (settings.pass_index == 0)
    {
        t_emission.GetDimensions(width, height);
        float2 texel_size = 1.0 / float2(width, height);

        blurred = linear_blur(
                t_emission,
                input.uv,
                settings.direction,
                texel_size,
                settings.samples);

        //blurred = linear_depth_aware_blur(
        //        t_emission,
        //        t_depth,
        //        u_camera.proj,
        //        input.uv,
        //        settings.direction,
        //        texel_size,
        //        settings.samples,
        //        settings.base_tex_radius);

        //blurred = linear_bilateral_blur(
        //        t_emission,
        //        t_depth,
        //        t_normal,
        //        u_camera.proj,
        //        input.uv,
        //        settings.direction,
        //        texel_size,
        //        settings.samples,
        //        settings.base_kernel_radius,
        //        settings.base_tex_radius,
        //        0.5,
        //        true);
    }
    else
    {
        t_temp1.GetDimensions(width, height);
        float2 texel_size = 1.0 / float2(width, height);

        blurred = linear_blur(
                t_temp1,
                input.uv,
                settings.direction,
                texel_size,
                settings.samples);

        //blurred = linear_depth_aware_blur(
        //        t_temp1,
        //        t_depth,
        //        u_camera.proj,
        //        input.uv,
        //        settings.direction,
        //        texel_size,
        //        settings.samples,
        //        settings.base_tex_radius);

        //blurred = linear_bilateral_blur(
        //        t_temp1,
        //        t_depth,
        //        t_normal,
        //        u_camera.proj,
        //        input.uv,
        //        settings.direction,
        //        texel_size,
        //        settings.samples,
        //        settings.base_kernel_radius,
        //        settings.base_tex_radius,
        //        0.5,
        //        true);
    }

    // Вывод во вложение
    FragmentOutput output;
    output.color = float4(blurred, 1.0);
    return output;
}