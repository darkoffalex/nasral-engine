#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec2 uv;
} fs_in;

// Выходы фрагментов (вложения 0 и 1, ping pong) для двух-проходных эффектов
layout (location = 0) out vec4 out_ping;
layout (location = 1) out vec4 out_pong;

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
    // В зависимоти от прохода читаем разные текстуры
    vec3 emission = pc_push.pass_index == 0 ? texture(frame_emission, fs_in.uv).rgb : texture(frame_ping, fs_in.uv).rgb;

    // В зависимости от прохода - обрабатываем по разному и пишем в разные вложения
    if(pc_push.pass_index == 0) {
        emission.b = 0.0;
        out_ping = vec4(emission, 1.0);
    }
    else {
        emission.g = 0.0;
        out_pong = vec4(emission, 1.0);
    }
}