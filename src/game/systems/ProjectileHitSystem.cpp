#include "game/systems/ProjectileHitSystem.h"

#include "core/Components.h"
#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"
#include "physics/Collision3D.h"
#include "physics/Components.h"

ProjectileHitSystem::ProjectileHitSystem(World& world) :
    System("ProjectileHit", COMPONENTS::MaskOf<TransformComponent, PHYSICS::SphereColliderComponent, DamageComponent>(),
           COMPONENTS::MaskOf<HealthComponent>()),
    projectiles(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::SphereColliderComponent, DamageComponent>()),
    targets(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::SphereColliderComponent, HealthComponent>())
{
}

void ProjectileHitSystem::Run(const SystemContext& context)
{
    projectiles.ForEachChunk([this, &context](ChunkView projectileChunk)
    {
        const Entity* projectileEntities = projectileChunk.Entities();
        const DamageComponent* damageComponents = projectileChunk.Column<DamageComponent>();
        const TransformComponent* projectileTransforms = projectileChunk.Column<TransformComponent>();
        const PHYSICS::SphereColliderComponent* projectileColliders = projectileChunk.Column<PHYSICS::SphereColliderComponent>();
        const uint32_t projectileRowTotal = projectileChunk.Count();
        for (uint32_t projectileRow = 0; projectileRow < projectileRowTotal; ++projectileRow)
        {
            /* A projectile deals damage once. The flag stops the next target chunks; break stops this one */
            bool spent = false;
            targets.ForEachChunk([&](ChunkView targetChunk)
            {
                if (spent) return;
                HealthComponent* healthComponents = targetChunk.Column<HealthComponent>();
                const TransformComponent* targetTransforms = targetChunk.Column<TransformComponent>();
                const PHYSICS::SphereColliderComponent* targetColliders = targetChunk.Column<PHYSICS::SphereColliderComponent>();
                const uint32_t targetRowTotal = targetChunk.Count();
                for (uint32_t targetRow = 0; targetRow < targetRowTotal; ++targetRow)
                {
                    if (!PHYSICS::Overlap(
                        projectileTransforms[projectileRow], projectileColliders[projectileRow],
                        targetTransforms[targetRow], targetColliders[targetRow]))
                        continue;

                    HealthComponent& health = healthComponents[targetRow];
                    health.points -= damageComponents[projectileRow].damage;

                    context.commands.DestroyEntity(projectileEntities[projectileRow]);
                    spent = true;
                    break;
                }
            });
        }
    });
}
