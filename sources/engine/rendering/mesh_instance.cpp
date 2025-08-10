#include "pch.h"
#include <nasral/rendering/mesh_instance.h>
#include <nasral/resources/resource_manager.h>
#include <nasral/resources/mesh.h>

namespace nasral::rendering
{
    MeshInstance::MeshInstance(
        resources::ResourceManager* manager,
        const std::string& mesh_path)
    : mesh_ref_(manager, resources::Type::eMesh, mesh_path)
    {
        mesh_ref_.set_callback([this](resources::IResource* resource){
            const auto* mesh = dynamic_cast<resources::Mesh*>(resource);
            if (mesh && resource->status() == resources::Status::eLoaded){
                mesh_handles_ = mesh->render_handles();
                mark_changed(eMeshChanged);
            }
        });
    }

    MeshInstance::MeshInstance(MeshInstance&& other) noexcept
    : mesh_ref_(
        other.mesh_ref_.manager().get(),
        other.mesh_ref_.type(),
        std::string(other.mesh_ref_.path().data()))
    , mesh_handles_({})
    {
        other.unbind_callbacks();
        other.release_resources();
        bind_callbacks();
    }

    MeshInstance& MeshInstance::operator=(MeshInstance&& other) noexcept
    {
        if (this == &other) return *this;

        unbind_callbacks();
        release_resources();

        mesh_ref_ = static_cast<const resources::Ref&>(resources::Ref(
            other.mesh_ref_.manager().get(),
            other.mesh_ref_.type(),
            std::string(other.mesh_ref_.path().data())));

        mesh_handles_ = {};

        other.unbind_callbacks();
        other.release_resources();
        bind_callbacks();
        return *this;
    }

    void MeshInstance::set_mesh(const std::string& path, const bool request){
        mesh_handles_ = {};
        mesh_ref_.release();
        mark_changed(eMeshChanged);

        mesh_ref_.set_path(path);
        if (request){
            mesh_ref_.request();
        }
    }

    void MeshInstance::request_resources(){
        mesh_ref_.request();
    }

    void MeshInstance::release_resources(){
        mesh_handles_ = {};
        mesh_ref_.release();
    }

    const Handles::Mesh& MeshInstance::mesh_render_handles() const{
        return mesh_handles_;
    }

    void MeshInstance::bind_callbacks(){
        mesh_ref_.set_callback([this](resources::IResource* resource){
            const auto* mesh = dynamic_cast<resources::Mesh*>(resource);
            if (mesh && resource->status() == resources::Status::eLoaded){
                mesh_handles_ = mesh->render_handles();
                mark_changed(eMeshChanged);
            }
        });
    }

    void MeshInstance::unbind_callbacks(){
        mesh_ref_.set_callback(nullptr);
    }
}
