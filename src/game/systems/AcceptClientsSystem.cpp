#include "game/systems/AcceptClientsSystem.h"

#include <iostream>
#include <vector>

#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"
#include "net/ClientTable.h"
#include "net/Components.h"
#include "net/ConnectionEvents.h"
#include "net/Messages.h"
#include "net/ServerSocket.h"

AcceptClientsSystem::AcceptClientsSystem(World& world) :
    System("AcceptClients", COMPONENTS::MaskOf<NET::NetworkIdComponent>(), COMPONENTS::MaskOf<>()),
    players(world, COMPONENTS::MaskOf<NET::NetworkIdComponent>())
{
}

void AcceptClientsSystem::Run(const SystemContext& context)
{
    auto& queue = context.Get<NET::ConnectionEventQueue>();
    auto& server = context.Get<NET::ServerSocket>();
    auto& table = context.Get<NET::ClientTable>();

    for (const NET::ConnectionEvent& event : queue.events)
    {
        if (event.listenSocket != server.GetListenSocket()) continue;
        switch (event.state)
        {
        case NET::ConnectionState::Connecting:
        {
            NET::ClientSlot* slot = table.Find(0);
            if (!slot)
            {
                server.Close(event.connection, "server full");
                std::cout << "[Server] turned a client away: full\n";
                break;
            }
            if (!server.Accept(event.connection))
            {
                server.Close(event.connection, "accept failed");
                break;
            }
            *slot = {.connection = event.connection, .clientId = table.nextClientId++};
            std::cout << "[Server] client " << slot->clientId << " connecting\n";
            break;
        }
        case NET::ConnectionState::ClosedByPeer:
        case NET::ConnectionState::ProblemDetectedLocally:
            Leave(server, table, context.commands, event.connection, event.endMessage.data());
            break;
        default:
            break;
        }
    }
    queue.events.clear();
}

void AcceptClientsSystem::Leave(NET::ServerSocket& server, NET::ClientTable& table, CommandBuffer& commands,
                                const std::uint32_t connection, const char* reason)
{
    /* GNS keeps the handle after the peer closes until we close it too */
    server.Close(connection, nullptr);
    NET::ClientSlot* slot = table.Find(connection);
    if (!slot) return;
    const NET::ClientSlot gone = *slot;
    *slot = {};

    if (gone.playerNetworkId != 0)
    {
        players.ForEachChunk([&commands, &gone](ChunkView chunk)
        {
            const NET::NetworkIdComponent* ids = chunk.Column<NET::NetworkIdComponent>();
            const Entity* entities = chunk.Entities();
            for (std::uint32_t row = 0; row < chunk.Count(); ++row)
            {
                if (ids[row].value == gone.playerNetworkId) commands.DestroyEntity(entities[row]);
            }
        });
        const std::vector<std::byte> left = NET::Write(NET::PlayerLeftMessage{gone.clientId});
        for (const NET::ClientSlot& other : table.slots)
        {
            if (other.playerNetworkId != 0) server.Send(other.connection, left);
        }
    }
    std::cout << "[Server] client " << gone.clientId << " left (" << reason << "), players: "
        << table.PlayerCount() << "\n";
}