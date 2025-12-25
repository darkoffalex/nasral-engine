#pragma once

#include <cstdint>

namespace nasral::scn
{
    enum class NodeType : uint32_t
    {
        eDummy = 0,
        eSpatial,
        eCamera,
        eMesh,
        eSprite,
        eLight,
        TOTAL
    };

    enum class CameraType : uint32_t
    {
        ePerspective = 0,
        eOrthographic,
        TOTAL
    };

    struct Config
    {
    };
}