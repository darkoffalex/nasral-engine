#include "pch.h"
#include <nasral/rendering/material_instance.h>
#include <nasral/resources/resource_manager.h>
#include <nasral/resources/material.h>
#include <nasral/resources/texture.h>

namespace nasral::rendering
{
    MaterialInstance::MaterialInstance(
        resources::ResourceManager* manager,
        const MaterialType type,
        const std::string& mat_path,
        const std::vector<std::string>& tex_paths)
    : material_type_(type)
    , material_ref_(manager, resources::Type::eMaterial, mat_path)
    {
        for (size_t i = 0; i < static_cast<size_t>(TextureType::TOTAL); ++i)
        {
            const auto builtin_tex = builtin_tex_for_type(static_cast<TextureType>(i));
            std::string path = resources::builtin_res_path(builtin_tex);

            if (i < tex_paths.size() && !tex_paths[i].empty()){
                path = tex_paths[i];
            }

            texture_refs_[i] = manager->make_ref(
                resources::Type::eTexture,
                path);
        }

        bind_callbacks();
    }

    MaterialInstance::MaterialInstance(MaterialInstance&& other) noexcept
    : material_type_(std::exchange(other.material_type_, MaterialType::eDummy))
    , material_ref_(
        other.material_ref_.manager().get(),
        other.material_ref_.type(),
        std::string(other.material_ref_.path().data()))
    , material_handles_({})
    {
        for (size_t i = 0; i < static_cast<size_t>(TextureType::TOTAL); ++i) {
            texture_refs_[i] = other.texture_refs_[i];
            texture_handles_[i] = {};
        }

        settings_ = other.settings_;

        other.unbind_callbacks();
        other.release_resources();
        bind_callbacks();
    }


    MaterialInstance& MaterialInstance::operator=(MaterialInstance&& other) noexcept
    {
        if (this == &other) return *this;

        unbind_callbacks();
        release_resources();

        material_type_ = std::exchange(other.material_type_, MaterialType::eDummy);
        material_ref_ = resources::Ref(
            other.material_ref_.manager().get(),
            other.material_ref_.type(),
            std::string(other.material_ref_.path().data()));

        material_handles_ = {};

        for (size_t i = 0; i < static_cast<size_t>(TextureType::TOTAL); ++i) {
            texture_refs_[i] = other.texture_refs_[i];
            texture_handles_[i] = {};
        }

        settings_ = other.settings_;

        other.unbind_callbacks();
        other.release_resources();
        bind_callbacks();
        return *this;
    }

    void MaterialInstance::set_material(const MaterialType type, const std::string& path, const bool request){
        material_handles_ = {};
        material_ref_.release();
        mark_changed(eShadersChanged);

        material_type_ = type;
        material_ref_.set_path(path);
        if (request){
            material_ref_.request();
        }
    }

    void MaterialInstance::set_texture(TextureType type, const std::string& path, const bool request){
        auto& ref = texture_refs_[static_cast<uint32_t>(type)];
        auto& handle = texture_handles_[static_cast<uint32_t>(type)];
        handle = {};
        ref.release();
        mark_changed(eTextureChanged);

        if (path.empty()){
            ref.set_path(resources::builtin_res_path(builtin_tex_for_type(type)));
        }else{
            ref.set_path(path);
        }

        if (request){
            ref.request();
        }
    }

    void MaterialInstance::set_settings(const ObjectMatUniforms& settings){
        if (material_type_ == MaterialType::eDummy
            || material_type_ == MaterialType::eTextured
            || material_type_ == MaterialType::eVertexColored)
        {
            settings_ = std::nullopt;
        }

        settings_ = settings;
        mark_changed(eSettingsChanged);
    }

    void MaterialInstance::request_resources(){
        material_ref_.request();
        for (auto& ref : texture_refs_){
            if (ref.type() == resources::Type::eTexture){
                ref.request();
            }
        }
    }

    void MaterialInstance::release_resources(){
        material_handles_ = {};
        material_ref_.release();

        for (size_t i = 0; i < static_cast<size_t>(TextureType::TOTAL); ++i){
            texture_handles_[i] = {};
            texture_refs_[i].release();
        }

        settings_ = std::nullopt;
    }

    const Handles::Material& MaterialInstance::mat_render_handles() const{
        return material_handles_;
    }

    const Handles::Texture& MaterialInstance::tex_render_handles(const TextureType type) const{
        return texture_handles_[static_cast<uint32_t>(type)];
    }

    const std::optional<ObjectMatUniforms>& MaterialInstance::settings() const{
        return settings_;
    }

    resources::BuiltinResources MaterialInstance::builtin_tex_for_type(const TextureType type){
        switch (type)
        {
        case TextureType::eAlbedoColor:
        case TextureType::eRoughnessOrSpecular:
            return resources::BuiltinResources::eWhitePixel;
        case TextureType::eNormal:
            return resources::BuiltinResources::eNormalPixel;
        case TextureType::eHeight:
        case TextureType::eMetallicOrReflection:
            return resources::BuiltinResources::eBlackPixel;
        default:
            return resources::BuiltinResources::eCheckerboardTexture;
        }
    }

    void MaterialInstance::bind_callbacks(){
        material_ref_.set_callback([this](resources::IResource* resource){
            const auto* material = dynamic_cast<resources::Material*>(resource);
            if (material && resource->status() == resources::Status::eLoaded){
                assert(material->material_type() == material_type_);
                material_handles_ = material->render_handles();
                mark_changed(eShadersChanged);
            }
        });

        for (size_t i = 0; i < static_cast<size_t>(TextureType::TOTAL); ++i){
            texture_refs_[i].set_callback([this, i](resources::IResource* resource){
                const auto* texture = dynamic_cast<resources::Texture*>(resource);
                if (texture && resource->status() == resources::Status::eLoaded){
                    texture_handles_[i] = texture->render_handles();
                    mark_changed(eTextureChanged);
                }
            });
        }
    }

    void MaterialInstance::unbind_callbacks(){
        material_ref_.set_callback(nullptr);
        for (size_t i = 0; i < static_cast<size_t>(TextureType::TOTAL); ++i){
            texture_refs_[i].set_callback(nullptr);
        }
    }
}
