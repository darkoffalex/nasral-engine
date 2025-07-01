#pragma once
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    class File final : public IResource
    {
    public:
        explicit File(ResourceManager* manager, const std::string_view& path);
        ~File() override;

        void load() override;
        bool read(void* buffer, size_t size);

    private:
        std::string_view path_;
        std::ifstream file_;
    };
}