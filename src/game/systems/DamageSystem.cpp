#include "game/systems/DamageSystem.h"

#include "ecs/Archetype.h"
#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"

DamageSystem::DamageSystem(World& world, const uint64_t damageEvery_) :
    System("Damage", COMPONENTS::MaskOf<Damageable>(), COMPONENTS::MaskOf<Hit>()),
    damageables(world, COMPONENTS::MaskOf<Damageable>()),
    damageEvery(damageEvery_)
{
}

void DamageSystem::Run(const SystemContext& context)
{
    ++runCount;
    if (runCount % damageEvery != 0) return;

    /* This damage frame does the opposite of the last one */
    hitOn = !hitOn;

    for (const Archetype* archetype : damageables.Matches())
    {
        const Entity* rows = archetype->EntityColumn();
        for (uint32_t row = 0; row < archetype->Count(); ++row)
        {
            if (hitOn) context.commands.AddComponent<Hit>(rows[row]);
            else context.commands.RemoveComponent<Hit>(rows[row]);
        }
    }
}
