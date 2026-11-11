#include "_common.slang"
#include "_utils.slang"
#include "_ao_kernels.slang"

// Параметры экранного прохода AO (set = 2, binding = 0)
// Структура соответствует gfx::uniforms::ScreenPassAo в C++ коде движка
struct ScreenPassSettings
{
    uint type;
    uint samples;
    float radius;
    float bias;
    float multiplier;
    float power;
};

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ И РАСЧЕТ SSAO
// ============================================================================

/**
 * @brief Переводит координаты из NDC [-1..1] в текстурные координаты UV [0..1] (с учетом инверсии оси Y в Vulkan).
 * @param[in] ndc_xy Двумерные координаты в пространстве NDC.
 * @return Текстурные координаты UV.
 */
[ForceInline]
float2 ndc_to_uv(float2 ndc_xy)
{
    return float2(
        ndc_xy.x * 0.5 + 0.5,
        1.0 - (ndc_xy.y * 0.5 + 0.5)
    );
}

/**
 * @brief Вычисляет коэффициент фонового затенения методом Screen Space Ambient Occlusion (SSAO).
 * @details Реализует ориентированный по нормали полусферический алгоритм SSAO в пространстве вида (View Space):
 *          1. Восстанавливает 3D-позицию и нормаль фрагмента во View Space с помощью функций из _utils.slang.
 *          2. Строит ортонормированный базис касательного пространства (TBN) по методу Frisvad / Duff et al.
 *             и поворачивает его вокруг нормали на случайный угол на основе Interleaved Gradient Noise (IGN).
 *          3. Проецирует точки полусферического ядра в экранное пространство и считывает реальную глубину сцены.
 *          4. Оценивает вклад каждого сэмпла с учетом плавного затухания на границах радиуса (range check)
 *             и защиты от самозатенения (bias), масштабируемой по глубине фрагмента.
 * @param[in] uv          Текущие текстурные координаты фрагмента [0..1].
 * @param[in] pixel_coord Экранные пиксельные координаты фрагмента (SV_Position.xy).
 * @param[in] settings    Параметры алгоритма AO (радиус, смещение bias, множитель, степень).
 * @return Итоговый коэффициент освещенности (1.0 — отсутствие затенения, 0.0 — полное затенение).
 */
