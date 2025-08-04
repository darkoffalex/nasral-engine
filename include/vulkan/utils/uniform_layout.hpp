/**
 * @file device.hpp
 * @brief Обертка для управления uniform объектами шейдеров и конвейера
 * @author Alex "DarkWolf" Nem
 * @date 2025
 * @copyright MIT License
 *
 * Файл содержит реализацию класса UniformLayout, который инкапсулирует работу
 * с дескрипторным пулом, макетами дескрипторных наборов и макетом конвейера
 */

#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/utils/device.hpp>

namespace vk::utils
{
    /**
     * @brief RAII обертка для управления дескрипторным пулом и макетами конвейера.
     *
     * Предоставляет высокоуровневый интерфейс для:
     *  описания размера дескрипторного пула,
     *  описания макетов uniform блоков (макет дескрипторных наборов)
     *  аллокации дескрипторов
     */
    class UniformLayout final
    {
    public:
        typedef std::unique_ptr<UniformLayout> Ptr;

        /**
         * @brief Описание одиночной привязки набора
         */
        struct SetBindingInfo
        {
            /// Индекс привязки в шейдере
            uint32_t binding = 0;
            /// Кол-во потенциально привязываемых дескрипторов в наборе (для массивов дескрипторов)
            uint32_t count = 1;
            /// Тип дескриптора
            vk::DescriptorType type = vk::DescriptorType::eUniformBuffer;
            /// Стадия шейдера
            vk::ShaderStageFlags stage_flags = vk::ShaderStageFlagBits::eVertex;
            /// Флаги привязки (например, для bindless дескрипторов)
            vk::DescriptorBindingFlagsEXT binding_flags = {};
        };

        /**
         * @brief Описание макета набора
         */
        struct SetLayoutInfo
        {
            /// Описание привязок
            std::vector<SetBindingInfo> bindings;
            /// Максимальное кол-во выделяемых наборов
            uint32_t max_sets = 0;
        };

        /** @brief Конструктор по умолчанию */
        UniformLayout()
        : vk_device_(VK_NULL_HANDLE)
        {}

        /**
         * @brief Упрощенный конструктор (для шейдеров без uniform блоков)
         * @param device Устройство Vulkan (обертка из utils)
         */
        explicit UniformLayout(const Device::Ptr& device)
        : vk_device_(device->logical_device())
        {
            assert(vk_device_);
            vk_pipeline_layout_ = vk_device_.createPipelineLayoutUnique(
                vk::PipelineLayoutCreateInfo()
                .setSetLayouts({}));
        }

        /**
         * @brief Основной конструктор
         * @param device Устройство Vulkan (обертка из utils)
         * @param set_layouts Описания макетов дескрипторных наборов
         */
        UniformLayout(const Device::Ptr& device,
                      const std::vector<SetLayoutInfo>& set_layouts)
        : vk_device_(device->logical_device())
        {
            // Дескрипторный пул
            {
                // Общее кол-во наборов (для размеров пула)
                uint32_t max_sets_allocations = 0;

                // Описываем размер дескрипторного пула.
                // На данном этапе нас не интересует структура самих дескрипторных наборов.
                // Важно лишь количество дескрипторов конкретного типа, что можно выделить из пула.
                std::map<vk::DescriptorType, uint32_t> type_max_allocations;
                for (const auto& [bindings, max_sets] : set_layouts){
                    for (const auto& binding : bindings){
                        max_sets_allocations += max_sets;
                        type_max_allocations[binding.type] += (max_sets * binding.count);
                    }
                }

                std::vector<vk::DescriptorPoolSize> pool_sizes;
                pool_sizes.reserve(type_max_allocations.size());
                for (const auto& [type, count] : type_max_allocations){
                    pool_sizes.push_back(vk::DescriptorPoolSize()
                        .setType(type)
                        .setDescriptorCount(count));
                }

                // Создать дескрипторный пул.
                // Учитываем допустимое количество дескрипторов и наборов, состоящих из этих дескрипторов
                vk_descriptor_pool_ = vk_device_.createDescriptorPoolUnique(
                    vk::DescriptorPoolCreateInfo()
                    .setMaxSets(max_sets_allocations)
                    .setPoolSizes(pool_sizes)
                    .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet));
            }

