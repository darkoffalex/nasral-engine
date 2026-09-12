#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec3 color;
    vec3 position;
    vec3 normal;
} fs_in;

// Выход фрагмента (цветовое вложение 0)
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outEmissive;

void main()
{
    color = vec4(fs_in.color, 1.0);
    outEmissive = vec4(0.0, 0.0, 0.0, 1.0);
    outNormal = vec4(fs_in.normal * 0.5 + 0.5, 1.0);
}