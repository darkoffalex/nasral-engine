#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec2 uv;
} fs_in;

// Выходы фрагментов (вложения кадрового буфера)
layout (location = 0) out vec4 frame_out;

// Push constants
layout(push_constant) uniform PushConstants {
    uint pass_index;
} pc_push;

// Текстуры кадрового буфера (в HDR формате, R16G16B16A16_SFLOAT)
layout(set = 0, binding = 0) uniform sampler2D frame_color;
layout(set = 0, binding = 1) uniform sampler2D frame_depth;
layout(set = 0, binding = 2) uniform sampler2D frame_normals;
layout(set = 0, binding = 3) uniform sampler2D frame_emission;

// Текстуры промежуточных данных (ping pong) для двух-проходных эффектов (bilateral blur и прочие)
layout(set = 0, binding = 4) uniform sampler2D frame_ping;
layout(set = 0, binding = 5) uniform sampler2D frame_pong;

// Uniform buffer для матриц камеры
layout(set = 1, binding = 0, std140) uniform UCamera {
    mat4 view;
    mat4 proj;
    mat4 view_inverse;
    mat4 proj_inverse;
    vec4 position;
} u_camera;

// Извлечь нормаль во View Space
vec3 fetch_view_normal(vec2 uv)
{
    // Распаковываем из [0, 1] в [-1, 1]
    vec3 world_normal = texture(frame_normals, uv).xyz * 2.0 - 1.0;
    vec3 view_normal = mat3(u_camera.view) * world_normal;
    return normalize(view_normal);
}

// Восстановление 3D-позиции во View Space
vec3 fetch_view_pos(vec2 uv)
{
    float depth_sample = texture(frame_depth, uv).r;

    // Vulkan ZO: NDC X in [-1..1], Y in [-1..1], Z in [0..1]
    vec4 ndc = vec4(
        uv.x * 2.0 - 1.0,
        (1.0 - uv.y) * 2.0 - 1.0,
        depth_sample, // Напрямую без * 2.0 - 1.0!
        1.0
    );

    vec4 view_pos = u_camera.proj_inverse * ndc;
    return view_pos.xyz / view_pos.w;
}

// Перевод из NDC пространства в UV-координаты (v - позиция после перспективного деления)
vec2 ndc_to_uv(vec2 v)
{
    return vec2(
        v.x * 0.5 + 0.5,
        1.0 - (v.y * 0.5 + 0.5)
    );
}

// Случайный шум (вариация 1)
float noise(vec2 p){
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// Случайный шум (вариация 2)
float gold_noise(in vec2 xy, in float seed){
    const float PHI = 1.61803398874989484820459;
    return fract(tan(distance(xy*PHI, xy)*seed)*xy.x);
}

// Случайный шум (вариация 3 - Interleaved Gradient Noise (Хорхе Хименес, Call of Duty))
float ign_noise(vec2 p){
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(p, magic.xy)));
}

