#include "game/systems/BossDeathSystem.h"

#include <iostream>

#include "core/Components.h"
#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"

BossDeathSystem::BossDeathSystem(World& world) :
    System("BossDeath", COMPONENTS::MaskOf<HealthComponent>(), 0),
    entities(world, COMPONENTS::MaskOf<BossComponent, HealthComponent>())
{
}

void BossDeathSystem::Run(const SystemContext& context)
{
    CommandBuffer& commands = context.commands;
    entities.ForEachChunk([&commands](ChunkView chunk)
    {
        const HealthComponent* healthComponents = chunk.Column<HealthComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (const auto& [points] = healthComponents[row]; points <= 0)
            {
                std::cout << "boss died\n";
                commands.DestroyEntity(chunk.Entities()[row]);
            }
        }
    });
}
