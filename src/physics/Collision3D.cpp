#include "physics/Collision3D.h"

#include "core/Components.h"

bool PHYSICS::Overlap(const Transform& a, const PHYSICS::SphereCollider& sphereA, const Transform& b,
                      const PHYSICS::SphereCollider& sphereB)
{
    /* Spheres assume that scale is uniform */
    const float radiusA = sphereA.radius * a.scale.x;
    const float radiusB = sphereB.radius * b.scale.x;
    return MATH::VMath::mag(a.position - b.position) < radiusA + radiusB;
}
