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

// Главная функция шейдера
void main()
{
    if(pc_push.pass_index == 0)
    {
        vec3 c = texture(frame_emission, fs_in.uv).rgb;
        c.b = 0.0;
        frame_out = vec4(c, 1.0);
    }
    else
    {
        vec3 c = texture(frame_ping, fs_in.uv).rgb;
        c.g = 0.0;
        frame_out = vec4(c, 1.0);
    }

}