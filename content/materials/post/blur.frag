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

// Извлечь нормаль в пространстве вида
vec3 fetch_view_normal(vec2 uv)
{
    // Распаковываем из [0, 1] в [-1, 1]
    vec3 world_normal = texture(frame_normals, uv).xyz * 2.0 - 1.0;
    // Переводим во View Space (используем mat3, чтобы исключить сдвиг/позицию)
    vec3 view_normal = mat3(u_camera.view) * world_normal;
    return normalize(view_normal);
}

// Извлечь глубину в линейном пространстве (в метрах)
float fetch_linear_depth(vec2 uv)
{
    float depth_sample = texture(frame_depth, uv).r;
    // Переводим UV [0..1] и Depth [0..1] в NDC [-1..1]
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth_sample, 1.0);
    // Восстанавливаем 3D-позицию во View Space
    vec4 view_pos = u_camera.proj_inverse * ndc;
    // Возвращаем физическую дистанцию до камеры вдоль оси Z (в метрах)
    return abs(view_pos.z / view_pos.w);
}

// Линейное размытие (в одном направлении)
vec3 linear_blur(sampler2D tex, vec2 uv, vec2 dir, vec2 texel_size, int samples)
{
    // 1. Рассчитываем кол-во выборок с двух сторон от центра (радиус выборки)
    int div = samples / 2;
    int from = -div;
    int to = div;

    // 2. Аккумулировать результат
    vec3 result = vec3(0.0);
    for(int i = from; i <= to; ++i){
        vec2 offset = float(i) * dir * texel_size;
        result += texture(tex, uv + offset).rgb;
    }

    // 3. Усреднение (деление на вес, вес каждой выборки = 1)
    return result / float(samples);
}

// Желаемый радиус размытия в пикселях для объекта на расстоянии 1 метра
// Радиус будет меняться обратно пропорционально дистанции (далекие объекты размыты слабее)
const float BASE_TEX_RADIUS = 2.0;
const float BASE_KERNEL_RADIUS = 4.0;
const float DEPTH_THRESHOLD = 2.0;

// Линейное размытие с учетом глубины (в одном направлении)
// В данном алгоритме увеличивается радиус текселя выборки, в то время как ядро выборки постоянно
vec3 linear_depth_aware_blur(sampler2D tex, vec2 uv, vec2 dir, vec2 texel_size, int samples)
{
    // 1. Получаем физическую глубину в метрах от камеры
    float z_meters = fetch_linear_depth(fs_in.uv);
    z_meters = max(z_meters, 0.1);

    // 2. Рассчитываем динамический радиус размытия в пикселях (Закон 1/Z)
    float dynamic_radius = BASE_TEX_RADIUS / z_meters;
    dynamic_radius = clamp(dynamic_radius, 1.0, 32.0);

    // 3. Рассчитываем кол-во выборок с двух сторон от центра (радиус выборки)
    int div = samples / 2;
    int from = -div;
    int to = div;

    // 4. Аккумулировать результат с учетом переменного радиуса (меняем размер текселя выборки)
    vec3 result = vec3(0.0);
    for(int i = from; i <= to; ++i){
        vec2 offset = float(i) * dir * texel_size * dynamic_radius;
        result += texture(tex, uv + offset).rgb;
    }

    // 5. Усреднение (деление на вес, вес каждой выборки = 1)
    return result / float(samples);
}

// Bilateral Blur
// В данном алгоритме дистанция влияет на динамический радиус ядра выборки (что влияет на вес выборок)
// В том числе учитывается граница глубины, и ориентированность нормалей а также Гауссов вес
vec3 bilateral_blur(sampler2D tex, vec2 uv, vec2 dir, vec2 texel_size, int samples)
{
    // 1. Получаем нормаль и физическую глубину (пространство вида)
    vec3 normal = fetch_view_normal(fs_in.uv);
    float depth = fetch_linear_depth(fs_in.uv);
    depth = max(depth, 0.01);

    // 2. Рассчитываем динамический радиус размытия в пикселях (Закон 1/Z)
    float kernel_dr = BASE_KERNEL_RADIUS / depth;
    kernel_dr = clamp(kernel_dr, 1.0, floor(float(samples) / 2.0));

    // 3. Рассчитываем кол-во выборок с двух сторон от центра (радиус выборки)
    int r = samples / 2;

    // 4. Аккумулировать результат с учетом переменного радиуса (как текселя так и ядра)
    vec3 result_c = vec3(0.0);
    float result_w = 0.0;

    for(int i = -r; i <= r; ++i){
        // Сглаженный вес (граница выборки)
        // Кол-во шагов фиксировано, но радиус размытия меняется непрерывно
        // Нужно корректно учесть интенсивность вклада пограничных семплов
        float w_edge = clamp(kernel_dr - float(abs(i)) + 1.0, 0.0, 1.0);
        
        // Для заведомо нулевых весов пропускаем выборку из текстуры
        if (w_edge <= 0.0) continue;

        // Рассчитать UV выборки (используем временно коэффициент размытия 2.5)
        vec2 offset = float(i) * dir * texel_size * 2.5;
        vec2 sample_uv = uv + offset;

        // Данные выборки (цвет, глубина, нормаль)
        vec3 sample_color = texture(tex, sample_uv).rgb;
        float sample_depth = fetch_linear_depth(sample_uv);
        vec3 sample_normal  = fetch_view_normal(sample_uv);

        // Пространственный вес (Гаусс)
        float sigma = float(kernel_dr) * 0.5;
        float w_spatial = exp(-float(i * i) / (2.0 * sigma * sigma));

        // Вес по глубине (чем больше разница глубины - тем меньше нужно учитывать вклад)
        float depth_diff = abs(depth - sample_depth);
        float w_depth = exp(-depth_diff / DEPTH_THRESHOLD);

        // Вес по нормалям (отсекаем соприкасающиеся грани под другим углом)
        float w_normal = pow(max(0.0, dot(normal, sample_normal)), 8.0);

        // Итоговый вес выборки
        float weight = w_edge * w_depth * w_normal * w_spatial;

        // Итоговый вклад
        result_c += sample_color * weight;
        result_w += weight;
    }

    // 5. Усреднение (деление на вес)
    return result_c / max(result_w, 0.0001);
}

// Главная функция шейдера
void main()
{
    if(pc_push.pass_index == 0)
    {
        vec2 texel_size = 1.0 / vec2(textureSize(frame_emission, 0));
        vec3 blurred = bilateral_blur(frame_emission, fs_in.uv, vec2(1.0, 0.0), texel_size, 16);
        frame_out = vec4(blurred, 1.0);
    }
    else
    {
        vec2 texel_size = 1.0 / vec2(textureSize(frame_ping, 0));
        vec3 blurred = bilateral_blur(frame_ping, fs_in.uv, vec2(0.0, 1.0), texel_size, 16);
        frame_out = vec4(blurred, 1.0);
    }
}