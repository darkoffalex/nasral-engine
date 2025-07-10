/**
 * @file device.hpp
 * @brief Обертка для управления Vulkan устройством и очередями команд
 * @author Alex "DarkWolf" Nem
 * @date 2025
 * @copyright MIT License
 *
 * Файл содержит реализацию класса Device, который инкапсулирует работу
 * с физическим и логическим устройствами Vulkan, а также управляет группами
 * очередей команд и их командными пулами.
 */

#pragma once
#include <map>
#include <vector>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <set>

namespace vk::utils
{
    /**
     * @brief RAII обертка для управления Vulkan устройством.
     *
     * Предоставляет высокоуровневый интерфейс для:
     *  выбора подходящего физического устройства,
     *  создания логического устройства,
     *  управления группами очередей команд,
     *  работы с командными пулами
     */
    class Device
    {
    public:
        typedef std::unique_ptr<Device> Ptr;

        /**
         * @brief Запрос на создание группы очередей.
         */
        struct QueueGroupRequest
        {
            vk::QueueFlags  queue_flags     = vk::QueueFlagBits::eGraphics;  ///< Требуемые флаги очередей
            bool            require_present = false;                         ///< Требуется ли поддержка представления (показа)
            uint32_t        queue_count     = 1;                             ///< Количество очередей в группе
            uint32_t        pool_count      = 1;                             ///< Количество командных пулов

            /**
             * @brief Создает запрос для очередей графических команд
             * @param queue_count Количество очередей
             * @param present Требуется ли представление
             * @return Сконфигурированный запрос
             */
            static QueueGroupRequest graphics(const uint32_t queue_count = 1, const bool present = true) {
                return {
                    vk::QueueFlagBits::eGraphics,
                    present,
                    queue_count
                };
            }

            /**
             * @brief Создает запрос для очередей команд перемещения данных
             * @param queue_count Количество очередей
             * @return
             */
            static QueueGroupRequest transfer(const uint32_t queue_count = 1) {
                return {
                    vk::QueueFlagBits::eTransfer,
                    false,
                    queue_count
                };
            }

            /**
             * @brief Создает запрос для очередей вычислительных команд
             * @param queue_count Количество очередей
             * @return Сконфигурированный запрос
             */
            static QueueGroupRequest compute(const uint32_t queue_count = 1) {
                return {
                    vk::QueueFlagBits::eCompute,
                    false,
                    queue_count
                };
            }
        };

        /**
         * @brief Группа очередей с общим семейством.
         *
         * Содержит информацию о семействе очередей, сами очереди и их командные пулы
         */
        struct QueueGroup
        {
            std::optional<uint32_t> family_index = 0;
            std::vector<vk::Queue> queues;
            std::vector<vk::UniqueCommandPool> command_pools;
        };

        /** @brief Конструктор по умолчанию */
        Device() = default;

        /**
         * @brief Создает устройство с заданными параметрами
         * @param instance Экземпляр Vulkan
         * @param surface Поверхность для отображения
         * @param req_queue_groups Запросы на создание групп очередей
         * @param req_extensions Требуемые расширения устройства
         * @param allow_integrated_device Разрешить использование интегрированного GPU
         * @throw std::runtime_error Если не удалось создать подходящее устройство
         */
        Device(const vk::UniqueInstance& instance,
               const vk::UniqueSurfaceKHR& surface,
               const std::vector<QueueGroupRequest>& req_queue_groups,
               const std::vector<const char*>& req_extensions,
               const bool allow_integrated_device = false) : Device()
        {
            // Поиск подходящего физ устройства
            pick_physical_device(instance,
                                 surface,
                                 req_queue_groups,
                                 req_extensions,
                                 allow_integrated_device);

            // Создание логического устройства
            init_logical_device(req_queue_groups,
                                req_extensions);
        }

        /** @brief Деструктор */
        ~Device() = default;

        /** @brief Запрет копирования */
        Device(const Device& other) = delete;

        /** @brief Запрет присваивания */
        Device& operator=(const Device& other) = delete;

        /**
         * @brief Проверяет инициализировано ли устройство
         * @return True, если устройство успешно создано
         */
        explicit operator bool() const {
            return static_cast<bool>(physical_device_) && static_cast<bool>(device_);
        }

