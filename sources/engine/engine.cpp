#include "pch.h"
#include <nasral/engine.h>

namespace nasral
{
    Engine::Engine(const Config& config)
    {
        try
        {
            std::apply([&](auto&&... systems) {
                (..., (void)[&](auto& s) {
                    using SubsystemPtr = std::decay_t<decltype(s)>;

                    if constexpr (std::is_same_v<SubsystemPtr, log::Logger::Ptr>) {
                        s = std::make_unique<log::Logger>(this, config.log);
                    } else if constexpr (std::is_same_v<SubsystemPtr, evt::Manager::Ptr>) {
                        s = std::make_unique<evt::Manager>(this);
                    } else if constexpr (std::is_same_v<SubsystemPtr, run::Manager::Ptr>) {
                        s = std::make_unique<run::Manager>(this, config.run);
                    } else if constexpr (std::is_same_v<SubsystemPtr, ecs::Manager::Ptr>) {
                        s = std::make_unique<ecs::Manager>(this, config.ecs);
                    } else if constexpr (std::is_same_v<SubsystemPtr, gfx::Manager::Ptr>) {
                        s = std::make_unique<gfx::Manager>(this, config.gfx);
                    } else if constexpr (std::is_same_v<SubsystemPtr, res::Manager::Ptr>) {
                        s = std::make_unique<res::Manager>(this, config.res);
                    } else if constexpr (std::is_same_v<SubsystemPtr, scn::Manager::Ptr>) {
                        s = std::make_unique<scn::Manager>(this, config.scn);
                    }
                    if (s) {
                        s->init();
                    }
                }(systems));
            }, subsystems_);
        }
        catch (const std::runtime_error& e){
            if (logger()) logger()->fatal(e.what());
            throw;
        }
    }

    Engine::~Engine()
    {
        // Уничтожение подсистем в обратном порядке
        apply_reverse([&](auto&&... systems) {
            (..., (void)[&](auto& s) {
                s.reset();
            }(systems));
        }, subsystems_);
    }

    void Engine::finalize() const
    {
        // Дождаться завершения графических команд
        gfx()->renderer()->cmd_wait_for_all();

        // Для всех подсистем выполнять:
        // - Финализация подсистемы
        // - Отложенные действия подсистемы
        // - (Опционально) Синхронизация ECS, если текущая система не является ECS.
        apply_reverse([&](auto&&... systems){
            (..., (void)[&](auto* s) {
                using SubsystemType = std::remove_pointer_t<decltype(s)>;

                if (kDebugBuild){
                    assert(s && "Subsystem pointer is null");
                }
                s->finalize();
                s->apply_deferred();

                if constexpr (!std::is_same_v<SubsystemType, ecs::Manager>){
                    ecs()->apply_deferred();
                }
            }(systems.get()));
        }, subsystems_);
    }

    void Engine::update([[maybe_unused]] const float delta)
    {
        // Для всех подсистем выполнять:
        // - Update
        // - Отложенные действия подсистемы
        // - (Опционально) Синхронизация ECS, если текущая система не является ECS.
        std::apply([&](auto&&... systems) {
            (..., (void)[&](auto* s) {
                using SubsystemType = std::remove_pointer_t<decltype(s)>;

                if (kDebugBuild){
                    assert(s && "Subsystem pointer is null");
                }

                s->update(delta);
                s->apply_deferred();

                if constexpr (!std::is_same_v<SubsystemType, ecs::Manager>){
                    ecs()->apply_deferred();
                }
            }(systems.get()));
        }, subsystems_);

        // Рендеринг
        gfx()->render();
    }

    log::Logger* Engine::logger() const noexcept{
        return std::get<log::Logger::Ptr>(subsystems_).get();
    }

    evt::Manager* Engine::events() const noexcept{
        return std::get<evt::Manager::Ptr>(subsystems_).get();
    }

    ecs::Manager* Engine::ecs() const noexcept{
        return std::get<ecs::Manager::Ptr>(subsystems_).get();
    }

    res::Manager* Engine::res() const noexcept{
        return std::get<res::Manager::Ptr>(subsystems_).get();
    }

    gfx::Manager* Engine::gfx() const noexcept{
        return std::get<gfx::Manager::Ptr>(subsystems_).get();
    }

    run::Manager* Engine::run() const noexcept{
        return std::get<run::Manager::Ptr>(subsystems_).get();
    }

    scn::Manager* Engine::scn() const noexcept{
        return std::get<scn::Manager::Ptr>(subsystems_).get();
    }
}
