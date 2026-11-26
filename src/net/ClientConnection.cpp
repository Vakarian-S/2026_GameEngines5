#include "net/ClientConnection.h"

#include <stdexcept>
#include <steam/steamnetworkingsockets.h>

namespace
{
    /** How many messages one GNS call hands back at most. Receive loops until fewer come back */
    constexpr int kReceiveBatch = 64;
}

NET::ClientConnection::ClientConnection(const std::string& address)
{
    SteamNetworkingIPAddr serverAddress{};
    if (!serverAddress.ParseString(address.c_str()) || serverAddress.m_port == 0)
    {
        throw std::invalid_argument("not an address with a port: \"" + address + "\"");
    }
    connection = SteamNetworkingSockets()->ConnectByIPAddress(serverAddress, 0, nullptr);
}

NET::ClientConnection::~ClientConnection()
{
    SteamNetworkingSockets()->CloseConnection(connection, 0, nullptr, false);
}

bool NET::ClientConnection::Send(const std::span<const std::byte> bytes)
{
    const EResult result = SteamNetworkingSockets()->SendMessageToConnection(
        connection,
        bytes.data(),
        static_cast<std::uint32_t>(bytes.size()),
        k_nSteamNetworkingSend_Reliable,
        nullptr);
    return result == k_EResultOK;
}

std::size_t NET::ClientConnection::Receive(std::vector<ReceivedMessage>& out)
{
    std::size_t added = 0;
    SteamNetworkingMessage_t* messages[kReceiveBatch];
    while (true)
    {
        const int count = SteamNetworkingSockets()->ReceiveMessagesOnConnection(connection, messages, kReceiveBatch);
        for (int i = 0; i < count; ++i)
        {
            const auto* data = static_cast<const std::byte*>(messages[i]->m_pData);
            out.push_back({
                .connection = messages[i]->m_conn,
                .bytes = {data, data + messages[i]->m_cbSize}
            });
            messages[i]->Release();
        }
        added += count > 0 ? count : 0;
        if (count < kReceiveBatch) return added;
    }
}
