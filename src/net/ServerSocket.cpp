#include "net/ServerSocket.h"

#include <stdexcept>
#include <string>

#include <steam/steamnetworkingsockets.h>

namespace
{
    /** How many messages one GNS call hands back at most. Receive loops until fewer come back */
    constexpr int kReceiveBatch = 64;
}

NET::ServerSocket::ServerSocket(const std::uint16_t port)
{
    SteamNetworkingIPAddr address{};
    address.Clear();
    address.m_port = port;
    listenSocket = SteamNetworkingSockets()->CreateListenSocketIP(address, 0, nullptr);
    if (listenSocket == k_HSteamListenSocket_Invalid)
    {
        throw std::runtime_error("could not listen on port " + std::to_string(port));
    }
    pollGroup = SteamNetworkingSockets()->CreatePollGroup();
}

NET::ServerSocket::~ServerSocket()
{
    SteamNetworkingSockets()->CloseListenSocket(listenSocket);
    SteamNetworkingSockets()->DestroyPollGroup(pollGroup);
}

bool NET::ServerSocket::Accept(const std::uint32_t connection)
{
    if (SteamNetworkingSockets()->AcceptConnection(connection) != k_EResultOK) return false;
    return SteamNetworkingSockets()->SetConnectionPollGroup(connection, pollGroup);
}

bool NET::ServerSocket::Send(std::uint32_t connection, std::span<const std::byte> bytes)
{
    const EResult result = SteamNetworkingSockets()->SendMessageToConnection(
        connection,
        bytes.data(),
        static_cast<std::uint32_t>(bytes.size()),
        k_nSteamNetworkingSend_Reliable,
        nullptr);
    return result == k_EResultOK;
}

void NET::ServerSocket::Close(const std::uint32_t connection, const char* reason)
{
    /* Linger: GNS tries to send queued reliable messages before the close */
    SteamNetworkingSockets()->CloseConnection(connection, 0, reason, true);
}

std::size_t NET::ServerSocket::Receive(std::vector<ReceivedMessage>& out)
{
    std::size_t added = 0;
    SteamNetworkingMessage_t* messages[kReceiveBatch];
    while (true)
    {
        const int count = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(pollGroup, messages, kReceiveBatch);
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
