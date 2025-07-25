#pragma once
#include <memory>
#include <vulkan/utils/buffer.hpp>
#include <nasral/resources/resource_types.h>
#include <nasral/rendering/rendering_types.h>

namespace nasral::resources
{
    class Mesh final : public IResource
    {
    public:
        typedef std::unique_ptr<Mesh> Ptr;

        struct Data
        {
            std::vector<rendering::Vertex> vertices;
            std::vector<uint32_t> indices;
        };

        explicit Mesh(const ResourceManager* manager, const std::string_view& path, std::unique_ptr<Loader<Data>> loader);
        ~Mesh() override;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        void load() noexcept override;
        [[nodiscard]] const vk::Buffer& vk_vertex_buffer() const { return vertex_buffer_->vk_buffer(); }
        [[nodiscard]] const vk::Buffer& vk_index_buffer() const { return index_buffer_->vk_buffer(); }
        [[nodiscard]] size_t vertex_count() const { return vertex_count_; }
        [[nodiscard]] size_t index_count() const { return index_count_; }
        [[nodiscard]] rendering::Handles::Mesh render_handles() const;

    protected:
        std::string_view path_;
        std::unique_ptr<Loader<Data>> loader_;
        vk::utils::Buffer::Ptr vertex_buffer_;
        vk::utils::Buffer::Ptr index_buffer_;
        size_t vertex_count_;
        size_t index_count_;
    };
}