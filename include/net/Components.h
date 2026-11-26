#pragma once

#include <cstdint>

namespace NET
{
    /** The number the server gives an entity so every program can name it. 0 is never given out */
    struct NetworkIdComponent
    {
        std::uint32_t value = 0;
    };
}