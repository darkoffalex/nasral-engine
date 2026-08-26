#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Выходные данные (в следующие этапы)
layout (location = 0) out VS_OUT {
    vec2 uv;
} vs_out;

// Массив координат для 2 треугольников (6 вершин)
const vec2 positions[6] = vec2[6](
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0),

    vec2( 1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0, -1.0)
);

const vec2 uvs[6] = vec2[6](
    vec2(0.0, 1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),

    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    vs_out.uv = uvs[gl_VertexIndex];
}
