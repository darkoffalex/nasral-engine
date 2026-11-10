#pragma once
#include "_utils.slang"

/**
 * @brief Вычисляет линейное размытие текстуры вдоль заданного направления.
 * @param[in] tex        Входная текстура.
 * @param[in] uv         Текущие текстурные координаты.
 * @param[in] dir        Направление размытия.
 * @param[in] texel_size Размер одного текселя.
 * @param[in] samples    Количество выборок.
 * @param[in] base_radius Базовый радиус (шаг) текселя.
 * @return Усредненный цвет в формате float3.
 */
[ForceInline]
float3 linear_blur(Sampler2D tex, float2 uv, float2 dir, float2 texel_size, uint samples, float base_radius = 1.0)
{
    // 1. Рассчитываем кол-во выборок с двух сторон от центра (радиус выборки)
    int div = samples / 2;
    int from = -div;
    int to = div;

    // 2. Аккумулировать результат
    float3 result = float3(0.0);
    for(int i = from; i <= to; ++i){
        float2 offset = float(i) * dir * texel_size * base_radius;
        result += tex.Sample(uv + offset).rgb;
    }

    // 3. Усреднение (деление на вес, вес каждой выборки = 1)
    return result / float(samples);
}

/**
 * @brief Вычисляет линейное размытие текстуры вдоль заданного направления с учетом глубины.
 * @details Шаг смещения выборки масштабируется обратно пропорционально линейной глубине (закон 1/Z)
 *          при фиксированном количестве выборок.
 * @param[in] tex         Входная текстура цвета.
 * @param[in] tex_depth   Текстура буфера глубины.
 * @param[in] proj_matrix Матрица проекции камеры (для линеаризации глубины).
 * @param[in] uv          Текущие текстурные координаты.
 * @param[in] dir         Направление размытия.
 * @param[in] texel_size  Размер одного текселя.
 * @param[in] samples     Количество выборок.
 * @param[in] base_radius Базовый радиус (шаг) размытия на дистанции 1 метр (по умолчанию 2.0).
 * @return Усредненный цвет в формате float3.
 */
[ForceInline]
float3 linear_depth_aware_blur(
        Sampler2D tex,
        Sampler2D tex_depth,
        float4x4 proj_matrix,
        float2 uv,
        float2 dir,
        float2 texel_size,
        uint samples,
        float base_radius = 2.0)
{
    // 1. Получаем физическую глубину в метрах от камеры
    float z_meters = fetch_linear_depth(tex_depth,uv,proj_matrix);
    z_meters = max(z_meters, 0.1);

    // 2. Рассчитываем динамический радиус размытия в пикселях (Закон 1/Z)
    float dynamic_radius = base_radius / z_meters;
    dynamic_radius = clamp(dynamic_radius, 0.0, 32.0);

    // 3. Рассчитываем кол-во выборок с двух сторон от центра (радиус выборки)
    int div = samples / 2;
    int from = -div;
    int to = div;

    // 4. Аккумулировать результат с учетом переменного радиуса (меняем размер текселя выборки)
    float3 result = float3(0.0);
    for(int i = from; i <= to; ++i){
        float2 offset = float(i) * dir * texel_size * dynamic_radius;
        result += tex.Sample(uv + offset).rgb;
    }

    // 5. Усреднение (деление на вес, вес каждой выборки = 1)
    return result / float(samples);
}

/**
 * @brief Вычисляет билатеральное размытие текстуры вдоль заданного направления с сохранением границ.
 * @details Радиус размытия масштабируется обратно пропорционально линейной глубине (закон 1/Z).
 *          В пределах доступного количества выборок размытие регулируется активным размером ядра,
 *          а при достижении максимума ядра плавно увеличивается шаг смещения текселя.
 *          Вклад каждой выборки взвешивается на основе перепада глубины, угла между нормалями
 *          и опционального пространственного распределения Гаусса.
 * @param[in] tex                Входная текстура цвета.
 * @param[in] tex_depth          Текстура буфера глубины.
 * @param[in] tex_normals        Текстура мировых нормалей.
 * @param[in] proj_matrix        Матрица проекции камеры (для линеаризации глубины).
 * @param[in] uv                 Текущие текстурные координаты.
 * @param[in] dir                Направление размытия.
 * @param[in] texel_size         Размер одного текселя.
 * @param[in] samples            Максимальное количество выборок.
 * @param[in] base_kernel_radius Базовый радиус ядра размытия на дистанции 1 метр (по умолчанию 6.0).
 * @param[in] base_tex_radius    Базовый множитель шага текселя (по умолчанию 2.0).
 * @param[in] depth_threshold    Порог чувствительности к перепаду глубины в метрах (по умолчанию 0.5).
 * @param[in] gauss_weight       Флаг использования пространственного веса по Гауссу (по умолчанию true).
 * @return Взвешенный усредненный цвет в формате float3.
 */
