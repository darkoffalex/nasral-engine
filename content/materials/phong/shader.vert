#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable

// Константы
#define MAX_OBJECTS 1000

// Входные данные вершины
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;

// Выходные данные (в следующие этапы)
layout(location = 0) out VS_OUT {
    vec3 color;
    vec3 position;
    vec3 normal;
    vec2 uv;
} vs_out;

// Push constants
layout(push_constant) uniform PushConstants {
    uint mat_index;
    uint obj_index;
} pc_push;

// Параметры трансформаций одиночного объекта
struct ObjectTransforms
{
    mat4 model;
    mat4 normals;
};

// Uniform buffer для матриц камеры
layout(set = 0, binding = 0, std140) uniform UCamera {
    mat4 view;
    mat4 proj;
    vec4 position;
} u_camera;

// Storage buffer для матриц объектов
layout(set = 1, binding = 0, std430) readonly buffer SObjectTransforms {
    ObjectTransforms s_objects[MAX_OBJECTS];
};

void main()
{
    // Матрицы модели и нормалей
    mat4 model = s_objects[pc_push.obj_index].model;
    mat3 normal_mat = mat3(s_objects[pc_push.obj_index].normals);

    // Нораль в мировом пространстве
    vec4 world_pos = model * vec4(in_position, 1.0);

    // Выход
    vs_out.uv = in_uv;
    vs_out.color = in_color.rgb;
    vs_out.normal = normal_mat * in_normal;
    vs_out.position = world_pos.xyz;
    gl_Position = u_camera.proj * u_camera.view * world_pos;
}