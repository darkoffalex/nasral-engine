#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable

// Константы
#define MAX_OBJECTS 1000

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec3 color;
    vec2 uv;
} fs_in;

// Выход фрагмента (цветовое вложение 0)
layout (location = 0) out vec4 color;

// Push constants
layout(push_constant) uniform PushConstants {
    uint obj_index;
} pc_push;

// Текстуры объекта
layout(set = 2, binding = 0) uniform sampler2D t_color[MAX_OBJECTS];

void main()
{
    vec4 tex_color = texture(t_color[pc_push.obj_index], fs_in.uv);
    color = vec4(tex_color.rgb, 1.0);
}