float calculate_ssao(float2 uv, float2 pixel_coord, ScreenPassSettings settings)
{
    // 1. Считываем нелинейную глубину текущего пикселя и отсекаем фон (небо / дальнюю плоскость)
    float raw_depth = t_depth.SampleLevel(uv, 0.0).r;
    if (raw_depth >= 1.0)
    {
        return 1.0;
    }

    // 2. Восстанавливаем 3D-позицию и нормаль фрагмента в пространстве вида (Right-Handed View Space, камера смотрит вдоль -Z)
    float3 view_pos = reconstruct_view_pos_fast(t_depth, uv, u_camera.proj);
    float3 view_normal = fetch_view_normal(t_normal, uv, u_camera.view);

    // 3. Построение ортонормированного базиса касательного пространства (TBN) вокруг нормали view_normal.
    // Выбираем вспомогательный вектор up так, чтобы избежать вырождения векторного произведения при N || Z.
    float3 up = abs(view_normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent_base = normalize(cross(up, view_normal));
    float3 bitangent_base = cross(view_normal, tangent_base);

    // 4. Случайное вращение ядра в плоскости (T, B) вокруг нормали N.
    // Используем Interleaved Gradient Noise из _utils.slang для равномерного распределения углов
    // между соседними пикселями — это заменяет периодическую 4x4 текстуру шума и устраняет полосы (banding).
    float angle = random_angle(pixel_coord, 1.0, 16.0);
    float sin_a, cos_a;
    sincos(angle, sin_a, cos_a);

    float3 tangent   = cos_a * tangent_base + sin_a * bitangent_base;
    float3 bitangent = -sin_a * tangent_base + cos_a * bitangent_base;

    // Матрица перехода из касательного пространства в пространство вида (векторы-столбцы T, B, N)
    float3x3 TBN = float3x3(
        tangent.x, bitangent.x, view_normal.x,
        tangent.y, bitangent.y, view_normal.y,
        tangent.z, bitangent.z, view_normal.z
    );

    // Слегка сдвигаем стартовую точку вдоль нормали поверхности, чтобы минимизировать артефакты
    // дискретизации буфера глубины (acne) на наклонных поверхностях
    float depth_scale = max(-view_pos.z, 1.0);
    float effective_bias = settings.bias * (1.0 + 0.05 * depth_scale);
    float3 origin_pos = view_pos + view_normal * (effective_bias * 0.5);

    // 5. Выбор ядра и основной цикл интегрирования затенения по полусфере
    uint kernel_index = get_kernel_index(settings.samples);
    uint kernel_size = get_kernel_size(settings.samples);

    float occlusion = 0.0;
    float valid_samples = 0.0;

    for (uint i = 0; i < kernel_size; ++i)
    {
        // Переводим вектор выборки из касательного пространства во View Space
        float3 sample_dir = mul(TBN, SSAO_KERNELS[kernel_index][i]);
        float3 sample_pos = origin_pos + sample_dir * settings.radius;

        // Проецируем 3D-точку выборки в экранное пространство (Clip Space -> NDC -> UV)
        float4 clip_pos = mul(u_camera.proj, float4(sample_pos, 1.0));
        if (clip_pos.w <= 0.0)
        {
            continue;
        }

        float2 sample_ndc = clip_pos.xy / clip_pos.w;
        float2 sample_uv = ndc_to_uv(sample_ndc);

        // Проверяем, что спроецированная точка не вышла за границы кадра
        if (any(sample_uv < 0.0) || any(sample_uv > 1.0))
        {
            continue;
        }

        // Проверяем глубину в точке выборки и пропускаем пиксели неба (фона)
        float sample_raw_depth = t_depth.SampleLevel(sample_uv, 0.0).r;
        if (sample_raw_depth >= 1.0)
        {
            continue;
        }

        valid_samples += 1.0;

        // Восстанавливаем реальную 3D-позицию геометрии на экране под текущим сэмплом
        float3 occluder_pos = reconstruct_view_pos_fast(t_depth, sample_uv, u_camera.proj);

        // В правосторонней системе координат (RH View Space) Z <= 0, поэтому большее значение Z
        // означает, что реальная поверхность (occluder_pos.z) находится БЛИЖЕ к камере, чем точка выборки (sample_pos.z).
        float depth_delta = occluder_pos.z - (sample_pos.z + effective_bias);
        if (depth_delta > 0.0)
        {
            // Вектор от исследуемого фрагмента к реальной точке-затенителю
            float3 to_occluder = occluder_pos - view_pos;
            float dist_to_occluder = length(to_occluder);

            // Плавная проверка дальности (Range Check): исключает ложные темные ореолы (halo) вокруг объектов,
            // находящихся далеко впереди фона на расстоянии, существенно превышающем радиус settings.radius
            float range_weight = smoothstep(0.0, 1.0, settings.radius / max(dist_to_occluder, 0.0001));

            // Угловой вес (косинус угла относительно нормали): точки, находящиеся прямо над поверхностью,
            // дают больший вклад в затенение, чем точки у самой касательной плоскости
            float3 occluder_dir = to_occluder / max(dist_to_occluder, 0.0001);
            float normal_weight = saturate(dot(view_normal, occluder_dir) - settings.bias);

            // Плавный переход срабатывания по глубине для устранения резких ступенчатых границ
            float depth_weight = saturate(depth_delta / max(effective_bias, 0.001));

            occlusion += range_weight * normal_weight * depth_weight;
        }
    }

    // 6. Нормализация и художественная настройка интенсивности и контраста AO
    if (valid_samples <= 0.0)
    {
        return 1.0;
    }

    float normalized_occlusion = (occlusion / valid_samples) * settings.multiplier;
    float ao = saturate(1.0 - normalized_occlusion);
    return pow(ao, max(settings.power, 0.0001));
}

// ============================================================================
// ФРАГМЕНТНЫЙ ШЕЙДЕР
// ============================================================================

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    // Получить настройки экранного прохода
    ScreenPassSettings settings = s_screen_passes.Load<ScreenPassSettings>(pc_push.pass_index * pc_push.pass_stride);

    // Вычислить затенение в зависимости от выбранного алгоритма
    float ao = 1.0;
    if (settings.type == 0)
    {
        ao = calculate_ssao(input.uv, input.clipPosition.xy, settings);
    }

    // Вывод во вложение
    FragmentOutput output;
    output.color = float4(ao, ao, ao, 1.0);
    return output;
}
