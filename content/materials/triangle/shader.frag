#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec4 color;

layout (location = 0) in VS_OUT {
    vec3 color;
} fs_in;

void main()
{
    color = vec4(fs_in.color, 1.0);
}