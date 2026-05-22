#pragma once

#include <nasral/res/objects/resource.h>

namespace nasral::res
{
    class Scene final : public Resource
    {
    public:
        typedef std::unique_ptr<Scene> Ptr;

        struct Data
        {
            // Define the data structure for a scene here.
            // This could include properties like objects, lights, cameras, etc.
        };

        Scene(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~Scene() override;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        void load() noexcept override;
    private:
        Loader<Data>::Ptr loader_;
        // Add any private members specific to the Scene class here.
    };
}