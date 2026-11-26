#include "app/NetworkArguments.h"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "net/Ports.h"

namespace
{
    /**
     * Finds flag in args and returns the word after it. Throws when flag is the last word
     * @return the word after flag, or no value when flag is missing
     */
    std::optional<std::string_view> FlagValue(const std::span<char*> args, const std::string_view flag,
                                              const std::string_view example)
    {
        const auto found = std::ranges::find(args, flag);
        if (found == args.end()) return std::nullopt;

        const auto value = std::next(found);
        if (value == args.end())
            throw std::invalid_argument(std::string(flag) + " needs a value, such as " + std::string(example));
        return std::string_view(*value);
    }

    /** Reads a port from 1 to 65535. Throws on anything else, such as 0, 70000 or "abc" */
    std::uint16_t ParsePort(const std::string_view text)
    {
        std::uint16_t port = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc() || end != text.data() + text.size() || port == 0)
            throw std::invalid_argument("a port is a number from 1 to 65535, not \"" + std::string(text) + "\"");
        return port;
    }
}

NetworkSettings ReadNetworkSettings(int argc, char* argv[])
{
    /* Wrap argv in a span and skip argv[0] */
    const auto args = std::span(argv, argc).subspan(1);
    const bool server = std::ranges::find(args, std::string_view("--server")) != args.end();
    const auto port = FlagValue(args, "--port", "--port 27020");
    const auto connect = FlagValue(args, "--connect", "--connect 127.0.0.1:27020");

    if (server && connect) throw std::invalid_argument("pick --server or --connect, not both");
    if (port && !server)
        throw std::invalid_argument("--port goes with --server. A client writes the port in --connect host:port");

    if (server) return {.role = NetworkRole::Server, .port = port ? ParsePort(*port) : NET::kDefaultPort};
    if (!connect) return {};

    /* The port follows the last colon. With no colon, the whole word is the host */
    const std::size_t colon = connect->rfind(':');
    const std::string_view host = connect->substr(0, colon);
    if (host.empty()) throw std::invalid_argument("--connect needs a host, such as 127.0.0.1");
    const std::uint16_t connectPort =
        colon == std::string_view::npos ? NET::kDefaultPort : ParsePort(connect->substr(colon + 1));
    return {.role = NetworkRole::Client, .host = std::string(host), .port = connectPort};
}

void PrintNetworkSettings(const NetworkSettings& settings)
{
    if (settings.role == NetworkRole::Server) std::cout << "server on port " << settings.port << "\n";
    if (settings.role == NetworkRole::Client)
        std::cout << "client for " << settings.host << ":" << settings.port << "\n";
}
