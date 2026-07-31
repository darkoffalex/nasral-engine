#pragma once

#include <fstream>
#include <nasral/res/objects/resource.h>

namespace nasral::res
{
    class File final : public Resource
    {
    public:
        typedef std::unique_ptr<File> Ptr;

        File(Manager* manager, const ResourceId& id);
        ~File() override;

        File(const File&) = delete;
        File& operator=(const File&) = delete;

        void load() noexcept override;
        bool read(void* buffer, size_t size);

    private:
        std::ifstream file_;
    };
}
