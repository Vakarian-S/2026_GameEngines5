#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "net/ReceivedMessage.h"

namespace NET
{
    /** A client's one connection to a server. It closes when destroyed */
    class ClientConnection
    {
    public:
        /**
         * Start connecting to address. such as "127.0.0.1:27020". Does not wait: A Connected ConnectionEvent says when
         * it is ready. Throws std::invalid_argument for an address GNS cannot read.
         * @param address address of the server to connect to
         */
        explicit ClientConnection(const std::string& address);

        /** Closes the connection, which the server sees as ClosedByPeer */
        ~ClientConnection();

        /* Owns a GNS handle, so it is never copied or moved */
        ClientConnection(const ClientConnection&) = delete;
        ClientConnection& operator=(const ClientConnection&) = delete;
        ClientConnection(ClientConnection&&) = delete;
        ClientConnection& operator=(ClientConnection&&) = delete;

        /** @return The connection's number, which ConnectionEvent::connection carries for it */
        [[nodiscard]] std::uint32_t GetConnection() const { return connection; }

        /**
         * Sends bytes to the server, reliably and in order
         * @return false for a bad or closed connection, a message that is too big, or a full send queue
         */
        bool Send(std::span<const std::byte> bytes);

        /** Appends every waiting message from the server to out. 
         * @return How many messages it added 
         */
        std::size_t Receive(std::vector<ReceivedMessage>& out);

    private:
        /** GNS's HSteamNetConnection */
        std::uint32_t connection = 0;
    };
}
