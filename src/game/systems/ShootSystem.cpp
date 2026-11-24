#include "game/systems/ShootSystem.h"

#include "core/Components.h"
#include "core/time/FixedTickCount.h"
#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"
#include "physics/Components.h"

ShootSystem::ShootSystem(World& world) :
    System("Shoot", COMPONENTS::MaskOf<TransformComponent, PlayerCommandComponent>(), COMPONENTS::MaskOf<GunComponent>()),
    entities(world, COMPONENTS::MaskOf<TransformComponent, GunComponent, PlayerCommandComponent>())
{
}

void ShootSystem::Run(const SystemContext& context)
{
    const std::uint64_t ticks = context.Get<FixedTickCount>().ticks;
    CommandBuffer& commandBuffer = context.commands;
    entities.ForEachChunk([ticks, &commandBuffer](ChunkView chunk)
    {
        GunComponent* guns = chunk.Column<GunComponent>();
        const PlayerCommandComponent* playerCommands = chunk.Column<PlayerCommandComponent>();
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            GunComponent& gun = guns[row];
            const MATH::Vec3& aim = playerCommands[row].aimDirection;

            if ((aim.x == 0.0f && aim.z == 0.0f) || ticks < gun.readyTick) continue;

            commandBuffer.CreateEntity(
                TransformComponent{.position = transforms[row].position},
                PHYSICS::VelocityComponent{.metresPerSecond = aim * gun.projectileSpeed},
                LifespanComponent{.ticksLeft = gun.lifetimeTicks},
                gun.projectileRenderable,
                PHYSICS::SphereColliderComponent{},
                DamageComponent{.damage = gun.projectileDamage}
            );
            gun.readyTick = ticks + gun.cooldownTicks;
        }
    });
}