            // Макет конвейера (материала/шейдера)
            {
                // Описываем макеты дескрипторных наборов.
                // Макет очень сильно зависит от кода шейдера и должен его учитывать.
                std::vector<vk::DescriptorSetLayout> vk_layouts;
                for (const auto& set_layout : set_layouts){
                    std::vector<vk::DescriptorSetLayoutBinding> bindings;
                    std::vector<vk::DescriptorBindingFlagsEXT> binding_flags;
                    bindings.reserve(set_layout.bindings.size());
                    binding_flags.reserve(set_layout.bindings.size());

                    for (const auto& binding : set_layout.bindings){
                        binding_flags.push_back(binding.binding_flags);
                        bindings.push_back(vk::DescriptorSetLayoutBinding()
                            .setBinding(binding.binding)
                            .setDescriptorType(binding.type)
                            .setDescriptorCount(binding.count)
                            .setStageFlags(binding.stage_flags));
                    }

                    auto flags_info = vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT()
                        .setBindingFlags(binding_flags)
                        .setPNext(nullptr);

                    vk_descriptor_set_layouts_.emplace_back(
                        vk_device_.createDescriptorSetLayoutUnique(
                        vk::DescriptorSetLayoutCreateInfo()
                        .setBindings(bindings)
                        .setPNext(&flags_info)));

                    vk_layouts.push_back(vk_descriptor_set_layouts_.back().get());
                }

                // Создать макет конвейера
                vk_pipeline_layout_ = vk_device_.createPipelineLayoutUnique(
                    vk::PipelineLayoutCreateInfo()
                    .setSetLayouts(vk_layouts));
            }
        }

        /** @brief Деструктор */
        ~UniformLayout() = default;

        /** @brief Запрет копирования */
        UniformLayout(const UniformLayout& other) = delete;

        /** @brief Запрет присваивания */
        UniformLayout& operator=(const UniformLayout& other) = delete;

        /**
         * @brief Возвращает макет конвейера Vulkan
         * @return Константная ссылка на макет
         */
        [[nodiscard]] const vk::PipelineLayout& vk_pipeline_layout() const{
            return *vk_pipeline_layout_;
        }

        /**
         * @brief Возвращает дескрипторный пул Vulkan
         * @return Константная ссылка на пул
         */
        [[nodiscard]] const vk::DescriptorPool& vk_descriptor_pool() const{
            return *vk_descriptor_pool_;
        }

        /**
         * @brief Возвращает массив макетов наборов дескрипторов Vulkan
         * @return Константная ссылка на вектор
         */
        [[nodiscard]] const std::vector<vk::UniqueDescriptorSetLayout>& vk_descriptor_set_layouts() const{
            return vk_descriptor_set_layouts_;
        }

        /**
         * Выделяет нужное кол-во наборов нужного типа (макета)
         * @param set_layout_index Индекс макета дескрипторного набора
         * @param count Кол-во наборов для выделения
         * @return Массив наборов
         */
        [[nodiscard]] std::vector<vk::UniqueDescriptorSet> allocate_sets(const size_t set_layout_index, const size_t count) const{
            assert(vk_device_);
            assert(vk_descriptor_pool_);
            assert(set_layout_index < vk_descriptor_set_layouts_.size());
            assert(count > 0);

            std::vector set_layouts(count, vk_descriptor_set_layouts_[set_layout_index].get());
            return vk_device_.allocateDescriptorSetsUnique(
                vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(vk_descriptor_pool_.get())
                .setDescriptorSetCount(static_cast<uint32_t>(count))
                .setSetLayouts(set_layouts));
        }

    protected:
        /// Handle-объект логического устройства Vulkan
        vk::Device vk_device_;
        /// Дескрипторный пул
        vk::UniqueDescriptorPool vk_descriptor_pool_;
        /// Макеты дескрипторных наборов
        std::vector<vk::UniqueDescriptorSetLayout> vk_descriptor_set_layouts_;
        /// Макет конвейера Vulkan
        vk::UniquePipelineLayout vk_pipeline_layout_;
    };
}
