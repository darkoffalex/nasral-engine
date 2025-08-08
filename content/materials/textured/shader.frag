#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#define MAX_OBJECTS 1000

layout (location = 0) out vec4 color;

layout (location = 0) in VS_OUT {
    vec3 color;
    vec2 uv;
} fs_in;

layout(push_constant) uniform PushConstants {
    uint obj_index;
} pc;

struct MaterialSettings
{
    vec4 color;
    float shininess;
    float specular;
    float padding;
};

layout(set = 1, binding = 1, std140) uniform UniformObjectMaterials {
    MaterialSettings material[MAX_OBJECTS];
} object;

layout(set = 2, binding = 0) uniform sampler2D tex_color[MAX_OBJECTS];
layout(set = 2, binding = 1) uniform sampler2D tex_normal[MAX_OBJECTS];
layout(set = 2, binding = 2) uniform sampler2D tex_spec[MAX_OBJECTS];

void main()
{
    vec4 tex_color = texture(tex_color[pc.obj_index], fs_in.uv);
    color = vec4(tex_color.rgb, 1.0);
}