#pragma once

#include <span>
#include <vector>

#include "net/ConnectionEvents.h"

namespace NET
{
    /**
     * Starts GameNetworkingSockets (GNS) when built and shuts it down when destroyed. GNS allows
     * one per program, so each application owns exactly one.
     */
    class NetworkLibrary
    {
    public:
        /** Starts GNS. Throws std::runtime_error with GNS's own message when it fails */
        NetworkLibrary();

        /** Shuts GNS down, which also closes every connection and socket it still holds */
        ~NetworkLibrary();

        /* GNS is one per program, so the owner of it is never copied or moved */
        NetworkLibrary(const NetworkLibrary&) = delete;
        NetworkLibrary& operator=(const NetworkLibrary&) = delete;
        NetworkLibrary(NetworkLibrary&&) = delete;
        NetworkLibrary& operator=(NetworkLibrary&&) = delete;

        /**
         * Runs GNS's callbacks and returns the connection changes they reported since the last
         * call. The span stays valid until the next call
         */
        std::span<const ConnectionEvent> PollConnectionEvents();

    private:
        /** Filled by GNS's status callback while PollConnectionEvents runs */
        std::vector<ConnectionEvent> events;
    };
}
