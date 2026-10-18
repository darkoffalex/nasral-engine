#pragma once

#include <fstream>
#include <nasral/res/objects/shader_program.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

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
            file.read(reinterpret_cast<char*>(shader_code.data()), size);
            file.close();

            // Определить тип шейдерной программы (учитывая параметры загрузки)
            auto type = gfx::ShaderProgramType::eRasterization;
            auto screen_fx_pass = gfx::ScreenFxPassType::ePingPong;
            if (auto* lp = params<ShaderLoadParams>()){
                if (lp->screen_space) type = gfx::ShaderProgramType::ePostProcessing;
                if (lp->ray_tracing) type = gfx::ShaderProgramType::eRaytracing;
                if (lp->final_pass) screen_fx_pass = gfx::ScreenFxPassType::eFinal;
            }

            set_error(Error::eNone);
            return std::optional{ShaderProgram::Data{
                shader_code,
                type,
                screen_fx_pass,
                parse_spv_entry_points(shader_code)
            }};
        }

        static std::vector<ShaderProgram::Data::EntryPointInfo> parse_spv_entry_points(const std::vector<uint32_t>& spirv_code)
        {
            std::vector<ShaderProgram::Data::EntryPointInfo> entry_points;
            if (spirv_code.size() < 5) return entry_points;

            size_t offset = 5; // Пропускаем заголовок SPIR-V (Magic, Version, Generator, Bound, Schema)
            while (offset < spirv_code.size())
            {
                const uint32_t instruction = spirv_code[offset];
                const uint16_t op_code = static_cast<uint16_t>(instruction & 0xFFFF);
                const uint16_t word_count = static_cast<uint16_t>(instruction >> 16);

                if (word_count == 0) break;

                // OpEntryPoint == 15
                if (op_code == 15 && offset + 3 < spirv_code.size())
                {
                    const uint32_t execution_model = spirv_code[offset + 1];
                    vk::ShaderStageFlagBits stage{};

                    switch (execution_model)
                    {
                        case 0: stage = vk::ShaderStageFlagBits::eVertex; break;
                        case 3: stage = vk::ShaderStageFlagBits::eGeometry; break;
                        case 4: stage = vk::ShaderStageFlagBits::eFragment; break;
                        case 5: stage = vk::ShaderStageFlagBits::eCompute; break;
                        case 5313: stage = vk::ShaderStageFlagBits::eRaygenKHR; break;
                        case 5315: stage = vk::ShaderStageFlagBits::eMissKHR; break;
                        case 5317: stage = vk::ShaderStageFlagBits::eClosestHitKHR; break;
                        default: break;
                    }

                    const char* name_ptr = reinterpret_cast<const char*>(&spirv_code[offset + 3]);
                    entry_points.push_back({ stage, std::string(name_ptr) });
                }
                else if (op_code > 16)
                {
                    // Все OpEntryPoint идут в самом начале файла
                    break;
                }

                offset += word_count;
            }

            return entry_points;
        }

    };
}