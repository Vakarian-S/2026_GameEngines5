#pragma once
#include "ecs/CommandBuffer.h"
#include "ecs/Query.h"
#include "ecs/System.h"
#include "net/ClientTable.h"
#include "net/ServerSocket.h"

class World;

/**
 * Server only. Accepts each client that connects at this World's listen socket while a slot is free, and turns it
 * away when all slots are taken. When a client leaves, it despawns that client's player, tells the other clients
 * and frees the slot
 */
class AcceptClientsSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit AcceptClientsSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Entities with a NetworkIdComponent, searched to find a leaving client's player */
    Query players;

    /**
    * Closes a client's handle, frees its slot, queues its player's despawn, and sends PlayerLeft to the others
    * @param server the socket the client arrived on
    * @param table the server's clients
    * @param commands where the despawn is queued
    * @param connection the client that left
    * @param reason GNS's text for why it left
    */
    void Leave(NET::ServerSocket& server, NET::ClientTable& table, CommandBuffer& commands, std::uint32_t connection,
               const char* reason);
};
