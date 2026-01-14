#include "pch.h"
#include <nasral/engine.h>

namespace nasral
{
    Engine::Engine(const Config& config)
    {
        try
        {
            /* Engine subsystems */

            logger_ = std::make_unique<log::Logger>(this, config.log);
            logger()->info("Logger initialized.");

            evt_ = std::make_unique<evt::Manager>(this);
            logger()->info("Event manager initialized.");

            ecs_ = std::make_unique<ecs::Manager>(this, config.ecs);
            logger()->info("ECS manager initialized.");

            renderer_ = std::make_unique<gfx::Renderer>(this, config.gfx);
            logger()->info("Renderer initialized.");

            res_ = std::make_unique<res::Manager>(this, config.res);
            logger()->info("Resource manager initialized.");

            scn_ = std::make_unique<scn::Manager>(this, config.scn);
            logger()->info("Scene manager initialized.");

            /* E C S */

            res_system_ = std::make_unique<res::System>(this);
            res_system_->init();
            logger()->info("ECS: Resource system initialized.");

            gfx_system_ = std::make_unique<gfx::System>(this);
            gfx_system_->init();
            logger()->info("ECS: GFX system initialized.");

            scn_system_ = std::make_unique<scn::System>(this);
            scn_system_->init();
            logger()->info("ECS: Scene system initialized.");
        }
        catch (const std::runtime_error& e){
            if (logger_) logger()->fatal(e.what());
            throw;
        }
    }

    Engine::~Engine()
    {
        assert(ecs_ != nullptr);
        assert(res_ != nullptr);
        assert(renderer_ != nullptr);
        assert(logger_ != nullptr);

        assert(res_system_ != nullptr);
        assert(gfx_system_ != nullptr);

        /* E C S */

        scn_system_.reset();
        logger()->info("ECS: Scene system destroyed.");

        res_system_.reset();
        logger()->info("ECS: Resource system destroyed.");

        gfx_system_.reset();
        logger()->info("ECS: GFX system destroyed.");

        /* Engine subsystems */

        scn_.reset();
        logger()->info("Scene manager destroyed.");

        res_.reset();
        logger()->info("Resource manager destroyed.");

        renderer_.reset();
        logger()->info("Renderer destroyed.");

        ecs_.reset();
        logger()->info("ECS manager destroyed.");

        evt_.reset();
        logger()->info("Event manager destroyed.");

        logger_.reset();
    }

    void Engine::finalize() const
    {
        logger()->info("Finalizing...");

        // Заключительные операции ECS систем
        scn_system_->shutdown();
        gfx_system_->shutdown();
        res_system_->shutdown();

        // Выполнить отложенные действия (после заключительных ECS операций)
        evt_->apply_deferred_actions();
        ecs_->apply_deferred_actions();

        // Последний loop ECS систем
        scn_system_->update(0.0f);
        res_system_->update(0.0f);
        gfx_system_->update(0.0f);

        // Подождать завершения кадра
        renderer_->cmd_wait_for_frame();

        // Заключительная обработка ресурсов
        res_->finalize();

        // Выполнить отложенные действия (завершение)
        evt_->apply_deferred_actions();
        ecs_->apply_deferred_actions();
    }

    void Engine::update(const float delta)
    {
        assert(ecs_ != nullptr);
        assert(res_ != nullptr);
        assert(renderer_ != nullptr);
        assert(logger_ != nullptr);

        assert(res_system_ != nullptr);
        assert(gfx_system_ != nullptr);

        // Обновление систем ECS
        scn_system_->update(delta);
        res_system_->update(delta);
        gfx_system_->update(delta);

        // Выполнение отложенных действий
        ecs_->apply_deferred_actions();
        evt_->apply_deferred_actions();

        // Загрузка/выгрузка ресурсов
        res_->update();

        // Рендеринг
        renderer_->cmd_begin_frame();
        renderer_->cmd_bind_frame_descriptors();
        gfx_system_->render();
        renderer_->cmd_end_frame();
    }
}