[ForceInline]
float3 linear_bilateral_blur(
        Sampler2D tex,
        Sampler2D tex_depth,
        Sampler2D tex_normals,
        float4x4 proj_matrix,
        float2 uv,
        float2 dir,
        float2 texel_size,
        uint samples,
        float base_kernel_radius = 6.0,
        float base_tex_radius = 2.0,
        float depth_threshold = 0.5,
        bool gauss_weight = true)
{
    // 1. Получаем нормаль и физическую глубину
    float3 normal = fetch_world_normal(tex_normals, uv);
    float depth = fetch_linear_depth(tex_depth, uv,proj_matrix);
    depth = max(depth, 0.01);

    // 2. Рассчитываем динамический радиус размытия в пикселях (Закон 1/Z).
    // Пока dynamic_radius <= max_kernel_r, растет только размер ядра (при шаге 1 тексель).
    // Когда dynamic_radius превышает max_kernel_r, начинает пропорционально расти шаг текселя (texel_dr),
    // так что итоговый охват (kernel_dr * texel_dr) растет плавно и без скачков.
    float max_kernel_r = max(floor(float(samples) / 2.0), 1.0);
    float dynamic_radius = max(base_kernel_radius / depth, 1.0);
    float kernel_dr = min(dynamic_radius, max_kernel_r);
    float texel_dr = (dynamic_radius / kernel_dr) * base_tex_radius;

    // 3. Рассчитываем кол-во выборок с двух сторон от центра (радиус выборки). Целочисленное деление (9 / 2 = 4)
    int r = samples / 2;

    // 4. Пред-расчет параметров Гаусса (ВНЕ цикла for)
    float sigma = kernel_dr * 0.5;
    float two_sigma_sq = max(2.0 * sigma * sigma, 0.0001);

    // 5. Аккумулировать результат с учетом переменного радиуса
    float3 result_c = float3(0.0);
    float result_w = 0.0;

    for(int i = -r; i <= r; ++i)
    {
        // Сглаженный вес (граница выборки)
        // Кол-во шагов фиксировано, но радиус размытия меняется непрерывно
        // Нужно корректно учесть интенсивность вклада пограничных семплов
        float w_edge = clamp(kernel_dr - float(abs(i)) + 1.0, 0.0, 1.0);

        // Для заведомо нулевых весов пропускаем выборку из текстуры
        if (w_edge <= 0.0) continue;

        // Рассчитать UV выборки
        float2 offset = float(i) * dir * texel_size * texel_dr;
        float2 sample_uv = uv + offset;

        // Данные выборки (цвет, глубина, нормаль)
        float3 sample_color = tex.Sample(sample_uv).rgb;
        float sample_depth = fetch_linear_depth(tex_depth, sample_uv,proj_matrix);
        float3 sample_normal  = fetch_world_normal(tex_normals, sample_uv);

        // Пространственный вес (Гаусс)
        float w_spatial = gauss_weight ? exp(-float(i * i) / two_sigma_sq) : 1.0;

        // Вес по глубине (чем больше разница глубины - тем меньше нужно учитывать вклад)
        float depth_diff = abs(depth - sample_depth);
        float w_depth = exp(-depth_diff / depth_threshold);

        // Вес по нормалям (отсекаем соприкасающиеся грани под другим углом)
        float w_normal = pow(max(0.0, dot(normal, sample_normal)), 8.0);

        // Итоговый вес выборки
        float weight = w_edge * w_depth * w_normal * w_spatial;

        // Итоговый вклад
        result_c += sample_color * weight;
        result_w += weight;
    }

    // 6. Усреднение (деление на вес)
    return result_c / max(result_w, 0.0001);
}