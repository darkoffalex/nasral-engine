#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные вершины
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;

// Выходные данные (в следующие этапы)
layout (location = 0) out VS_OUT {
    vec3 color;
} vs_out;

void main()
{
    // Выход
    gl_Position = vec4(in_position, 1.0);
    vs_out.color = in_color.rgb;
}