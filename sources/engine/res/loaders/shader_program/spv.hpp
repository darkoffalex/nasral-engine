#pragma once

#include <fstream>
#include <nasral/res/objects/shader_program.h>
#include <nasral/res/manager.h>
#include <spirv-headers/spirv.hpp>

namespace nasral::res
{
    class ShaderProgramSpvLoader final : public ShaderProgram::Loader<ShaderProgram::Data>
    {
    public:
        explicit ShaderProgramSpvLoader(Manager* manager, const std::optional<LoadParams>& params = std::nullopt) : Loader(manager, params)
        {}

        std::optional<ShaderProgram::Data> load(const std::string_view& path) override
        {
            std::ifstream file{path.data(), std::ios::binary | std::ios::ate};
            if (!file.is_open()){
                set_error(Error::eCannotOpenFile);
                return std::nullopt;
            }
            
            const auto size = static_cast<std::size_t>(file.seekg(0, std::ios::end).tellg());
            if (size == 0 || size % sizeof(std::uint32_t) != 0){
                set_error(Error::eBadFormat);
                return std::nullopt;
            }

            std::vector<std::uint32_t> shader_code(size / 4);
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(shader_code.data()), static_cast<std::streamsize>(size));
            file.close();

            // Определить тип шейдерной программы (учитывая параметры загрузки)
            auto type = gfx::ShaderProgramType::eRasterization;
            if (auto* lp = params<ShaderLoadParams>()){
                constexpr EnumArray<ShaderLoadParams::Type, gfx::ShaderProgramType> type_map{
                    gfx::ShaderProgramType::eRasterization,
                    gfx::ShaderProgramType::eScreenPass,
                    gfx::ShaderProgramType::eRaytracing,
                    gfx::ShaderProgramType::eCompute,
                };
                type = type_map[lp->type];
            }

            set_error(Error::eNone);
            return std::optional{ShaderProgram::Data{
                shader_code,
                type,
                parse_spv_entry_points(shader_code)
            }};
        }

        static std::vector<ShaderProgram::Data::EntryPointInfo> parse_spv_entry_points(const std::vector<uint32_t>& spv_code)
        {
            std::vector<ShaderProgram::Data::EntryPointInfo> entry_points;
            if (spv_code.size() < 5) {
                return entry_points;
            }

            // Проверка сигнатуры SPIR-V
            if (spv_code[0] != spv::MagicNumber) {
                return entry_points;
            }

            // Пропускаем заголовок SPIR-V (5 слов: Magic, Version, Generator, Bound, Schema)
            size_t offset = 5;

            while (offset < spv_code.size())
            {
                const uint32_t instruction = spv_code[offset];
                const auto op_code = static_cast<spv::Op>(instruction & spv::OpCodeMask);
                const auto word_count = static_cast<uint16_t>(instruction >> spv::WordCountShift);

                // Защита от поврежденного файла или бесконечного цикла
                if (word_count == 0 || offset + word_count > spv_code.size()) {
                    break;
                }

                if (op_code == spv::Op::OpEntryPoint && word_count >= 4)
                {
                    // Формат OpEntryPoint:
                    // Word 0: [ WordCount | OpEntryPoint ]
                    // Word 1: ExecutionModel
                    // Word 2: EntryPoint ID (ID функции)
                    // Word 3..N: Name (null-terminated UTF-8 строка)
                    // Word N+1..end: Interface IDs
                    const auto execution_model = static_cast<spv::ExecutionModel>(spv_code[offset + 1]);
                    vk::ShaderStageFlagBits stage{};

                    switch (execution_model)
                    {
                        case spv::ExecutionModel::ExecutionModelVertex:
                            stage = vk::ShaderStageFlagBits::eVertex;
                            break;
                        case spv::ExecutionModel::ExecutionModelTessellationControl:
                            stage = vk::ShaderStageFlagBits::eTessellationControl;
                            break;
                        case spv::ExecutionModel::ExecutionModelTessellationEvaluation:
                            stage = vk::ShaderStageFlagBits::eTessellationEvaluation;
                            break;
                        case spv::ExecutionModel::ExecutionModelGeometry:
                            stage = vk::ShaderStageFlagBits::eGeometry;
                            break;
                        case spv::ExecutionModel::ExecutionModelFragment:
                            stage = vk::ShaderStageFlagBits::eFragment;
                            break;
                        case spv::ExecutionModel::ExecutionModelGLCompute:
                            stage = vk::ShaderStageFlagBits::eCompute;
                            break;
                        case spv::ExecutionModel::ExecutionModelTaskEXT:
                            stage = vk::ShaderStageFlagBits::eTaskEXT;
                            break;
                        case spv::ExecutionModel::ExecutionModelMeshEXT:
                            stage = vk::ShaderStageFlagBits::eMeshEXT;
                            break;
                        case spv::ExecutionModel::ExecutionModelRayGenerationKHR:
                            stage = vk::ShaderStageFlagBits::eRaygenKHR;
                            break;
                        case spv::ExecutionModel::ExecutionModelIntersectionKHR:
                            stage = vk::ShaderStageFlagBits::eIntersectionKHR;
                            break;
                        case spv::ExecutionModel::ExecutionModelAnyHitKHR:
                            stage = vk::ShaderStageFlagBits::eAnyHitKHR;
                            break;
                        case spv::ExecutionModel::ExecutionModelClosestHitKHR:
                            stage = vk::ShaderStageFlagBits::eClosestHitKHR;
                            break;
                        case spv::ExecutionModel::ExecutionModelMissKHR:
                            stage = vk::ShaderStageFlagBits::eMissKHR;
                            break;
                        case spv::ExecutionModel::ExecutionModelCallableKHR:
                            stage = vk::ShaderStageFlagBits::eCallableKHR;
                            break;
                        default:
                            break;
                    }

                    // Безопасно получаем строку (гарантируем, что не выйдем за пределы инструкции)
                    const auto name_ptr = reinterpret_cast<const char*>(&spv_code[offset + 3]);
                    const size_t max_name_len = (word_count - 3) * sizeof(uint32_t);
                    std::string entry_name(name_ptr, strnlen(name_ptr, max_name_len));
                    entry_points.push_back({ stage, std::move(entry_name) });
                }
                // Оптимизация: когда начинаются типы/константы или функции,
                // секция OpEntryPoint гарантированно завершена по спецификации Logical Layout
                else if (op_code == spv::Op::OpTypeVoid || op_code == spv::Op::OpFunction)
                {
                    break;
                }

                offset += word_count;
            }

            return entry_points;
        }
    };
}