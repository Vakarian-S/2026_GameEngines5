#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "net/ReceivedMessage.h"

namespace NET
{
    /**
     * A server's listen socket and the poll group that holds every client it accepted. Both close when it's destroyed,
     * which also drops every client
     */
    class ServerSocket
    {
    public:
        /** Listen on every local address at port. Throws std::runtime_error when the port cannot be opened */
        explicit ServerSocket(std::uint16_t port);
        /** Closes the listen socket, which closes every accepted client, then the poll group */
        ~ServerSocket();

        /* Owns GNS handles, so it is never copied or moved */
        ServerSocket(const ServerSocket&) = delete;
        ServerSocket& operator=(const ServerSocket&) = delete;
        ServerSocket(ServerSocket&&) = delete;
        ServerSocket& operator=(ServerSocket&&) = delete;

        /** 
         * The listen socket's number, which ConnectionEvent::listenSocket carries for this server's clients
         * @return number of the current listen socket
         */
        [[nodiscard]] std::uint32_t GetListenSocket() const { return listenSocket; }

        /**
         * Accepts a connecting client and adds it to the poll group. Returns false when GNS refuses
         * @return false when GNS refuses the connection or cannot add it to the poll group. 
         */
        bool Accept(std::uint32_t connection);

        /**
         * Sends bytes to one accepted client, reliably and in order
         * @return false when GNS refuses, such as for a closed connection
         */
        bool Send(std::uint32_t connection, std::span<const std::byte> bytes);

        /**
         * Closes one client's connection and frees its handle. GNS reports no more events for it
         * @param reason text the client sees as the end reason, or null
         */
        void Close(std::uint32_t connection, const char* reason);

        /**
         * Appends every waiting message from every accepted client to out. Returns how many it added
         * @return the number of messages received
         */
        std::size_t Receive(std::vector<ReceivedMessage>& out);

    private:
        /** GNS's HSteamListenSocket */
        std::uint32_t listenSocket = 0;
        /** GNS's HSteamNetPollGroup */
        std::uint32_t pollGroup = 0;
    };
}
