#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec3 color;
} fs_in;

// Выход фрагмента (цветовое вложение 0)
layout (location = 0) out vec4 color;

void main()
{
    color = vec4(fs_in.color, 1.0);
}