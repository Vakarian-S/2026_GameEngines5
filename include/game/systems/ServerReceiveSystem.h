#pragma once
#include <vector>

#include "ecs/System.h"
#include "net/ReceivedMessage.h"

class World;

/**
 * Server only. Reads every message from the accepted clients. A Hello with the right protocol version spawns that
 * client's player, answers with Welcome, and tells the other clients. A wrong version closes the connection
 */
class ServerReceiveSystem : public System
{
public:
    /** @param world the world this system spawns players in */
    explicit ServerReceiveSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** This tick's messages. Kept between ticks so its memory is reused */
    std::vector<NET::ReceivedMessage> messages;
};
