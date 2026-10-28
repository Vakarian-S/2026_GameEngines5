/*
 * Tests for the physics math: the ray, the quadratic solver and, later, the ray-shape tests.
 *
 * Nothing here touches SDL, Vulkan or the ECS, so this target builds on Win32 as well as x64.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <cmath>
#include <iostream>
#include <optional>

#include "physics/Components.h"
#include "physics/Quadratic.h"
#include "physics/Ray.h"
#include "physics/RayIntersect.h"

namespace
{
    int checks = 0;
    int failures = 0;

    constexpr const char* Pass = "\x1b[1;32m";
    constexpr const char* Fail = "\x1b[1;91m";
    constexpr const char* Reset = "\x1b[0m";

    /** How far apart two floats may be and still count as equal in these tests. */
    constexpr float kTolerance = 1.0e-4f;

    /** Prints one named expectation, and remembers whether it held. */
    void Check(const char* what, const bool held)
    {
        ++checks;
        if (!held) ++failures;
        std::cout << (held ? "  pass  " : "  FAIL  ") << what << "\n";
    }

    /** @return true if two floats differ by less than kTolerance */
    bool Near(const float a, const float b)
    {
        return std::abs(a - b) < kTolerance;
    }

    /** @return true if two vectors differ by less than kTolerance on every axis */
    bool Near(const MATH::Vec3& a, const MATH::Vec3& b)
    {
        return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z);
    }
}

