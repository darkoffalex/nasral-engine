#version 450 core
#extension GL_ARB_separate_shader_objects : enable

// Входные данные фрагмента
layout (location = 0) in VS_OUT {
    vec2 uv;
} fs_in;

// Выход фрагмента (цветовое вложение 0)
layout (location = 0) out vec4 color;

// Push constants
layout(push_constant) uniform PushConstants {
    uint pass_index;
} pc_push;

// Текстуры кадрового буфера (в HDR формате, R16G16B16A16_SFLOAT)
layout(set = 0, binding = 0) uniform sampler2D frame_color;
layout(set = 0, binding = 1) uniform sampler2D frame_depth;
layout(set = 0, binding = 2) uniform sampler2D frame_normals;
layout(set = 0, binding = 3) uniform sampler2D frame_emission;

// Uniform buffer для матриц камеры
layout(set = 1, binding = 0, std140) uniform UCamera {
    mat4 view;
    mat4 proj;
    mat4 view_inverse;
    mat4 proj_inverse;
    vec4 position;
} u_camera;

// Экспозиция (далее будет передаваться через Push Constants)
const float exposure = 1.0;

// Функция ACES Tone Mapping
vec3 tone_map_aces(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Размытие BOX-фильтром
vec3 box_blur3x3(sampler2D tex, vec2 uv, float lod)
{
    vec3 result = vec3(0.0);
    // Получаем размер одного текселя ТЕКУЩЕГО мип-уровня
    vec2 texel_size = 1.0 / vec2(textureSize(tex, int(lod)));
    // Сэмплируем сетку 3x3 со сдвигом на 1 тексель мип-уровня
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texel_size;
            result += textureLod(tex, uv + offset, lod).rgb;
        }
    }

    return result / 9.0; // Усредняем 9 сэмплов
}

// Размытие Gauss-фильтром (трюк с аппаратной интерполяцией для выборки 4x4)
vec3 gauss_fast_blur4x4(sampler2D tex, vec2 uv, float lod){
    vec3 result = vec3(0.0);
    // Получаем размер одного текселя ТЕКУЩЕГО мип-уровня
    vec2 texel_size = 1.0 / vec2(textureSize(tex, int(lod)));
    // Сдвиг на 0.5 текселя для аппаратной интерполяции 4-х соседей
    vec2 offset = texel_size * 0.5;
    result += textureLod(tex, uv + vec2(-offset.x, -offset.y), lod).rgb;
    result += textureLod(tex, uv + vec2( offset.x, -offset.y), lod).rgb;
    result += textureLod(tex, uv + vec2(-offset.x,  offset.y), lod).rgb;
    result += textureLod(tex, uv + vec2( offset.x,  offset.y), lod).rgb;
    // Складываем с одинаковым весом 0.25, так как интерполятор сам учтет Гаусс
    return result * 0.25;
}

// Размытие за счет мип-уровней буфера
vec3 mip_blur(sampler2D tex, vec2 uv){
    vec3 result = vec3(0.0);
    result += textureLod(frame_emission, fs_in.uv, 2.0).rgb * 0.4;
    result += textureLod(frame_emission, fs_in.uv, 3.0).rgb * 0.3;
    result += textureLod(frame_emission, fs_in.uv, 4.0).rgb * 0.2;
    result += textureLod(frame_emission, fs_in.uv, 5.0).rgb * 0.1;
    return result;
}

// Комбинированное размытие (mip + box) для двух mip уровней
vec3 mip_box_blur(sampler2D tex, vec2 uv){
    vec3 result = vec3(0.0);
    result += box_blur3x3(tex, uv, 2.0) * 0.7;
    result += box_blur3x3(tex, uv, 3.0) * 0.3;
    return result;
}

// Комбинированное размытие (mip + gauss) для двух mip уровней
vec3 mip_gauss_blur(sampler2D tex, vec2 uv){
    vec3 result = vec3(0.0);
    result += gauss_fast_blur4x4(tex, uv, 2.0) * 0.7;
    result += gauss_fast_blur4x4(tex, uv, 3.0) * 0.3;
    return result;
}

// Главная функция шейдера
void main()
{
    // Считываем чистый HDR цвет кадра
    vec3 base_hdr = texture(frame_color, fs_in.uv).rgb;

    // HDR цвет размытия bloom эффекта (доробный mip дает интерполяцию между 3 и 4 уровнями)
    vec3 bloom_hdr = box_blur3x3(frame_emission, fs_in.uv, 2.5);

    // Складываем их в ЛИНЕЙНОМ HDR пространстве
    vec3 hdr_composite = base_hdr + bloom_hdr;

    // Экпозиция (яркость)
    hdr_composite *= exposure;

    // Выполняем Tone Mapping (сжатие HDR -> SDR)
    vec3 sdr_color = tone_map_aces(hdr_composite);

    // Гамма коррекция
    vec3 final_color = pow(sdr_color, vec3(1.0 / 2.2));

    // Итоговый результат: сложение основного цвета и размытого сияния (Bloom)
    color = vec4(final_color, 1.0);
}
