#pragma once
#include <nasral/resources/loaders/loader.h>
#include <nasral/resources/material.h>

namespace nasral::resources
{
    class MaterialLoader final : public Loader<Material::Data>
    {
    public:
        std::optional<Material::Data> load(const std::string_view& path) override;
    };
}