#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** Checks all boss entities to destroy whichever has its health down to 0 */
class BossDeathSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit BossDeathSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    Query entities;
};
