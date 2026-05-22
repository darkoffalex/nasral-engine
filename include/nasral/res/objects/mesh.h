#pragma once

#include <nasral/res/objects/resource.h>
#include <nasral/gfx/types.h>
#include <vulkan/utils/buffer.hpp>

namespace nasral::res
{
    class Mesh final : public Resource
    {
    public:
        typedef std::unique_ptr<Mesh> Ptr;
        typedef gfx::handles::Mesh::Surface Surface;

        struct Data
        {
            std::vector<gfx::Vertex> vertices = {};
            std::vector<uint32_t> indices = {};
            std::vector<Surface> materials = {};
        };

        Mesh(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~Mesh() override;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        void load() noexcept override;

        [[nodiscard]] const auto& vk_vertex_buffer() const noexcept { return vertex_buffer_->vk_buffer(); }
        [[nodiscard]] const auto& vk_index_buffer() const noexcept { return index_buffer_->vk_buffer(); }
        [[nodiscard]] auto vertex_count() const noexcept { return vertex_count_; }
        [[nodiscard]] auto index_count() const noexcept { return index_count_; }
        [[nodiscard]] const auto& surfaces() const noexcept { return surfaces_; }

        [[nodiscard]] gfx::handles::Mesh render_handles() const;

    private:
        Loader<Data>::Ptr loader_;
        vk::utils::Buffer::Ptr vertex_buffer_;
        vk::utils::Buffer::Ptr index_buffer_;
        uint32_t vertex_count_;
        uint32_t index_count_;
        std::vector<Surface> surfaces_ = {};
    };
}