        /**
         * @brief Ищет подходящий тип памяти по заданным требованиям
         * @param m_req Требования к памяти
         * @param m_prop_flag Требуемые свойства памяти
         * @return Индекс типа памяти или std::nullopt если не найден
         */
        [[nodiscard]] std::optional<uint32_t> find_memory_type_index(
                const vk::MemoryRequirements& m_req,
                const vk::MemoryPropertyFlags& m_prop_flag) const noexcept
        {
            std::optional<uint32_t> index = std::nullopt;
            const auto type_bits = m_req.memoryTypeBits;
            const auto m_props = physical_device_.getMemoryProperties();

            for(uint32_t i = 0; i < m_props.memoryTypeCount; ++i){
                const bool type_supported = ((type_bits & (1 << i)) > 0);
                const bool prop_supported = (m_props.memoryTypes[i].propertyFlags & m_prop_flag) == m_prop_flag;
                if(type_supported && prop_supported){
                    index = i;
                    break;
                }
            }

            return index;
        }

        /**
         * @brief Проверяет поддержку формата поверхности
         * @param format Проверяемый формат
         * @param surface Поверхность для проверки
         * @return True, если формат поддерживается
         */
        [[nodiscard]] bool supports_color(const vk::Format& format, const vk::UniqueSurfaceKHR& surface) const
        {
            const auto sfs = physical_device_.getSurfaceFormatsKHR(surface.get());
            if(sfs.empty()) return false;

            if(sfs.size() == 1 && sfs[0].format == ::vk::Format::eUndefined){
                return true;
            }

            return std::any_of(sfs.cbegin(), sfs.cend(), [&](const ::vk::SurfaceFormatKHR& sf){
                return sf.format == format;
            });
        }

        /**
         * @brief Проверяет поддержку формата и цветового пространства поверхности
         * @param format Проверяемый формат с цветовым пространством
         * @param surface Поверхность для проверки
         * @return True, если формат поддерживается
         */
        [[nodiscard]] bool supports_format(const vk::SurfaceFormatKHR& format, const vk::UniqueSurfaceKHR& surface) const
        {
            const auto sfs = physical_device_.getSurfaceFormatsKHR(surface.get());
            if(sfs.empty()) return false;

            if(sfs.size() == 1 && sfs[0].format == ::vk::Format::eUndefined){
                return true;
            }

            return std::any_of(sfs.begin(), sfs.end(), [&](const ::vk::SurfaceFormatKHR& sf){
                return sf.format == format.format && sf.colorSpace == format.colorSpace;
            });
        }

