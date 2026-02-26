#pragma once

#include <bitset>
#include <nasral/ecs/utils.h>
#include <nasral/gfx/components.h>
#include <nasral/res/components.h>
#include <nasral/scn/components.h>

namespace nasral::ecs
{
    // Tuple из возможных типов компонентов
    using ComponentTypes = std::tuple<
        // Подсистема рендеринга
        gfx::comp::MaterialHandles,
        gfx::comp::MaterialSettings,
        gfx::comp::MeshHandles,
        gfx::comp::UniformIndex,
        gfx::comp::UniformState,
        gfx::comp::DirtyUniform,
        gfx::comp::DirtyTextures,
        gfx::comp::Activate,
        gfx::comp::Deactivate,

        // Подсистема ресурсов
        res::comp::AssetId,
        res::comp::Descriptor,
        res::comp::DescriptorList<gfx::TextureType>,
        res::comp::Request,
        res::comp::Release,
        res::comp::Error,
        res::comp::Loaded,
        res::comp::PendingDelete,

        // Сцена
        scn::comp::Node,
        scn::comp::NodeChildren,
        scn::comp::Spatial,
        scn::comp::Camera,
        scn::comp::Mesh,
        scn::comp::Light
    >;

    // Битовая маска компонентов (размер зависит от кол-ва возможных типов компонентов)
    using ComponentMask = std::bitset<std::tuple_size_v<ComponentTypes>>;

    // Вариант-вектор компонентов
    using ComponentPool = to_variant_vector<ComponentTypes>;

    // Получение ID компонента (соответствует индексу элемента в ComponentTypes)
    template <typename T>
    constexpr size_t kComponentId = tuple_index<T, ComponentTypes>::value;

    // Получение битовой маски типу компонентов
    template<typename... Ts>
    ComponentMask make_mask(){
        ComponentMask mask;
        ((mask.set(kComponentId<Ts>)), ...);
        return mask;
    }

    template<typename... Ts>
    inline const ComponentMask kMaskOf = make_mask<Ts...>();

    struct Config
    {
        size_t max_entities = 1024;
    };
}