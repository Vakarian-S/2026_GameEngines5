#include "physics/RayIntersect.h"

#include "Vector.h"
#include "VMath.h"
#include "physics/Components.h"
#include "physics/Quadratic.h"
#include "physics/Ray.h"

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
