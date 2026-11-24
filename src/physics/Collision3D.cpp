#include "physics/Collision3D.h"

#include "core/Components.h"

bool PHYSICS::Overlap(const TransformComponent& a, const PHYSICS::SphereColliderComponent& sphereA, const TransformComponent& b,
                      const PHYSICS::SphereColliderComponent& sphereB)
{
    /* Spheres assume that scale is uniform */
    const float radiusA = sphereA.radius * a.scale.x;
    const float radiusB = sphereB.radius * b.scale.x;
    return MATH::VMath::mag(a.position - b.position) < radiusA + radiusB;
}
