#include "game/systems/ServerReceiveSystem.h"

#include <iostream>

#include "core/Components.h"
#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"
#include "net/ClientTable.h"
#include "net/Components.h"
#include "net/Messages.h"
#include "net/ServerSocket.h"
#include "physics/Components.h"

ServerReceiveSystem::ServerReceiveSystem(World&) :
    System("ServerReceive", COMPONENTS::MaskOf<>(), COMPONENTS::MaskOf<>())
{
}

void ServerReceiveSystem::Run(const SystemContext& context)
{
    auto& server = context.Get<NET::ServerSocket>();
    auto& table = context.Get<NET::ClientTable>();
    messages.clear();
    server.Receive(messages);

    for (const NET::ReceivedMessage& message : messages)
    {
        NET::ClientSlot* slot = table.Find(message.connection);
        if (!slot) continue;
        if (NET::PeekMessageType(message.bytes) != NET::MessageType::Hello || slot->playerNetworkId != 0)
        {
            std::cout << "[Server] dropped a message from client " << slot->clientId << "\n";
            continue;
        }
        const std::optional<NET::HelloMessage> hello = NET::ReadHello(message.bytes);
        if (!hello || hello->protocolVersion != NET::kProtocolVersion)
        {
            server.Close(message.connection, "wrong protocol version");
            *slot = {};
            continue;
        }

        slot->playerNetworkId = table.nextNetworkId++;
        context.commands.CreateEntity(
            TransformComponent{.position = MATH::Vec3(0.f, 0.f, 5.f)},
            PHYSICS::VelocityComponent{},
            PlayerCommandComponent{},
            NET::NetworkIdComponent{.value = slot->playerNetworkId}
        );
        server.Send(slot->connection, NET::Write(NET::WelcomeMessage{slot->clientId, slot->playerNetworkId}));

        const std::vector<std::byte> joined = NET::Write(NET::PlayerJoinedMessage{slot->clientId});
        for (const NET::ClientSlot& other : table.slots)
        {
            if (other.playerNetworkId != 0 && other.connection != slot->connection)
                server.Send(
                    other.connection, joined);
        }
        std::cout << "[Server] client " << slot->clientId << " joined, players: " << table.PlayerCount() << "\n";
    }
}
