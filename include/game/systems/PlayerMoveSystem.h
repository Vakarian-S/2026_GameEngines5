#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * Reads the players commands and sets the entities velocity along the player command direction.
 */
class PlayerMoveSystem : public System
{
public:
    /** @param world the world whose entities this system reads
     * @param speed_ speed in metres per seconds for player movement*/
    PlayerMoveSystem(World& world, float speed_);

    void Run(const SystemContext& context) override;

private:
    /** Entities with a PlayerCommandComponent and a VelocityComponent */
    Query entities;
    /** Speed in metres per second to move entities */
    float speed;
};


