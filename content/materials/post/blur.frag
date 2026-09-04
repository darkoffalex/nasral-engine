#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec2 uv;
} fs_in;

// Выходы фрагментов (влоежения кадрового буфера)
layout (location = 0) out vec4 frame_out;

// Push constants
layout(push_constant) uniform PushConstants {
    uint pass_index;
} pc_push;

// Текстуры кадрового буфера (в HDR формате, R16G16B16A16_SFLOAT)
layout(set = 0, binding = 0) uniform sampler2D frame_color;
layout(set = 0, binding = 1) uniform sampler2D frame_depth;
layout(set = 0, binding = 2) uniform sampler2D frame_normals;
layout(set = 0, binding = 3) uniform sampler2D frame_emission;

// Текстуры промежуточных данных (ping pong) для двух-проходных эффектов (bilateral blur и прочие)
layout(set = 0, binding = 4) uniform sampler2D frame_ping;
layout(set = 0, binding = 5) uniform sampler2D frame_pong;

// Uniform buffer для матриц камеры
layout(set = 1, binding = 0, std140) uniform UCamera {
    mat4 view;
    mat4 proj;
    mat4 view_inverse;
    mat4 proj_inverse;
    vec4 position;
} u_camera;

// Линейное размытие (в одном направлении)
vec3 linear_blur(sampler2D tex, vec2 uv, vec2 dir, vec2 texel_size, int samples)
{
    int div = samples / 2;
    int from = -div;
    int to = div;

    vec3 result = vec3(0.0);
    for(int i = from; i <= to; ++i){
        vec2 offset = float(i) * dir * texel_size;
        result += texture(tex, uv + offset).rgb;
    }

    return result / float(samples);
}

// Главная функция шейдера
void main()
{
    if(pc_push.pass_index == 0)
    {
        vec2 texel_size = 1.0 / vec2(textureSize(frame_emission, 0));
        vec3 b = linear_blur(frame_emission, fs_in.uv, vec2(1.0, 0.0), texel_size, 11);
        frame_out = vec4(b, 1.0);
    }
    else
    {
        vec2 texel_size = 1.0 / vec2(textureSize(frame_ping, 0));
        vec3 b = linear_blur(frame_ping, fs_in.uv, vec2(0.0, 1.0), texel_size, 11);
        frame_out = vec4(b, 1.0);
    }
}