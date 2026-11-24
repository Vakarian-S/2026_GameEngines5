#include "core/Components.h"
#include "physics/Components.h"


namespace PHYSICS
{
    /**
     * Detect Overlap collision in a 3d space from 2 Sphere colliders
     * @param a TransformComponent of component A
     * @param sphereA Collider for Sphere A
     * @param b TransformComponent of component B
     * @param sphereB Collider for Sphere B
     * @return Boolean indicating that the two shapes overlap
     */
    bool Overlap(const TransformComponent& a, const PHYSICS::SphereColliderComponent& sphereA, const TransformComponent& b,
                 const PHYSICS::SphereColliderComponent& sphereB);
}
