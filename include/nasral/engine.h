#pragma once

#include <nasral/log/logger.h>
#include <nasral/evt/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/res/manager.h>
#include <nasral/gfx/manager.h>
#include <nasral/run/manager.h>
#include <nasral/scn/manager.h>

namespace nasral
{
    struct Config
    {
        log::Config log = {};
        ecs::Config ecs = {};
        res::Config res = {};
        gfx::Config gfx = {};
        run::Config run = {};
        scn::Config scn = {};
    };

    class Engine
    {
    public:
        typedef std::unique_ptr<Engine> Ptr;

        explicit Engine(const Config& config);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        void finalize() const;
        void update(float delta);

        [[nodiscard]] log::Logger* logger() const noexcept;
        [[nodiscard]] evt::Manager* events() const noexcept;
        [[nodiscard]] ecs::Manager* ecs() const noexcept;
        [[nodiscard]] res::Manager* res() const noexcept;
        [[nodiscard]] gfx::Manager* gfx() const noexcept;
        [[nodiscard]] run::Manager* run() const noexcept;
        [[nodiscard]] scn::Manager* scn() const noexcept;

    protected:
        /**
         * @brief Подсистемы
         * @details От порядка подсистем в кортеже зависит порядок
         * инициализации, обновления, финализации.
         * Финализация происходит в порядке, обратном порядку инициализации.
         */
        using SubsystemTuple = std::tuple<
            log::Logger::Ptr,
            evt::Manager::Ptr,
            run::Manager::Ptr,
            ecs::Manager::Ptr,
            gfx::Manager::Ptr,
            res::Manager::Ptr,
            scn::Manager::Ptr>;

        SubsystemTuple subsystems_;
    };
}
