#pragma once

#include <vulkan/vulkan.hpp>
#include <nasral/res/resource.h>
#include <nasral/res/loader.h>
#include <nasral/log/loggable.h>
#include <vulkan/utils/buffer.hpp>
#include <nasral/gfx/types.h>

namespace nasral::res
{
    class Mesh final : public IResource, public log::Loggable<Mesh>
    {
    public:
        typedef std::unique_ptr<Mesh> Ptr;

        struct Data
        {
            std::vector<gfx::Vertex> vertices;
            std::vector<uint32_t> indices;
        };

        Mesh(Manager* manager, ResourceId id, Loader<Data>::Ptr loader);
        ~Mesh() override;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        void load() noexcept override;
        [[nodiscard]] const vk::Buffer& vk_vertex_buffer() const { return vertex_buffer_->vk_buffer(); }
        [[nodiscard]] const vk::Buffer& vk_index_buffer() const { return index_buffer_->vk_buffer(); }
        [[nodiscard]] uint32_t vertex_count() const noexcept { return vertex_count_; }
        [[nodiscard]] uint32_t index_count() const noexcept { return index_count_; }

        [[nodiscard]] gfx::handles::Mesh render_handles() const noexcept{
            return {
                vk_vertex_buffer(),
                vk_index_buffer(),
                index_count()
            };
        }

    protected:
        Loader<Data>::Ptr loader_;
        vk::utils::Buffer::Ptr vertex_buffer_;
        vk::utils::Buffer::Ptr index_buffer_;
        uint32_t vertex_count_;
        uint32_t index_count_;
    };
}