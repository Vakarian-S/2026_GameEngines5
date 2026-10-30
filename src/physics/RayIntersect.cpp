#include "physics/RayIntersect.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Vector.h"
#include "VMath.h"
#include "physics/Components.h"
#include "physics/Quadratic.h"
#include "physics/Ray.h"

namespace
{
    /**
     * Tests the ray against one flat end cap: the plane through the cap centre, then the radius.
     * @return the hit on the cap, or std::nullopt if the ray misses the disc
     */
    std::optional<PHYSICS::RayHit> IntersectCap(const PHYSICS::Ray& localRay, const MATH::Vec3& capCentre,
                                                const MATH::Vec3& axis, const float radius)
    {
        /* The ray runs along the cap's plane and never crosses it */
        const float directionAlongAxis = MATH::VMath::dot(localRay.direction, axis);
        if (std::abs(directionAlongAxis) < MATH::kVerySmall) return std::nullopt;

        /* t = ((C - S).n) / (V.n), the ray-plane formula */
        const float t = MATH::VMath::dot(capCentre - localRay.origin, axis) / directionAlongAxis;
        if (t < 0.0f) return std::nullopt;

        /* The plane is hit, but the cap is only the disc of the radius around its centre */
        const MATH::Vec3 point = localRay.PointAt(t);
        const MATH::Vec3 centreToPoint = point - capCentre;
        if (MATH::VMath::dot(centreToPoint, centreToPoint) > radius * radius) return std::nullopt;

        return {
                    {
                        .intersectionPosition = point,
                        .t = t
                    }
        };
    }
}

std::optional<PHYSICS::RayHit> PHYSICS::Intersect(const Ray& localRay, const SphereCollider& sphereCollider)
{
    /* Quadratic equation at² + bt + c = 0, where
     * a = D·D, b = 2(M·D), c = M·M - r², and M = rayStart - centre */
    /* Rays work in local space, so RayStart - Centre is just RayStart */
    const MATH::Vec3 centerToRay = localRay.origin;
    const float a = MATH::VMath::dot(localRay.direction, localRay.direction);
    const float b = 2 * MATH::VMath::dot(centerToRay, localRay.direction);
    const float c = MATH::VMath::dot(centerToRay, centerToRay) - sphereCollider.radius * sphereCollider.radius;
    const QuadraticRoots solution = SolveQuadratic(a, b, c);

    /* Miss */
    if (solution.numSolutions == 0) return std::nullopt;

    /* The solution is behind the ray, so it doesnt count, and if there is a positive larger solution it means we are
     * inside the sphere, which we dont take into account */
    if (solution.smallerSolution < 0.0f) return std::nullopt;

    return {
        {
            .intersectionPosition = localRay.PointAt(solution.smallerSolution),
            .t = solution.smallerSolution
        }
    };
}

std::optional<PHYSICS::RayHit> PHYSICS::Intersect(const Ray& localRay, const BoxCollider& boxCollider)
{
    /* The handout starts tmin at 0, which makes a ray starting inside a hit at t = 0.
     * Starting at -infinity keeps the real entry, so the inside check below can see it */
    float tMin = -std::numeric_limits<float>::infinity();
    float tMax = std::numeric_limits<float>::infinity();

    /* Loop over the 3 slabs of the box along the x, y, and z axes */
    for (int i = 0; i < 3; i++)
    {
        const float halfExtent = boxCollider.halfExtents[i];
        const float origin = localRay.origin[i];
        const float direction = localRay.direction[i];

        /* Parallel to this slab: outside it never enters, inside it this axis sets no limit */
        if (std::abs(direction) < MATH::kVerySmall)
        {
            if (origin < -halfExtent || origin > halfExtent) return std::nullopt;
            continue;
        }

        /* Where the ray crosses the -h and +h planes of this axis */
        float t1 = (-halfExtent - origin) / direction;
        float t2 = (halfExtent - origin) / direction;

        /* A negative direction reaches +h first, so t1 must hold the nearer crossing */
        if (t1 > t2) std::swap(t1, t2);

        /* Inside the box means inside every slab: the last entry and the first exit */
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);

        /* The ray left one slab before it entered another */
        if (tMin > tMax) return std::nullopt;
    }

    /* The entry is behind the ray: the box is behind it, or the ray starts inside */
    if (tMin < 0.0f) return std::nullopt;

    return {
        {
            .intersectionPosition = localRay.PointAt(tMin),
            .t = tMin
        }
    };
}

std::optional<PHYSICS::RayHit> PHYSICS::Intersect(const Ray& localRay, const CylinderCollider& cylinderCollider)
{
     /* A is the bottom cap centre, B the top one, and AB the unit axis from A to B */
    const MATH::Vec3 v = localRay.direction;
    constexpr MATH::Vec3 AB = MATH::Vec3(0.f, 1.f, 0.f);
    const MATH::Vec3 A = MATH::Vec3(0.f, -cylinderCollider.halfHeight, 0.f);
    const MATH::Vec3 B = MATH::Vec3(,.f, cylinderCollider.halfHeight, 0.f);
    const MATH::Vec3 AS = localRay.origin - A;
    const float height = 2.0f * cylinderCollider.halfHeight;
    const float r2 = cylinderCollider.radius * cylinderCollider.radius;

    /*
    * a = V.V - (V.AB)^2
    * b = 2 (AS . V - (V.AB)(AS.AB))
    * c = AS.AS - (AS.AB)^2 - r^2
    */
    const float a = MATH::VMath::dot(v, v) - MATH::VMath::dot(v, AB) * MATH::VMath::dot(v, AB);
    const float b = 2 * (MATH::VMath::dot(AS, v) - MATH::VMath::dot(v, AB) * MATH::VMath::dot(AS, AB));
    const float c = MATH::VMath::dot(AS, AS) - MATH::VMath::dot(AS, AB) * MATH::VMath::dot(AS, AB) - r2;

    /* How far above A the start sits, measured along the axis */
    const float startHeight = MATH::VMath::dot(AS, AB);

    /* c <= 0 means the start is within the radius of the infinite cylinder */
    if (c <= MATH::kVerySmall)
    {
        /* Between the caps too, so the ray starts inside the can, which we dont take into account */
        if (startHeight >= 0.0f && startHeight <= height) return std::nullopt;

        /* Above or below the can: only the nearer cap can be hit */
        return IntersectCap(localRay, startHeight < 0.0f ? A : B, AB, cylinderCollider.radius);
    }

    /* The ray runs along the axis outside the radius, so it never reaches the tube.*/
    if (a < MATH::kVerySmall) return std::nullopt;

    const QuadraticRoots solution = SolveQuadratic(a, b, c);

    /* Miss the infinite tube */
    if (solution.numSolutions == 0) return std::nullopt;

    /* The start is outside the tube, so both roots are on the same side: behind the ray */
    if (solution.smallerSolution < 0.0f) return std::nullopt;

    /* Where the ray meets the infinite tube, and how far above A that is */
    const MATH::Vec3 Q = localRay.PointAt(solution.smallerSolution);
    const float hitHeight = MATH::VMath::dot(Q - A, AB);

    /* Between the caps: the ray hit the side */
    if (hitHeight >= 0.0f && hitHeight <= height)
    {
        return {
            {
                .intersectionPosition = Q,
                .t = solution.smallerSolution
            }
        };
    }

    /* Outside the caps: the ray can only get in through the cap on that side */
    return IntersectCap(localRay, hitHeight < 0.0f ? A : B, AB, cylinderCollider.radius);
}
