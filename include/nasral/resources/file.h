#pragma once
#include <memory>
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    class File final : public IResource
    {
    public:
        typedef std::unique_ptr<File> Ptr;

        explicit File(ResourceManager* manager, const std::string_view& path);
        ~File() override;

        File(const File&) = delete;
        File& operator=(const File&) = delete;

        void load() noexcept override;
        bool read(void* buffer, size_t size);

    protected:
        std::string_view path_;
        std::ifstream file_;
    };
}