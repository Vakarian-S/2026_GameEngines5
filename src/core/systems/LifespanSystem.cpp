#include "core/systems/LifespanSystem.h"

#include "core/Components.h"
#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"

LifespanSystem::LifespanSystem(World& world) :
    System("Lifespan", COMPONENTS::MaskOf<LifespanComponent>(), COMPONENTS::MaskOf<LifespanComponent>()),
    entities(world, COMPONENTS::MaskOf<LifespanComponent>())
{
}

void LifespanSystem::Run(const SystemContext& context)
{
    entities.ForEachChunk([&context](ChunkView chunk)
    {
        LifespanComponent* lifespans = chunk.Column<LifespanComponent>();
        const Entity* entityIds = chunk.Entities();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            /* Unsigned: a lifespan that starts at 0 wraps around and never ends */
            if (--lifespans[row].ticksLeft == 0) context.commands.DestroyEntity(entityIds[row]);
        }
    });
}
