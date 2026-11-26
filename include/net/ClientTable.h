#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace NET
{
    /** Most clients one server holds at once */
    inline constexpr std::size_t kMaxClients = 8;

    /** One accepted client. Connection 0 marks a free slot */
    struct ClientSlot
    {
        /** GNS's HSteamNetConnection on the server's side */
        std::uint32_t connection = 0;
        /** This client's number, never reused within a server run */
        std::uint32_t clientId = 0;
        /** The NetworkId of this client's player, 0 until its Hello spawns one */
        std::uint32_t playerNetworkId = 0;
    };

    /** Every client the server accepted. A server World holds one as a resource */
    struct ClientTable
    {
        std::array<ClientSlot, kMaxClients> slots{};
        /** The next clientId to give out */
        std::uint32_t nextClientId = 1;
        /** The next NetworkId to give out */
        std::uint32_t nextNetworkId = 1;

        /**
         * @param connection the connection id on the server of the client to find
         * @return the slot holding connection, or null. Find(0) returns a free slot */
        ClientSlot* Find(std::uint32_t connection);
        /** @return how many slots have a spawned player */
        [[nodiscard]] std::size_t PlayerCount() const;
    };
}
