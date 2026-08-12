#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec2 uv;
} fs_in;

// Выход фрагмента (цветовое вложение 0)
layout (location = 0) out vec4 color;

// Текстуры кадрового буфера
layout(set = 0, binding = 0) uniform sampler2D frame_color;
layout(set = 0, binding = 1) uniform sampler2D frame_depth;
layout(set = 0, binding = 2) uniform sampler2D frame_normals;
layout(set = 0, binding = 3) uniform sampler2D frame_emission;

void main()
{
    vec4 tex_color = texture(frame_color, fs_in.uv);
    color = vec4(tex_color.rgb, 1.0);
}
