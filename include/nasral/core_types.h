#pragma once
#include <string>

namespace nasral::logging { class Logger; }
namespace nasral::resources { class ResourceManager; }

namespace nasral
{
    struct EngineContext
    {
        logging::Logger* logger = nullptr;
        resources::ResourceManager* resource_manager = nullptr;
    };

    struct EngineConfig
    {
        struct Log
        {
            std::string file;
            bool console_out = false;
        } log;

        struct Resources
        {
            std::string content_dir;
        } resources;
    };
}