        /**
         * @brief Проверяет поддержку формата для глубины
         * @param format Проверяемый формат
         * @return True, если формат поддерживается для буфера глубины
         */
        [[nodiscard]] bool supports_depth(const ::vk::Format& format) const
        {
            const auto fp = physical_device_.getFormatProperties(format);
            return static_cast<bool>(fp.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        }

        /**
         * @brief Проверяет принадлежность групп очередей к одному семейству
         * @param indices Индексы проверяемых групп
         * @return True, если все группы принадлежат одному семейству
         */
        [[nodiscard]] bool is_same_family(const std::vector<size_t>& indices) const {
            if (indices.empty()) return true;

            if (std::any_of(indices.begin(), indices.end(),
                [this](const size_t idx) { return idx >= queue_groups_.size(); })) {
                return false;
            }

            return std::adjacent_find(indices.begin(), indices.end(),
                [this](const size_t i1, const size_t i2) {
                    return queue_groups_[i1].family_index != queue_groups_[i2].family_index;
                }) == indices.end();
        }

        /**
         * @brief Получает индексы семейств очередей для указанных групп
         * @details Поскольку на несколько групп может приходиться одно семейство, может быть меньше чем групп
         * @param group_indices Индексы групп
         * @return Массив индексов очередей
         */
        [[nodiscard]] std::vector<uint32_t> queue_family_indices(const std::vector<size_t>& group_indices) const {
            if(group_indices.empty()) return {};
            std::set<uint32_t> unique_indices;
            for (size_t i : group_indices) {
                if (i >= queue_groups_.size()) {
                    continue;
                }
                if (queue_groups_[i].family_index.has_value()) {
                    unique_indices.insert(queue_groups_[i].family_index.value());
                }
            }
            return {unique_indices.begin(), unique_indices.end()};
        }

        /**
         * @brief Возвращает физическое устройство
         * @return Константная ссылка на физическое устройство
         */
        [[nodiscard]] const vk::PhysicalDevice& physical_device() const {
            return physical_device_;
        }

        /**
         * @brief Возвращает логическое устройство
         * @return Ссылка на логическое устройство
         */
        [[nodiscard]] vk::Device& logical_device() {
            return device_.get();
        }

        /**
         * @brief Возвращает все группы очередей
         * @return Ссылка на вектор групп очередей
         */
        [[nodiscard]] std::vector<QueueGroup>& queue_groups() {
            return queue_groups_;
        }

        /**
         * @brief Возвращает группу очередей по индексу
         * @param index Индекс группы
         * @return Ссылка на группу очередей
         */
        [[nodiscard]] QueueGroup& queue_group(const size_t index) {
            assert(index < queue_groups_.size());
            return queue_groups_[index];
        }

        /**
         * @brief Проверяет поддержку расширения устройства
         * @param extension Имя проверяемого расширения
         * @return True, если расширение поддерживается
         */
        [[nodiscard]] bool supports_extension(const char* extension) const {
            assert(physical_device_);
            const auto extensions = physical_device_.enumerateDeviceExtensionProperties();
            return std::any_of(extensions.begin(), extensions.end(),
                [extension](const auto& ext) {
                    return strcmp(ext.extensionName, extension) == 0;
                });
        }

    private:
        /**
         * @brief Выбирает подходящее физическое устройство
         * @param instance Экземпляр Vulkan
         * @param surface Поверхность для отображения
         * @param req_queue_groups Требуемые группы очередей
         * @param req_extensions Требуемые расширения
         * @param allow_integrated_device Разрешить интегрированное GPU
         * @throw std::runtime_error Если не найдено подходящее устройство
         */
        void pick_physical_device(const vk::UniqueInstance& instance,
                                  const vk::UniqueSurfaceKHR& surface,
                                  const std::vector<QueueGroupRequest>& req_queue_groups,
                                  const std::vector<const char*>& req_extensions,
                                  bool allow_integrated_device)
        {
            // Получить список физических устройств
            const auto physical_devices = instance->enumeratePhysicalDevices();
            if (physical_devices.empty()){
                throw std::runtime_error("Cannot find GPUs with Vulkan support");
            }

            // Предварительно заполнить список групп очередей
            queue_groups_.resize(req_queue_groups.size());

            // Пройтись по всем устройствам и проверить на соответствие требованиям, выбрать одно.
            for (const auto& device : physical_devices)
            {
                // Сбросить все индексы семейств, если какие-то установлены
                for (size_t i = 0; i < req_queue_groups.size(); ++i){
                    queue_groups_[i].family_index = std::nullopt;
                }

                // Кол-во использований каждого семейства
                std::map<uint32_t, uint32_t> family_usage_count;

                // TODO: Улучшить алгоритм выбора семейств учитывая запрашиваемое кол-во очередей

                // Найти нужные семейства очередей, учитывая запросы (req_queue_groups) и доступные семейства
                for(size_t i = 0; i < req_queue_groups.size(); ++i){
                    auto available = device.getQueueFamilyProperties();
                    auto suitable = std::vector<uint32_t>();

                    for (size_t family_index = 0; family_index < available.size(); ++family_index)
                    {
                        // Проверка поддержки представления
                        if(req_queue_groups[i].require_present){
                            vk::Bool32 support = false;
                            vk::Result checked = device.getSurfaceSupportKHR(
                                    static_cast<uint32_t>(family_index),
                                    surface.get(),
                                    &support);

                            if (!(checked == vk::Result::eSuccess && support == VK_TRUE)) {
                                continue;
                            }
                        }

                        // Проверка поддержки требуемых типов команд.
                        // Если команды поддерживаются - добавляем семейство в список подходящих
                        if(req_queue_groups[i].queue_flags & available[family_index].queueFlags){
                            suitable.push_back(static_cast<uint32_t>(family_index));
                        }
                    }

                    // Выбрать из подходящих семейств то, которое использовалось меньшее кол-во раз
                    if(!suitable.empty()){
                        // Отсортировать подходящие очереди по возрастанию количества использований
                        std::sort(suitable.begin(), suitable.end(),
                            [&family_usage_count](const uint32_t a, const uint32_t b) {
                                const uint32_t count_a = family_usage_count.count(a) ? family_usage_count[a] : 0;
                                const uint32_t count_b = family_usage_count.count(b) ? family_usage_count[b] : 0;
                                return count_a < count_b;
                            });

                        // Взять первый элемент (с наименьшим количеством использований)
                        queue_groups_[i].family_index = suitable[0];

                        // Увеличить счетчик использований для выбранного семейства
                        family_usage_count[suitable[0]]++;
                    }
                }

                // Пропустить устройство без поддержки необходимых команд
                if(std::any_of(queue_groups_.cbegin(),
                               queue_groups_.cend(),
                               [](const QueueGroup& qg){ return !qg.family_index.has_value(); }))
                {
                    continue;
                }

                // Пропустить устройство без поддержки необходимых расширений
                auto supports_ext = [&](const char* extension){
                    const auto extensions = device.enumerateDeviceExtensionProperties();
                    return std::any_of(extensions.begin(), extensions.end(),
                        [extension](const auto& ext) {
                            return strcmp(ext.extensionName, extension) == 0;
                        });
                };

                if (!std::all_of(req_extensions.begin(),
                    req_extensions.end(),
                    [this, supports_ext](const char* required_ext){return supports_ext(required_ext);}))
                {
                    continue;
                }

                // Пропустить устройство без поддержки необходимых особенностей
                auto features = device.getFeatures();
                if (!features.samplerAnisotropy || !features.geometryShader || !features.multiViewport) {
                    continue;
                }

                // Пропустить интегрированное устройство графики (если требуется исключить)
                if(!allow_integrated_device && device.getProperties().deviceType != vk::PhysicalDeviceType::eDiscreteGpu){
                    continue;
                }

                // Пропустить устройство без поддержки требуемой поверхности
                const auto formats = device.getSurfaceFormatsKHR(surface.get());
                const auto present_modes = device.getSurfacePresentModesKHR(surface.get());
                if (formats.empty() || present_modes.empty())
                {
                    continue;
                }

                // Выбрать устройство и остановить поиск
                physical_device_ = device;
                break;
            }

            // Если не было найдено подходящего устройства
            if(!physical_device_){
                throw std::runtime_error("Cannot find suitable GPU");
            }
        }

        /**
         * @brief Инициализирует логическое устройство
         * @param req_queue_groups Требуемые группы очередей
         * @param req_extensions Требуемые расширения
         * @throw std::runtime_error Если не удалось создать устройство
         */
        void init_logical_device(const std::vector<QueueGroupRequest>& req_queue_groups,
                                 const std::vector<const char*>& req_extensions)
        {
            // Проверить были ли получены семейства очередей для всех запрашиваемых групп
            for(size_t i = 0; i < req_queue_groups.size(); ++i){
                if(!queue_groups_[i].family_index.has_value()){
                    throw std::runtime_error("Some queue groups are not supported");
                }
            }

            // Подготовить информацию для выделения очередей
            std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
            for(size_t i = 0; i < req_queue_groups.size(); ++i)
            {
                auto& qg = queue_groups_[i];
                const auto& family_index = qg.family_index.value();

                // TODO: Вынести приоритеты в QueueGroupRequest
                auto priorities = std::vector(req_queue_groups[i].queue_count, 1.0f);

                queue_create_infos.emplace_back(
                        vk::DeviceQueueCreateInfo()
                        .setQueueFamilyIndex(family_index)
                        .setQueueCount(req_queue_groups[i].queue_count)
                        .setQueuePriorities(priorities));
            }

            // Особенности
            auto features = vk::PhysicalDeviceFeatures()
                    .setSamplerAnisotropy(true)
                    .setGeometryShader(true)
                    .setMultiViewport(true);

            // Создать устройство
            device_ = physical_device_.createDeviceUnique(
                    vk::DeviceCreateInfo()
                    .setQueueCreateInfos(queue_create_infos)
                    .setPEnabledExtensionNames(req_extensions)
                    .setPEnabledFeatures(&features));

            // Получить очереди и создать пулы для каждой группы
            for(size_t i = 0; i < req_queue_groups.size(); ++i)
            {
                auto& qg = queue_groups_[i];
                auto& family_index = qg.family_index.value();
                auto& queues = qg.queues;
                auto& command_pools = qg.command_pools;

                for(uint32_t q = 0; q < req_queue_groups[i].queue_count; ++q){
                    queues.emplace_back(device_->getQueue(family_index, q));
                }

                for(uint32_t p = 0; p < req_queue_groups[i].pool_count; ++p){
                    command_pools.emplace_back(device_->createCommandPoolUnique(
                            vk::CommandPoolCreateInfo()
                            .setQueueFamilyIndex(family_index)
                            .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)));
                }
            }
        }

    protected:
        /// Физическое устройство
        vk::PhysicalDevice physical_device_;
        /// Логическое устройство
        vk::UniqueDevice device_;
        /// Группы очередей
        std::vector<QueueGroup> queue_groups_;
    };
}