#pragma once

#include "core/NetworkSettings.h"

/**
 * Reads --server, --port P and --connect host[:P] from the command line. A missing port is
 * NET::kDefaultPort. Throws std::invalid_argument on a bad or clashing flag
 * @return Server with --server, Client with --connect, Offline with neither
 */
NetworkSettings ReadNetworkSettings(int argc, char* argv[]);

/** Prints one line with the role and the address. Prints nothing when offline */
void PrintNetworkSettings(const NetworkSettings& settings);