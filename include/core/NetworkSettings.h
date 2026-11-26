#pragma once

#include <cstdint>
#include <string>

/** What this program is on the network. Offline when the command line names no role */
enum class NetworkRole : std::uint8_t
{
    Offline,
    Server,
    Client,
};

/**
 * The network part of the command line. SceneManager sets it on each new World before OnCreate,
 * so a scene picks its systems from it.
 */
struct NetworkSettings
{
    /** Server with --server, Client with --connect, Offline with neither */
    NetworkRole role = NetworkRole::Offline;
    /** The server's host for a client, such as "127.0.0.1". Empty for the other roles */
    std::string host;
    /** Where a server listens, or where a client connects. 0 when offline */
    std::uint16_t port = 0;
};
