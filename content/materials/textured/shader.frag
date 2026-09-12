#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable

// Константы
#define MAX_OBJECTS 1000
#define MAX_MATERIALS 100

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec3 color;
    vec3 position;
    vec3 normal;
    vec2 uv;
} fs_in;

// Выход фрагмента (цветовое вложение 0)
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outEmissive;

// Push constants
layout(push_constant) uniform PushConstants {
    uint mat_index;
    uint obj_index;
} pc_push;

// Текстуры объекта
layout(set = 3, binding = 0) uniform sampler2D t_color[MAX_MATERIALS];

void main()
{
    vec4 tex_color = texture(t_color[pc_push.mat_index], fs_in.uv);
    color = vec4(tex_color.rgb, 1.0);
    outEmissive = vec4(0.0, 0.0, 0.0, 1.0);
    outNormal = vec4(fs_in.normal * 0.5 + 0.5, 1.0);
}