#include "core/Components.h"
#include "physics/Components.h"


namespace PHYSICS
{
    /**
     * Detect Overlap collision in a 3d space from 2 Sphere colliders
     * @param a Transform of component A
     * @param sphereA Collider for Sphere A
     * @param b Transform of component B
     * @param sphereB Collider for Sphere B
     * @return Boolean indicating that the two shapes overlap
     */
    bool Overlap(const Transform& a, const PHYSICS::SphereCollider& sphereA, const Transform& b,
                 const PHYSICS::SphereCollider& sphereB);
}
