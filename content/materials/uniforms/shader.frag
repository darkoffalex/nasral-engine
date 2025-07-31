#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec4 color;

layout (location = 0) in VS_OUT {
    vec3 color;
    vec2 uv;
} fs_in;

layout(set = 2, binding = 0) uniform sampler2D tex_color;

void main()
{
    vec4 tex_color = texture(tex_color, fs_in.uv);
    color = vec4(tex_color.rgb, 1.0);
}