// Случайный шум (вариация 4)
float hash12(vec2 p){
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Получить случайный угол
float random_angle(vec2 pixel_coord){
    vec2 cell = floor(pixel_coord / 2.0);
    float random_value = ign_noise(cell);
    float angle_index = floor(random_value * 16.0);
    return angle_index * (6.28318530718 / 16.0);
}

const int   KERNEL_SIZE = 16;
const float RADIUS      = 0.3;   // 30 см
const float BIAS        = 0.02;  // 2 см
const float AO_MUL      = 1.0;   // Множитель итогового эффекта
const float AO_POW      = 2.5;   // Степень эффекта

// Простое фиксированное ядро (полусфера Z >= 0)
const vec3 KERNEL[16] = vec3[16](
    vec3( 0.0242,  0.0000,  0.0969),
    vec3(-0.0518,  0.0475,  0.1568),
    vec3( 0.0136, -0.1548,  0.1863),
    vec3( 0.1542,  0.1563,  0.2045),

    vec3(-0.2490, -0.0440,  0.2233),
    vec3( 0.2371, -0.1508,  0.2504),
    vec3(-0.0837,  0.3107,  0.2576),
    vec3(-0.2854, -0.2748,  0.2681),

    vec3( 0.3960,  0.1446,  0.2865),
    vec3(-0.0600, -0.4494,  0.2944),
    vec3(-0.3660,  0.3161,  0.2990),
    vec3( 0.4898, -0.1790,  0.3028),

    vec3(-0.3585, -0.5069,  0.3053),
    vec3( 0.1575,  0.6086,  0.3069),
    vec3( 0.5281,  0.3871,  0.3078),
    vec3(-0.6485,  0.1090,  0.3079)
);

// Классический SSAO (Screen Space Ambient Occlusion)
float calculate_ssao(vec2 uv, bool normal_check)
{
    // Пропускаем фон
    if (texture(frame_depth, uv).r >= 1.0) {
        return 1.0;
    }

    // Извлечь позицию фрагмента и нормаль в пространстве вида
    vec3 P = fetch_view_pos(uv);
    vec3 N = fetch_view_normal(uv);

    // Получить базис-векторы для построения матрицы касательного пространства

    vec3 up = abs(N.z) < 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent_base = normalize(cross(up, N));
    vec3 bitangent_base = cross(N, tangent_base);

    // Развернуть тангент-вектор касательного пространства по случайному углу angle вокруг нормали и построить матрицу
    // В касательном пространстве нормаль (N) соответствует вектору Z (направлена вверх от плоскости)
    float angle = random_angle(gl_FragCoord.xy);
    vec3 T = cos(angle) * tangent_base + sin(angle) * bitangent_base;
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    // Вычисление затенённости фрагмента
    float occlusion = 0.0;
    int valid_samples = 0;
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        // Получить позицию точки выборки в пространстве вида
        vec3 k_sample = KERNEL[i];
        vec3 sample_pos = P + (TBN * k_sample) * RADIUS;

        // Проекция в координаты экрана (лоя последующей выборки глубины)
        vec4 offset = u_camera.proj * vec4(sample_pos, 1.0);
        if (offset.w <= 0.0) {
            continue;
        }

        offset.xyz /= offset.w;
        vec2 sample_uv = ndc_to_uv(offset.xy);

        // Отсекать выборку за границами экрана
        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
            continue;
        }

        // Отекать выборку очень далёких фрагментов (небо)
        if (texture(frame_depth, sample_uv).r >= 1.0) {
            continue;
        }

        // Семпл задействован
        valid_samples++;

        // Получаем РЕАЛЬНУЮ 3D-точку геометрии под этим сэмплом экрана
        vec3 real_pos = fetch_view_pos(sample_uv);

        // Геометрическая проверка
        if(normal_check)
        {
            // Вектор от центра фрагмента к реальной геометрии
            vec3 diff = real_pos - P;
            float dist = length(diff);

            // Проверка дальности: точка должна быть внутри радиуса сферы
            float range_check = smoothstep(0.0, 1.0, RADIUS / max(dist, 0.001));

            // Честная геометрическая проверка:
            // Точка затеняет нас, если она выступает НАД плоскостью нашего фрагмента (dot(diff, N) > BIAS)
            // И при этом реальная геометрия ближе к камере, чем сам сэмпл:
            float height_above_plane = dot(diff, N);
            if (height_above_plane > BIAS && real_pos.z >= sample_pos.z + BIAS) {
                occlusion += range_check * clamp(height_above_plane / RADIUS, 0.0, 1.0);
            }
        }
        // Упрощенная проверка
        else
        {
            // Стандартный range check: разность глубин вдоль направления взгляда.
            float z_distance = abs(P.z - real_pos.z);
            float range_check = smoothstep(
                0.0,
                1.0,
                RADIUS / max(z_distance, 0.001)
            );

            // В RH view space большее Z — ближе к камере.
            // Bias имеет знак '+' именно для защиты от self-occlusion.
            float is_occluded = real_pos.z >= sample_pos.z + BIAS ? 1.0 : 0.0;
            occlusion += is_occluded * range_check;
        }
    }

    // Если не было валидных семплов - нет и затенения
    if (valid_samples == 0) return 1.0;
    // Усреднить результат затенения всех семплов
    return pow(1.0 - clamp((occlusion / float(valid_samples)) * AO_MUL, 0.0, 1.0), AO_POW);
}

// Главная функция шейдера
void main()
{
    float ao = 1.0;
    switch(pc_push.pass_index)
    {
        case 0:{
            ao = calculate_ssao(fs_in.uv,true);
            break;
        }case 1:{
            // TODO: Рассчитать GTAO
            break;
        }
        default:{
            ao = 1.0;
            break;
        }
    }
    frame_out = vec4(vec3(ao), 1.0);
}