#include "game/systems/ClientReceiveSystem.h"

#include <iostream>
#include <stdexcept>
#include <string>

#include "ecs/SystemContext.h"
#include "net/ClientConnection.h"
#include "net/ConnectionEvents.h"
#include "net/Messages.h"

ClientReceiveSystem::ClientReceiveSystem(World&) :
    System("ClientReceive", COMPONENTS::MaskOf<>(), COMPONENTS::MaskOf<>())
{
}
void ClientReceiveSystem::Run(const SystemContext& context)
{
    auto& queue = context.Get<NET::ConnectionEventQueue>();
    auto& client = context.Get<NET::ClientConnection>();

    for (const NET::ConnectionEvent& event : queue.events)
    {
        if (event.connection != client.GetConnection()) continue;
        if (event.state == NET::ConnectionState::Connected)
        {
            std::cout << "[Client] connected, sending Hello\n";
            client.Send(NET::Write(NET::HelloMessage{NET::kProtocolVersion}));
        }
        else if (event.state == NET::ConnectionState::ClosedByPeer ||
                 event.state == NET::ConnectionState::ProblemDetectedLocally)
        {
            throw std::runtime_error(std::string("lost the server: ") + event.endMessage.data());
        }
    }
    queue.events.clear();

    messages.clear();
    client.Receive(messages);
    for (const NET::ReceivedMessage& message : messages)
    {
        const std::optional<NET::MessageType> type = NET::PeekMessageType(message.bytes);
        if (!type) continue;
        switch (*type)
        {
        case NET::MessageType::Welcome:
            if (const auto welcome = NET::ReadWelcome(message.bytes))
                std::cout << "[Client] welcome: client " << welcome->clientId << ", player NetworkId "
                    << welcome->playerNetworkId << "\n";
            break;
        case NET::MessageType::PlayerJoined:
            if (const auto joined = NET::ReadPlayerJoined(message.bytes))
                std::cout << "[Client] player " << joined->clientId << " joined\n";
            break;
        case NET::MessageType::PlayerLeft:
            if (const auto left = NET::ReadPlayerLeft(message.bytes))
                std::cout << "[Client] player " << left->clientId << " left\n";
            break;
        default:
            break;
        }
    }
}
