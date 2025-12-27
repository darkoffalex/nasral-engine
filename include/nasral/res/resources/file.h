#pragma once

#include <fstream>
#include <nasral/res/resource.h>
#include <nasral/log/loggable.h>

namespace nasral::res
{
    class Manager;
    class File final : public IResource, public log::Loggable<File>
    {
    public:
        typedef std::unique_ptr<File> Ptr;

        File(Manager* manager, ResourceId id);
        ~File() override;

        File(const File&) = delete;
        File& operator=(const File&) = delete;

        void load() noexcept override;
        bool read(void* buffer, size_t size);

    protected:
        std::ifstream file_;
    };
}