int main()
{
    std::cout << "PhysicsTests\n\n";

    std::cout << "TEST CASE: a ray walks along its direction\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(1.0f, 2.0f, 3.0f), MATH::Vec3(0.0f, 0.0f, -2.0f)};

        Check("t = 0 is the origin", Near(ray.PointAt(0.0f), MATH::Vec3(1.0f, 2.0f, 3.0f)));
        Check("t = 1 moves one whole direction", Near(ray.PointAt(1.0f), MATH::Vec3(1.0f, 2.0f, 1.0f)));
        Check("a direction of length 2 is not normalized", Near(ray.PointAt(0.5f), MATH::Vec3(1.0f, 2.0f, 2.0f)));
        Check("a negative t is behind the origin", Near(ray.PointAt(-1.0f), MATH::Vec3(1.0f, 2.0f, 5.0f)));
    }

    std::cout << "\nTEST CASE: two roots, x^2 - 5x + 6 = 0\n";
    {
        const PHYSICS::QuadraticRoots roots = PHYSICS::SolveQuadratic(1.0f, -5.0f, 6.0f);

        Check("there are two roots", roots.numSolutions == 2);
        Check("the smaller root is 2", Near(roots.smallerSolution, 2.0f));
        Check("the larger root is 3", Near(roots.largerSolution, 3.0f));
    }

    std::cout << "\nTEST CASE: a negative a swaps the formula's roots, -x^2 + 5x - 6 = 0\n";
    {
        const PHYSICS::QuadraticRoots roots = PHYSICS::SolveQuadratic(-1.0f, 5.0f, -6.0f);

        Check("there are two roots", roots.numSolutions == 2);
        Check("the smaller root is still first", Near(roots.smallerSolution, 2.0f));
        Check("the larger root is still second", Near(roots.largerSolution, 3.0f));
    }

    std::cout << "\nTEST CASE: roots on both sides of 0, x^2 - 1 = 0 (a ray starting inside)\n";
    {
        const PHYSICS::QuadraticRoots roots = PHYSICS::SolveQuadratic(1.0f, 0.0f, -1.0f);

        Check("there are two roots", roots.numSolutions == 2);
        Check("the smaller root is -1, behind the start", Near(roots.smallerSolution, -1.0f));
        Check("the larger root is 1, ahead of the start", Near(roots.largerSolution, 1.0f));
    }

    std::cout << "\nTEST CASE: one root, x^2 + 2x + 1 = 0\n";
    {
        const PHYSICS::QuadraticRoots roots = PHYSICS::SolveQuadratic(1.0f, 2.0f, 1.0f);

        Check("there is one root", roots.numSolutions == 1);
        Check("the root is -1", Near(roots.smallerSolution, -1.0f));
        Check("one root fills both fields", roots.smallerSolution == roots.largerSolution);
    }

    std::cout << "\nTEST CASE: one root that float rounding pushes below 0, x^2 - 0.7x + 0.1225 = 0\n";
    {
        /* The exact discriminant is 0. In floats, 0.7 * 0.7 - 4 * 0.1225 comes out near -3e-8 */
        const PHYSICS::QuadraticRoots roots = PHYSICS::SolveQuadratic(1.0f, -0.7f, 0.1225f);

        Check("there is one root", roots.numSolutions == 1);
        Check("the root is 0.35", Near(roots.smallerSolution, 0.35f));
    }

    std::cout << "\nTEST CASE: no real roots, x^2 + 1 = 0\n";
    {
        const PHYSICS::QuadraticRoots roots = PHYSICS::SolveQuadratic(1.0f, 0.0f, 1.0f);

        Check("there are no roots", roots.numSolutions == 0);
    }

    std::cout << "\nTEST CASE: a = 0 is not a quadratic, 0x^2 + 0x - 1 = 0 (a ray along a can's axis)\n";
    {
        const PHYSICS::QuadraticRoots roots = PHYSICS::SolveQuadratic(0.0f, 0.0f, -1.0f);

        Check("there are no roots", roots.numSolutions == 0);
    }

    std::cout << "\nTEST CASE: a = 0 with a b, 0x^2 + 2x - 4 = 0, still no roots\n";
    {
        const PHYSICS::QuadraticRoots roots = PHYSICS::SolveQuadratic(0.0f, 2.0f, -4.0f);

        Check("there are no roots, even though the line has one at x = 2", roots.numSolutions == 0);
    }

    /* ===== Ray against sphere ===== */

    const PHYSICS::SphereCollider unitSphere{1.0f};

    std::cout << "\nTEST CASE: a ray from the front hits a sphere of radius 1\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, unitSphere);

        Check("it hits", hit.has_value());
        Check("t is 4, the distance to the near surface", hit && Near(hit->t, 4.0f));
        Check("the point is on the near surface, (0, 0, 1)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 0.0f, 1.0f)));
    }

    std::cout << "\nTEST CASE: a direction twice as long halves t but hits the same point\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -2.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, unitSphere);

        Check("it hits", hit.has_value());
        Check("t is 2", hit && Near(hit->t, 2.0f));
        Check("the point is still (0, 0, 1)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 0.0f, 1.0f)));
    }

    std::cout << "\nTEST CASE: the default collider has radius 0.5\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, PHYSICS::SphereCollider{});

        Check("t is 4.5", hit && Near(hit->t, 4.5f));
    }

    std::cout << "\nTEST CASE: a ray passing beside the sphere misses\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(3.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, unitSphere).has_value());
    }

    std::cout << "\nTEST CASE: a ray just touching the edge hits once\n";
    {
        /* a = 1, b = -10, c = 25: the discriminant is exactly 0 */
        const PHYSICS::Ray ray{MATH::Vec3(1.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, unitSphere);

        Check("it hits", hit.has_value());
        Check("t is 5", hit && Near(hit->t, 5.0f));
        Check("the point is the side of the sphere, (1, 0, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a sphere behind the ray is a miss\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, 1.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, unitSphere).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting inside ignores the sphere\n";
    {
        const PHYSICS::Ray fromCentre{MATH::Vec3(0.0f, 0.0f, 0.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const PHYSICS::Ray offCentre{MATH::Vec3(0.0f, 0.5f, 0.5f), MATH::Vec3(1.0f, 0.0f, 0.0f)};

        Check("from the centre, it misses", !PHYSICS::Intersect(fromCentre, unitSphere).has_value());
        Check("from off the centre, it misses", !PHYSICS::Intersect(offCentre, unitSphere).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting on the surface\n";
    {
        /* Heading in: the roots are 0 and 2, so the entry is t = 0. Heading out: -2 and 0, so the entry is behind */
        const PHYSICS::Ray headingIn{MATH::Vec3(0.0f, 0.0f, 1.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const PHYSICS::Ray headingOut{MATH::Vec3(0.0f, 0.0f, 1.0f), MATH::Vec3(0.0f, 0.0f, 1.0f)};
        const std::optional<PHYSICS::RayHit> inHit = PHYSICS::Intersect(headingIn, unitSphere);

        Check("heading in, it hits at t = 0", inHit && Near(inHit->t, 0.0f));
        Check("heading out, it misses", !PHYSICS::Intersect(headingOut, unitSphere).has_value());
    }

    std::cout << "\n" << (failures ? Fail : Pass)
        << checks - failures << " of " << checks << " checks passed" << Reset << "\n";

    return failures == 0 ? 0 : 1;
}
