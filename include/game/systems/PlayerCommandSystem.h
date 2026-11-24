#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"
#include "game/Components.h"

class World;

/** Turns this machine's keys into the local player's PlayerCommandComponent, once per frame in Update. The only writer of PlayerCommandComponent */
class PlayerCommandSystem : public System
{
public:
    /** @param world the world whose local player this system drives */
    explicit PlayerCommandSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** The local player: every entity with a PlayerCommandComponent and LocalPlayerTag */
    Query localPlayers;
};
