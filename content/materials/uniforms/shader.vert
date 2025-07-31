#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec4 color;

layout (location = 0) out VS_OUT {
    vec3 color;
    vec2 uv;
} vs_out;

layout(set = 0, binding = 0, std140) uniform UniformCamera {
    mat4 view;
    mat4 proj;
} camera;

layout(set = 1, binding = 0, std140) uniform UniformObject {
    mat4 model;
} object;

void main()
{
    gl_Position = camera.proj * camera.view * object.model * vec4(position, 1.0);
    vs_out.color = color.rgb;
    vs_out.uv = uv;
}