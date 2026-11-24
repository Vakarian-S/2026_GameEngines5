#include "game/systems/ShootSystem.h"

#include "core/Components.h"
#include "core/time/FixedTickCount.h"
#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"
#include "physics/Components.h"

ShootSystem::ShootSystem(World& world) :
    System("Shoot", COMPONENTS::MaskOf<Transform, PlayerCommand>(), COMPONENTS::MaskOf<GunComponent>()),
    entities(world, COMPONENTS::MaskOf<Transform, GunComponent, PlayerCommand>())
{
}

void ShootSystem::Run(const SystemContext& context)
{
    const std::uint64_t ticks = context.Get<FixedTickCount>().ticks;
    CommandBuffer& commandBuffer = context.commands;
    entities.ForEachChunk([ticks, &commandBuffer](ChunkView chunk)
    {
        GunComponent* guns = chunk.Column<GunComponent>();
        const PlayerCommand* playerCommands = chunk.Column<PlayerCommand>();
        const Transform* transforms = chunk.Column<Transform>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            GunComponent& gun = guns[row];
            const MATH::Vec3& aim = playerCommands[row].aimDirection;

            if ((aim.x == 0.0f && aim.z == 0.0f) || ticks < gun.readyTick) continue;

            commandBuffer.CreateEntity(
                Transform{.position = transforms[row].position},
                PHYSICS::Velocity{.metresPerSecond = aim * gun.projectileSpeed},
                LifespanComponent{.ticksLeft = gun.lifetimeTicks},
                gun.projectileRenderable,
                PHYSICS::SphereCollider{},
                DamageComponent{.damage = gun.projectileDamage}
            );
            gun.readyTick = ticks + gun.cooldownTicks;
        }
    });
}