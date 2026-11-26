#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace NET
{
    /** The connection states the engine reacts to. Every other GNS state becomes Other */
    enum class ConnectionState : std::uint8_t
    {
        None,
        Connecting,
        Connected,
        ClosedByPeer,
        ProblemDetectedLocally,
        Other
    };

    /** One connection changing state, copied out of GNS's status callback */
    struct ConnectionEvent
    {
        /** The connection that changed, as GNS's HSteamNetConnection number */
        std::uint32_t connection = 0;
        /** The listen socket the connection arrived on, or 0 for a connection this program opened */
        std::uint32_t listenSocket = 0;
        /** The state before the change */
        ConnectionState oldState = ConnectionState::None;
        /** The state after the change */
        ConnectionState state = ConnectionState::None;
        /** GNS's end reason code, 0 while the connection is open */
        int endReason = 0;
        /** GNS's text for why the connection ended, empty while it is open */
        std::array<char, 128> endMessage{};
    };

    /** A World's connection events since its systems last read them. Only a World that holds one gets events */
    struct ConnectionEventQueue
    {
        /** Events in the order GNS reported them */
        std::vector<ConnectionEvent> events;
    };
}
