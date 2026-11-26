---
name: gfx-structure
description: Архитектура графической подсистемы (gfx) движка на Vulkan (vulkan.hpp) и Slang. Используй при работе с gfx::Manager, gfx::Renderer, ECS-системой рендеринга, материалами, экранными проходами, storage/uniform буферами и пулами индексов, вложениями кадра, max_frames_in_flight, шейдерами или цепочкой рендеринга.
---

# Архитектура графической подсистемы

## Ключевые принципы и каталоги
- Графическая подсистема построена на **Vulkan API (`vulkan.hpp`)** и кастомных RAII-обёртках из `include/vulkan/utils` (`buffer.hpp`, `device.hpp`, `image.hpp`, `uniform_layout.hpp`).
- Код подсистемы находится в `include/nasral/gfx` и `sources/engine/gfx`; сущности сцены и графики — в `scn/entities/` (`mesh.h`, `light.h`) и `gfx/entities/` (`material.h`, `screen_pass.h`).
- Ресурсы рендеринга лежат в `include/nasral/res/objects/`: `shader_program.h` (`vk::Pipeline`), `texture.h` (`vk::Image`), `mesh.h` (буферы геометрии и индексов).
- Шейдеры пишутся на **Slang** (`content/shaders`) и компилируются в SPIR-V.
- Данные объектов, источников света, материалов и экранных проходов хранятся в массивах / storage-буферах сцены и адресуются через индексы из пулов, которые выделяются при создании сущностей и возвращаются при удалении.
- Ресурсы, зависящие от параллельной работы CPU/GPU (кадровые вложения, командные буферы, часть дескрипторных наборов), создаются по числу `max_frames_in_flight`.

## Компоненты подсистемы
- **`gfx::Manager` (`include/nasral/gfx/manager.h`, `sources/engine/gfx/manager.cpp`)**:
    - Владеет пулами индексов (`object_ubo_ids_`, `light_ubo_ids_`, `material_ubo_ids_`, `screen_pass_ubo_ids_`).
    - Создаёт и удерживает время жизни `gfx::Renderer` и ECS-системы рендеринга (`gfx::System`).
    - Предоставляет методы регистрации материалов/экранных проходов (`add_material`, `add_screen_pass` и др.) и обновления uniform-данных (`update_cam_uniforms`, `update_obj_uniforms`, `update_mat_uniforms` и др.).
    - Реагирует на `on_res_registry_changed` (формирует списки материалов и экранных проходов) и `on_display_surface_changed` (уведомляет рендерер о смене поверхности).
- **`gfx::Renderer` (`include/nasral/gfx/renderer.h`, `renderer_defs.h`, `sources/engine/gfx/renderer.cpp`, `renderer_init.cpp`)**:
    - Методы `init_*` в `renderer_init.cpp` (вызываются из конструктора в заданном порядке) создают вложения кадра (`init_vk_framebuffers`), `UniformLayout` с пулом/лейаутами дескрипторов и pipeline layout (`init_vk_uniform_layouts`), буферы и привязки дескрипторных наборов (`init_vk_uniforms`, `init_vk_framebuffer_bindings`).
    - Методы `cmd_*` в `renderer.cpp` реализуют запись команд кадра (`cmd_begin_frame`, проходы `cmd_begin_*_pass` / `cmd_end_*_pass`, команды отрисовки, `cmd_blit_to_screen`, `cmd_end_frame`).
- **ECS-система рендеринга (`include/nasral/gfx/system.h` / `systems.h`, `.cpp`)**:
    - Работает с ECS-компонентами графических сущностей и в `on_render` запускает проходы рендерера.

## От загрузки ресурсов до рендеринга кадра
1. `content/project.json` и сцены (`content/scenes/*.json`) загружаются через подсистему ресурсов и сериализаторы (`sources/engine/res/serializers/json_scn.hpp`, `json_gfx.hpp`) в промежуточные `Desc`-структуры (`MaterialDesc`, `ScreenPassDesc`).
2. После загрузки проекта и обновления реестра ресурсов (`eProjectFileLoaded` → `eResourceRegistryChanged`) `gfx::Manager` добавляет материалы и экранные проходы (`MaterialView::setup`, `ScreenPassView::setup`).
3. При создании вложений учитываются их назначение и разрешение (`rendering_resolution`, `screen_fx_resolution`).
4. В `gfx::System::on_render` сначала идут фиксированные проходы (например, растеризация), затем активные экранные проходы в порядке `previous`/`next` из конфигурации проекта. Ожидается, что финальный экранный проход пишет в `eCompositing`, после чего `cmd_blit_to_screen` копирует результат в swapchain.

## На что обращать внимание при изменениях
- При добавлении новых проходов, типов вложений, материалов/проходов или дескрипторов проверяй согласованность `types.h`, `renderer_init.cpp` (инициализация и связки), `renderer.cpp` (`cmd_*` и синхронизация по `max_frames_in_flight`) и `gfx::System::on_render`.