#pragma once
#include <optional>

#include "Ray.h"

namespace PHYSICS
{
    struct SphereCollider;
    
    /**
     * Computes the intersection of a ray with a sphere collider in local space.
     * @param localRay The ray in local space for which the intersection is to be checked.
     * @param sphereCollider The sphere collider to test for intersection with the ray.
     * @return An optional containing a RayHit struct if the intersection occurs,
     *         or std::nullopt if the ray and sphere do not intersect.
     */
    std::optional<RayHit> Intersect(const Ray& localRay, const SphereCollider& sphereCollider);
}
