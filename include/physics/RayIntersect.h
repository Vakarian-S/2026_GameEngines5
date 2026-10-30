#pragma once
#include <optional>

#include "Ray.h"

namespace PHYSICS
{
    struct CapsuleCollider;
    struct SphereCollider;
    struct BoxCollider;
    struct CylinderCollider;
    
    /**
     * Computes the intersection of a ray with a sphere collider in local space.
     * @param localRay The ray in local space for which the intersection is to be checked.
     * @param sphereCollider The sphere collider to test for intersection with the ray.
     * @return An optional containing a RayHit struct if the intersection occurs,
     *         or std::nullopt if the ray and sphere do not intersect.
     */
    std::optional<RayHit> Intersect(const Ray& localRay, const SphereCollider& sphereCollider);
    
    /**
     * Computes the intersection of a ray with a box collider in local space.
     * @param localRay The ray in local space for which the intersection is to be checked.
     * @param boxCollider The box collider to test for intersection with the ray.
     * @return An optional containing a RayHit struct if the intersection occurs,
     *         or std::nullopt if the ray and sphere do not intersect.
     */
    std::optional<RayHit> Intersect(const Ray& localRay, const BoxCollider& boxCollider);

    /**
     * Tests the intersection of a ray with a cylinder collider in local space.
     * @param localRay The ray in local space to be tested against the cylinder collider.
     * @param cylinderCollider The cylinder collider to test for intersection with the ray.
     * @return An optional containing a RayHit struct if the ray intersects the cylinder,
     *         or std::nullopt if no intersection occurs.
     */
    std::optional<RayHit> Intersect(const Ray& localRay, const CylinderCollider& cylinderCollider);
    
    std::optional<RayHit> Intersect(const Ray& localRay, const CapsuleCollider& capsuleCollider);
}
