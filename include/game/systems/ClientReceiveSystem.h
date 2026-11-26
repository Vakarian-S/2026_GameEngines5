#pragma once
#include <vector>

#include "ecs/System.h"
#include "net/ReceivedMessage.h"

class World;

/**
 * Client only. Sends Hello once the connection is up, and prints what the server says: Welcome, PlayerJoined and
 * PlayerLeft. Throws when the connection to the server ends
 */
class ClientReceiveSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit ClientReceiveSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** This frame's messages. Kept between frames so its memory is reused */
    std::vector<NET::ReceivedMessage> messages;
};


