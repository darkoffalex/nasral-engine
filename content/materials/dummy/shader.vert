#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec4 color;

layout (location = 0) out VS_OUT {
    vec3 color;
} vs_out;

void main()
{
    gl_Position = vec4(position, 1.0);
    vs_out.color = color.rgb;
}