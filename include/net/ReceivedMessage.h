#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NET
{
    /** One message that arrived, copied out of GNS so GNS's own copy is released at once */
    struct ReceivedMessage
    {
        /** The connection that sent it, as GNS's HSteamNetConnection number */
        std::uint32_t connection = 0;
        /** The message's bytes, exactly as the sender passed them */
        std::vector<std::byte> bytes;
    };
}