#pragma once
#include <cstdint>

#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * TEST SYSTEM -
 * Every few frames, tags or untags each Damageable entity with Hit, so playback has work to measure */
class DamageSystem : public System
{
public:
    /**
     * @param world the world whose Damageable entities this system tags
     * @param damageEvery_ runs between one damage run and the next
     */
    DamageSystem(World& world, uint64_t damageEvery_);

    void Run(const SystemContext& context) override;

private:
    /** Every entity with the Damageable tag */
    Query damageables;
    /** Times Run has been called. */
    uint64_t runCount = 0;
    /** Runs between one damage run and the next */
    uint64_t damageEvery;
    /** Whether the last damage frame added Hit. If so, the next one removes it */
    bool hitOn = false;
};
