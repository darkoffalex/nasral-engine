#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные: треугольники из вершинного шейдера
layout(triangles) in;

// Выходные данные: треугольная полоса, максимум 3 вершины
layout(triangle_strip, max_vertices = 3) out;

// Входные данные от вершинного шейдера
layout(location = 0) in VS_OUT {
    vec3 color;
    vec3 position;
    vec3 normal;
    vec2 uv;
} gs_in[];

// Выходные данные для фрагментного шейдера
layout(location = 0) out GS_OUT {
    vec3 color;
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec3 tangent;
    vec3 bitangent;
    mat3 TBN;
} gs_out;

void main()
{
    // Вычисление тангента и битангента на основе позиций и UV-координат треугольника
    vec3 edge1 = gs_in[1].position - gs_in[0].position;
    vec3 edge2 = gs_in[2].position - gs_in[0].position;
    vec2 deltaUV1 = gs_in[1].uv - gs_in[0].uv;
    vec2 deltaUV2 = gs_in[2].uv - gs_in[0].uv;

    // Вычисление тангента и битангента
    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
    vec3 tangent = normalize(vec3(
        f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
        f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
        f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
        ));
    vec3 bitangent = normalize(vec3(
        f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
        f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
        f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z)
        ));

    // Пройтись по каждой вершине входного треугольника
    for (int i = 0; i < 3; ++i)
    {
        // Ортогонализованные компоненты TBN матрицы
        vec3 T = normalize(tangent);
        vec3 N = normalize(gs_in[i].normal);
        T = normalize(T - dot(T, N) * N);
        vec3 B = normalize(cross(N, T));

        // Копировать входные данные в выходные
        gs_out.color = gs_in[i].color;
        gs_out.position = gs_in[i].position;
        gs_out.normal = gs_in[i].normal;
        gs_out.uv = gs_in[i].uv;
        gs_out.tangent = T;
        gs_out.bitangent = B;
        gs_out.TBN = mat3(T, B, N);

        // Установить позицию вершины
        gl_Position = gl_in[i].gl_Position;

        // Выдать вершину
        EmitVertex();
    }

    // Завершить треугольник
    EndPrimitive();
}