/*
 * Tests for the physics math: the ray, the quadratic solver and, later, the ray-shape tests.
 *
 * Nothing here touches SDL, Vulkan or the ECS, so this target builds on Win32 as well as x64.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>

#include "MMath.h"
#include "QMath.h"
#include "VMath.h"
#include "core/Camera.h"
#include "core/Components.h"
#include "physics/Components.h"
#include "physics/Quadratic.h"
#include "physics/Ray.h"
#include "physics/RayIntersect.h"
#include "physics/RigidBody.h"

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

    /* ===== Ray against box ===== */

    /* A different half size per axis, so a mixed up axis shows */
    const PHYSICS::BoxCollider box{MATH::Vec3(1.0f, 0.5f, 2.0f)};

    std::cout << "\nTEST CASE: a ray along -z hits the front face of a box\n";
    {
        /* A negative direction: the z slab gives t1 = 7 and t2 = 3 before the swap */
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, box);

        Check("it hits", hit.has_value());
        Check("t is 3, the distance to the z = 2 face", hit && Near(hit->t, 3.0f));
        Check("the point is on that face, (0, 0, 2)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 0.0f, 2.0f)));
    }

    std::cout << "\nTEST CASE: a ray along +x hits the left face\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(-4.0f, 0.2f, 0.5f), MATH::Vec3(1.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, box);

        Check("t is 3", hit && Near(hit->t, 3.0f));
        Check("the point is (-1, 0.2, 0.5)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(-1.0f, 0.2f, 0.5f)));
    }

    std::cout << "\nTEST CASE: a slanted ray enters through the bottom face, the later slab entry wins\n";
    {
        /* The x slab is entered at t = -10, the y slab at t = 2.5, so tmin = 2.5 */
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, -3.0f, 0.0f), MATH::Vec3(0.1f, 1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, box);

        Check("t is 2.5", hit && Near(hit->t, 2.5f));
        Check("the point is on the y = -0.5 face, (0.25, -0.5, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.25f, -0.5f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a direction twice as long halves t\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -2.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, box);

        Check("t is 1.5", hit && Near(hit->t, 1.5f));
        Check("the point is still (0, 0, 2)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 0.0f, 2.0f)));
    }

    std::cout << "\nTEST CASE: the default box has half size 0.5\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, PHYSICS::BoxCollider{});

        Check("t is 4.5", hit && Near(hit->t, 4.5f));
    }

    std::cout << "\nTEST CASE: a ray that leaves one slab before entering another misses\n";
    {
        /* The y slab runs from t = 0.5 to 1.5, the x slab from t = 3 to 5: they never overlap */
        const PHYSICS::Ray ray{MATH::Vec3(-4.0f, -1.0f, 0.0f), MATH::Vec3(1.0f, 1.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, box).has_value());
    }

    std::cout << "\nTEST CASE: a ray parallel to a slab and outside it misses\n";
    {
        /* The y part of the direction is 0 and y = 2 is outside -0.5..0.5 */
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 2.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, box).has_value());
    }

    std::cout << "\nTEST CASE: a ray sliding along a face, parallel to it, still hits\n";
    {
        /* x = 1 sits exactly on the slab plane. Without the parallel branch this is 0 / 0, a NaN */
        const PHYSICS::Ray ray{MATH::Vec3(1.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, box);

        Check("it hits", hit.has_value());
        Check("t is 3, at the edge (1, 0, 2)",
              hit && Near(hit->t, 3.0f) && Near(hit->intersectionPosition, MATH::Vec3(1.0f, 0.0f, 2.0f)));
    }

    std::cout << "\nTEST CASE: a box behind the ray is a miss\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, 1.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, box).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting inside ignores the box\n";
    {
        const PHYSICS::Ray fromCentre{MATH::Vec3(0.0f, 0.0f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};
        const PHYSICS::Ray slanted{MATH::Vec3(0.5f, 0.2f, -1.0f), MATH::Vec3(-1.0f, 0.5f, 2.0f)};

        Check("from the centre, it misses", !PHYSICS::Intersect(fromCentre, box).has_value());
        Check("from off the centre, slanted, it misses", !PHYSICS::Intersect(slanted, box).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting on a face\n";
    {
        /* Heading in: the z slab gives 0 and 4, so the entry is t = 0. Heading out: -4 and 0 */
        const PHYSICS::Ray headingIn{MATH::Vec3(0.0f, 0.0f, 2.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const PHYSICS::Ray headingOut{MATH::Vec3(0.0f, 0.0f, 2.0f), MATH::Vec3(0.0f, 0.0f, 1.0f)};
        const std::optional<PHYSICS::RayHit> inHit = PHYSICS::Intersect(headingIn, box);

        Check("heading in, it hits at t = 0", inHit && Near(inHit->t, 0.0f));
        Check("heading out, it misses", !PHYSICS::Intersect(headingOut, box).has_value());
    }

    std::cout << "\nTEST CASE: a ray aimed at a corner enters all three slabs at once\n";
    {
        /* Every slab is entered at t = 1, at the corner (-1, -0.5, -2) */
        const PHYSICS::Ray ray{MATH::Vec3(-2.0f, -1.5f, -4.0f), MATH::Vec3(1.0f, 1.0f, 2.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, box);

        Check("it hits", hit.has_value());
        Check("t is 1", hit && Near(hit->t, 1.0f));
        Check("the point is the corner (-1, -0.5, -2)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(-1.0f, -0.5f, -2.0f)));
    }

    /* ===== Ray against can ===== */

    /* Radius 1, caps at y = -2 and y = 2 */
    const PHYSICS::CylinderCollider can{1.0f, 2.0f};

    std::cout << "\nTEST CASE: a ray from the side hits the wall of a can\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(-1.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, can);

        Check("it hits", hit.has_value());
        Check("t is 4, the distance to the wall", hit && Near(hit->t, 4.0f));
        Check("the point is on the wall, (1, 0, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a direction twice as long halves t but hits the same point of the can\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(-2.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, can);

        Check("t is 2", hit && Near(hit->t, 2.0f));
        Check("the point is still (1, 0, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: the default can has radius 0.5\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(-1.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, PHYSICS::CylinderCollider{});

        Check("t is 4.5", hit && Near(hit->t, 4.5f));
    }

    std::cout << "\nTEST CASE: a ray down the axis hits the top cap, the case the notes lose\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 5.0f, 0.0f), MATH::Vec3(0.0f, -1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, can);

        Check("it hits", hit.has_value());
        Check("t is 3", hit && Near(hit->t, 3.0f));
        Check("the point is the top cap's centre, (0, 2, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 2.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray up along the axis, off centre, hits the bottom cap\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.5f, -5.0f, 0.0f), MATH::Vec3(0.0f, 1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, can);

        Check("t is 3", hit && Near(hit->t, 3.0f));
        Check("the point is on the bottom cap, (0.5, -2, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.5f, -2.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray parallel to the axis, outside the radius, misses\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(3.0f, 5.0f, 0.0f), MATH::Vec3(0.0f, -1.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, can).has_value());
    }

    std::cout << "\nTEST CASE: a ray above the can, heading up the axis, misses\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 5.0f, 0.0f), MATH::Vec3(0.0f, 1.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, can).has_value());
    }

    std::cout << "\nTEST CASE: a slanted ray enters the tube above the can and lands on the top cap\n";
    {
        /* The tube is entered at t = 2, at height 3. The top cap is crossed at t = 3 */
        const PHYSICS::Ray ray{MATH::Vec3(-3.0f, 5.0f, 0.0f), MATH::Vec3(1.0f, -1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, can);

        Check("t is 3", hit && Near(hit->t, 3.0f));
        Check("the point is (0, 2, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 2.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a slanted ray enters the tube below the can and lands on the bottom cap\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(-3.0f, -5.0f, 0.0f), MATH::Vec3(1.0f, 1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, can);

        Check("t is 3", hit && Near(hit->t, 3.0f));
        Check("the point is (0, -2, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, -2.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a flat ray above the can crosses the tube and misses\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(-3.0f, 4.0f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, can).has_value());
    }

    std::cout << "\nTEST CASE: a ray crossing the tube above the can leaves it before the cap's height\n";
    {
        /* It reaches height 2 at t = 20, at x = 18, outside the disc */
        const PHYSICS::Ray ray{MATH::Vec3(-2.0f, 4.0f, 0.0f), MATH::Vec3(1.0f, -0.1f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, can).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting above the can, within the radius, leaves the tube first\n";
    {
        /* It reaches height 2 at t = 10, at x = 10, outside the disc */
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 3.0f, 0.0f), MATH::Vec3(1.0f, -0.1f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, can).has_value());
    }

    std::cout << "\nTEST CASE: a ray passing beside the can misses\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(3.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, can).has_value());
    }

    std::cout << "\nTEST CASE: a ray just touching the wall hits once\n";
    {
        /* a = 1, b = -10, c = 25: the discriminant is exactly 0 */
        const PHYSICS::Ray ray{MATH::Vec3(1.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, can);

        Check("it hits", hit.has_value());
        Check("t is 5", hit && Near(hit->t, 5.0f));
        Check("the point is (1, 0, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a can behind the ray is a miss\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, can).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting inside ignores the can\n";
    {
        const PHYSICS::Ray fromCentre{MATH::Vec3(0.0f, 0.0f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};
        const PHYSICS::Ray slanted{MATH::Vec3(0.2f, 1.0f, 0.2f), MATH::Vec3(0.0f, 1.0f, 1.0f)};
        const PHYSICS::Ray alongAxis{MATH::Vec3(0.0f, 0.0f, 0.0f), MATH::Vec3(0.0f, 1.0f, 0.0f)};

        Check("from the centre, it misses", !PHYSICS::Intersect(fromCentre, can).has_value());
        Check("from off the centre, slanted, it misses", !PHYSICS::Intersect(slanted, can).has_value());
        Check("along the axis, it misses", !PHYSICS::Intersect(alongAxis, can).has_value());
    }

    std::cout << "\nTEST CASE: a ray hitting the rim, where the wall meets the top cap\n";
    {
        /* The tube is entered at t = 1, at height 4 above the bottom cap, exactly the can's height */
        const PHYSICS::Ray ray{MATH::Vec3(-2.0f, 3.0f, 0.0f), MATH::Vec3(1.0f, -1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, can);

        Check("t is 1", hit && Near(hit->t, 1.0f));
        Check("the point is the rim, (-1, 2, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(-1.0f, 2.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray starting on the can's surface counts as starting inside\n";
    {
        /* c = 0 and a height within the caps. The sphere and the box report t = 0 here when heading in */
        const PHYSICS::Ray onWall{MATH::Vec3(1.0f, 0.0f, 0.0f), MATH::Vec3(-1.0f, 0.0f, 0.0f)};
        const PHYSICS::Ray onCap{MATH::Vec3(0.0f, 2.0f, 0.0f), MATH::Vec3(0.0f, -1.0f, 0.0f)};

        Check("on the wall, heading in, it misses", !PHYSICS::Intersect(onWall, can).has_value());
        Check("on the cap, heading in, it misses", !PHYSICS::Intersect(onCap, can).has_value());
    }

    /* ===== Ray against capsule ===== */

    /* Radius 1, sphere caps centred at y = -2 and y = 2, so it runs from y = -3 to y = 3 */
    const PHYSICS::CapsuleCollider capsule{1.0f, 2.0f};

    std::cout << "\nTEST CASE: a ray from the side hits the wall of a capsule\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(-1.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("it hits", hit.has_value());
        Check("t is 4, the distance to the wall", hit && Near(hit->t, 4.0f));
        Check("the point is on the wall, (1, 0, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a direction twice as long halves t but hits the same point of the capsule\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(-2.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is 2", hit && Near(hit->t, 2.0f));
        Check("the point is still (1, 0, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: the default capsule has radius 0.5\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(-1.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, PHYSICS::CapsuleCollider{});

        Check("t is 4.5", hit && Near(hit->t, 4.5f));
    }

    std::cout << "\nTEST CASE: a ray down the axis hits the top of the top sphere cap\n";
    {
        /* The point must come back in the capsule's space, not the moved ray's: y = 3, not y = 1 */
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 5.0f, 0.0f), MATH::Vec3(0.0f, -1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("it hits", hit.has_value());
        Check("t is 2", hit && Near(hit->t, 2.0f));
        Check("the point is (0, 3, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 3.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray up the axis hits the bottom of the bottom sphere cap\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, -5.0f, 0.0f), MATH::Vec3(0.0f, 1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is 2", hit && Near(hit->t, 2.0f));
        Check("the point is (0, -3, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, -3.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray parallel to the axis, off centre, lands on the curve of the sphere cap\n";
    {
        /* Moved to the sphere cap, the start is (0.6, 3, 0): t = 3 - sqrt(1 - 0.36) = 2.2 */
        const PHYSICS::Ray ray{MATH::Vec3(0.6f, 5.0f, 0.0f), MATH::Vec3(0.0f, -1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is 2.2", hit && Near(hit->t, 2.2f));
        Check("the point is (0.6, 2.8, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(0.6f, 2.8f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray parallel to the axis, outside the radius, misses the capsule\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(3.0f, 5.0f, 0.0f), MATH::Vec3(0.0f, -1.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, capsule).has_value());
    }

    std::cout << "\nTEST CASE: a ray above the capsule, heading up the axis, misses\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 5.0f, 0.0f), MATH::Vec3(0.0f, 1.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, capsule).has_value());
    }

    std::cout << "\nTEST CASE: a ray from beside the capsule enters the tube below it, the bottom sphere cap counts\n";
    {
        /* It starts between the sphere caps, so only where it enters the tube can pick the right one */
        const PHYSICS::Ray ray{MATH::Vec3(-3.0f, 0.0f, 0.0f), MATH::Vec3(1.0f, -1.05f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("it hits", hit.has_value());
        Check("t is about 2.0056", hit && Near(hit->t, 2.005624f));
        Check("the point is on the bottom sphere cap",
              hit && Near(hit->intersectionPosition, MATH::Vec3(-0.994376f, -2.105905f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray from beside the capsule enters the tube above it, the top sphere cap counts\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(-3.0f, 0.0f, 0.0f), MATH::Vec3(1.0f, 1.05f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is about 2.0056", hit && Near(hit->t, 2.005624f));
        Check("the point is on the top sphere cap",
              hit && Near(hit->intersectionPosition, MATH::Vec3(-0.994376f, 2.105905f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a slanted ray from above enters the tube above the capsule and lands on the top\n";
    {
        /* The tube is entered at t = 2. The sphere cap's roots are 3 and 4 */
        const PHYSICS::Ray ray{MATH::Vec3(-3.0f, 6.0f, 0.0f), MATH::Vec3(1.0f, -1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is 3", hit && Near(hit->t, 3.0f));
        Check("the point is (0, 3, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 3.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a flat ray across the curve of the top sphere cap hits it\n";
    {
        /* Moved to the sphere cap, a = 1, b = -6, c = 8.81 */
        const PHYSICS::Ray ray{MATH::Vec3(-3.0f, 2.9f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is about 2.5641", hit && Near(hit->t, 2.564110f));
        Check("the point is (-0.4359, 2.9, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(-0.435890f, 2.9f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a flat ray above the capsule crosses the tube and misses\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(-3.0f, 3.5f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, capsule).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting above the capsule, within the radius, can reach the sphere cap\n";
    {
        /* Outside the sphere cap but inside the tube, aimed at the centre (0, 2, 0) from the corner */
        const PHYSICS::Ray ray{MATH::Vec3(0.9f, 2.9f, 0.0f), MATH::Vec3(-1.0f, -1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is about 0.1929", hit && Near(hit->t, 0.192893f));
        Check("the point is on the sphere cap, (0.7071, 2.7071, 0)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.707107f, 2.707107f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray starting above the capsule, within the radius, leaves the tube first\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(0.9f, 4.0f, 0.0f), MATH::Vec3(1.0f, -0.1f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, capsule).has_value());
    }

    std::cout << "\nTEST CASE: a ray passing beside the capsule misses\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(3.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, capsule).has_value());
    }

    std::cout << "\nTEST CASE: a ray just touching the capsule's wall hits once\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(1.0f, 0.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is 5", hit && Near(hit->t, 5.0f));
        Check("the point is (1, 0, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a ray just touching the top of the sphere cap hits once\n";
    {
        /* It enters the tube above the sphere cap's centre, then the sphere cap's discriminant is exactly 0 */
        const PHYSICS::Ray ray{MATH::Vec3(0.0f, 3.0f, 5.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(ray, capsule);

        Check("t is 5", hit && Near(hit->t, 5.0f));
        Check("the point is (0, 3, 0)", hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 3.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a capsule behind the ray is a miss\n";
    {
        const PHYSICS::Ray ray{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};

        Check("it misses", !PHYSICS::Intersect(ray, capsule).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting inside ignores the capsule\n";
    {
        const PHYSICS::Ray fromCentre{MATH::Vec3(0.0f, 0.0f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};
        const PHYSICS::Ray alongAxis{MATH::Vec3(0.0f, 0.0f, 0.0f), MATH::Vec3(0.0f, 1.0f, 0.0f)};
        const PHYSICS::Ray inTopCap{MATH::Vec3(0.0f, 2.5f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};
        const PHYSICS::Ray inBottomCap{MATH::Vec3(0.0f, -2.5f, 0.0f), MATH::Vec3(0.0f, -1.0f, 0.0f)};

        Check("from the centre, it misses", !PHYSICS::Intersect(fromCentre, capsule).has_value());
        Check("along the axis, it misses", !PHYSICS::Intersect(alongAxis, capsule).has_value());
        Check("inside the top sphere cap, it misses", !PHYSICS::Intersect(inTopCap, capsule).has_value());
        Check("inside the bottom sphere cap, it misses", !PHYSICS::Intersect(inBottomCap, capsule).has_value());
    }

    std::cout << "\nTEST CASE: a ray starting on the capsule's surface and heading in\n";
    {
        /* On the wall, c = 0 counts as inside, like the can. On a sphere cap, the sphere's rule gives t = 0 */
        const PHYSICS::Ray onWall{MATH::Vec3(1.0f, 0.0f, 0.0f), MATH::Vec3(-1.0f, 0.0f, 0.0f)};
        const PHYSICS::Ray onSphereCap{MATH::Vec3(0.0f, 3.0f, 0.0f), MATH::Vec3(0.0f, -1.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> capHit = PHYSICS::Intersect(onSphereCap, capsule);

        Check("on the wall, it misses", !PHYSICS::Intersect(onWall, capsule).has_value());
        Check("on the sphere cap, it hits at t = 0", capHit && Near(capHit->t, 0.0f));
    }

    /* ===== World ray to local ray ===== */

    const MATH::Vec3 yAxis(0.0f, 1.0f, 0.0f);
    const MATH::Vec3 zAxis(0.0f, 0.0f, 1.0f);

    std::cout << "\nTEST CASE: a default transform leaves the ray as it is\n";
    {
        const PHYSICS::Ray world{MATH::Vec3(1.0f, 2.0f, 3.0f), MATH::Vec3(0.0f, 0.0f, -2.0f)};
        const PHYSICS::Ray local = PHYSICS::ToLocalRay(world, Transform{});

        Check("the origin is unchanged", Near(local.origin, world.origin));
        Check("the direction is unchanged", Near(local.direction, world.direction));
    }

    std::cout << "\nTEST CASE: a moved shape changes the origin, never the direction\n";
    {
        const PHYSICS::Ray world{MATH::Vec3(4.0f, 2.0f, 3.0f), MATH::Vec3(-1.0f, 0.0f, 0.0f)};
        const Transform transform{.position = MATH::Vec3(1.0f, 2.0f, 3.0f)};
        const PHYSICS::Ray local = PHYSICS::ToLocalRay(world, transform);

        Check("the origin is measured from the shape, (3, 0, 0)", Near(local.origin, MATH::Vec3(3.0f, 0.0f, 0.0f)));
        Check("the direction is unchanged", Near(local.direction, world.direction));
    }

    std::cout << "\nTEST CASE: a scaled shape divides both the origin and the direction\n";
    {
        const PHYSICS::Ray world{MATH::Vec3(0.0f, 0.0f, 10.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const Transform transform{.scale = 2.0f};
        const PHYSICS::Ray local = PHYSICS::ToLocalRay(world, transform);

        Check("the origin is (0, 0, 5)", Near(local.origin, MATH::Vec3(0.0f, 0.0f, 5.0f)));
        Check("the direction is (0, 0, -0.5)", Near(local.direction, MATH::Vec3(0.0f, 0.0f, -0.5f)));
    }

    std::cout << "\nTEST CASE: a shape turned 90 degrees about y turns the ray back\n";
    {
        /* The turn takes the shape's +x to the world's -z, so the world's -z is the shape's +x */
        const PHYSICS::Ray world{MATH::Vec3(0.0f, 0.0f, -5.0f), MATH::Vec3(0.0f, 0.0f, 1.0f)};
        const Transform transform{.orientation = MATH::QMath::angleAxisRotation(90.0f, yAxis)};
        const PHYSICS::Ray local = PHYSICS::ToLocalRay(world, transform);

        Check("the origin is (5, 0, 0)", Near(local.origin, MATH::Vec3(5.0f, 0.0f, 0.0f)));
        Check("the direction is (-1, 0, 0)", Near(local.direction, MATH::Vec3(-1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: ModelMatrix takes a local ray's points back to the world ray's points\n";
    {
        /* Moved, scaled and turned about a slanted axis, so no term can cancel by luck */
        const Transform transform{
            .position = MATH::Vec3(1.0f, -2.0f, 3.0f), .scale = 2.0f,
            .orientation = MATH::QMath::angleAxisRotation(30.0f, MATH::Vec3(1.0f, 2.0f, 3.0f))
        };
        const PHYSICS::Ray world{MATH::Vec3(-4.0f, 5.0f, 6.0f), MATH::Vec3(1.0f, -0.5f, 2.0f)};
        const PHYSICS::Ray local = PHYSICS::ToLocalRay(world, transform);
        const MATH::Matrix4 model = ModelMatrix(transform);

        for (const float t : {0.0f, 1.0f, 2.5f})
        {
            const MATH::Vec3 back(model * MATH::Vec4(local.PointAt(t), 1.0f));
            Check("the same t gives the same point in both spaces", Near(back, world.PointAt(t)));
        }
    }

    std::cout << "\nTEST CASE: a moved, scaled sphere gives the same t as the world's numbers\n";
    {
        /* World radius 2 at z = -10, so the near surface is at z = -8 */
        const Transform transform{.position = MATH::Vec3(0.0f, 0.0f, -10.0f), .scale = 2.0f};
        const PHYSICS::Ray world{MATH::Vec3(0.0f, 0.0f, 0.0f), MATH::Vec3(0.0f, 0.0f, -1.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(PHYSICS::ToLocalRay(world, transform), unitSphere);

        Check("t is 8", hit && Near(hit->t, 8.0f));
        Check("the local point is the sphere's near side, (0, 0, 1)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 0.0f, 1.0f)));
        Check("ModelMatrix takes it to the world's (0, 0, -8)",
              hit && Near(MATH::Vec3(ModelMatrix(transform) * MATH::Vec4(hit->intersectionPosition, 1.0f)),
                          MATH::Vec3(0.0f, 0.0f, -8.0f)));
    }

    std::cout << "\nTEST CASE: a moved, turned, scaled box shows its long side to the ray\n";
    {
        /* The turn puts the box's z half size of 2 along the world's x. At scale 2 that is 4, so the near face is at
         * x = 6. Without the turn it would be the x half size, and t would be 8 */
        const Transform transform{
            .position = MATH::Vec3(10.0f, 0.0f, 0.0f), .scale = 2.0f,
            .orientation = MATH::QMath::angleAxisRotation(90.0f, yAxis)
        };
        const PHYSICS::Ray world{MATH::Vec3(0.0f, 0.0f, 0.0f), MATH::Vec3(1.0f, 0.0f, 0.0f)};
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(PHYSICS::ToLocalRay(world, transform), box);

        Check("t is 6", hit && Near(hit->t, 6.0f));
        Check("the local point is on the z = -2 face, (0, 0, -2)",
              hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 0.0f, -2.0f)));
    }

    std::cout << "\nTEST CASE: a can and a capsule lying on their side, a ray along the world's x\n";
    {
        /* Turned 90 degrees about z, their y axis lies along the world's x. The can's cap faces the ray at x = 2,
         * the capsule's sphere cap reaches x = 3 */
        const Transform transform{.orientation = MATH::QMath::angleAxisRotation(90.0f, zAxis)};
        const PHYSICS::Ray world{MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(-1.0f, 0.0f, 0.0f)};
        const PHYSICS::Ray local = PHYSICS::ToLocalRay(world, transform);
        const std::optional<PHYSICS::RayHit> canHit = PHYSICS::Intersect(local, can);
        const std::optional<PHYSICS::RayHit> capsuleHit = PHYSICS::Intersect(local, capsule);

        Check("the can's t is 3", canHit && Near(canHit->t, 3.0f));
        Check("the capsule's t is 2", capsuleHit && Near(capsuleHit->t, 2.0f));
    }

    /* ===== Mouse to world ray ===== */

    /* At 90 degrees, tan(fov / 2) is 1, so the window's edges sit one unit out per unit forward */
    const MATH::Vec2 window(800.0f, 600.0f);
    Camera wideCamera;
    wideCamera.fovYDegrees = 90.0f;

    std::cout << "\nTEST CASE: a click at the centre, with a camera at the origin, looks down -z\n";
    {
        const PHYSICS::Ray ray = PHYSICS::MakePickRay(MATH::Vec2(400.0f, 300.0f), window, wideCamera);

        Check("the ray starts at the camera", Near(ray.origin, MATH::Vec3(0.0f, 0.0f, 0.0f)));
        Check("the direction is (0, 0, -1)", Near(ray.direction, MATH::Vec3(0.0f, 0.0f, -1.0f)));
    }

    std::cout << "\nTEST CASE: clicks on the window's edges, so y is the right way up and the aspect counts\n";
    {
        const PHYSICS::Ray top = PHYSICS::MakePickRay(MATH::Vec2(400.0f, 0.0f), window, wideCamera);
        const PHYSICS::Ray bottom = PHYSICS::MakePickRay(MATH::Vec2(400.0f, 600.0f), window, wideCamera);
        const PHYSICS::Ray right = PHYSICS::MakePickRay(MATH::Vec2(800.0f, 300.0f), window, wideCamera);

        Check("the top edge looks up, pixel y 0 is the top",
              Near(top.direction, MATH::VMath::normalize(MATH::Vec3(0.0f, 1.0f, -1.0f))));
        Check("the bottom edge looks down",
              Near(bottom.direction, MATH::VMath::normalize(MATH::Vec3(0.0f, -1.0f, -1.0f))));
        Check("the right edge is wider by the aspect, 4 / 3",
              Near(right.direction, MATH::Vec3(0.8f, 0.0f, -0.6f)));
        Check("the direction has length 1", Near(MATH::VMath::mag(right.direction), 1.0f));
    }

    std::cout << "\nTEST CASE: a click at the centre follows a moved and turned camera\n";
    {
        Camera camera;
        camera.view = MATH::MMath::lookAt(MATH::Vec3(5.0f, 0.0f, 0.0f), MATH::Vec3(0.0f, 0.0f, 0.0f), yAxis);
        const PHYSICS::Ray ray = PHYSICS::MakePickRay(MATH::Vec2(400.0f, 300.0f), window, camera);

        Check("the ray starts at the camera, (5, 0, 0)", Near(ray.origin, MATH::Vec3(5.0f, 0.0f, 0.0f)));
        Check("the direction faces where it looks, (-1, 0, 0)", Near(ray.direction, MATH::Vec3(-1.0f, 0.0f, 0.0f)));
    }

    std::cout << "\nTEST CASE: a click on a point projected the way the GPU does it aims back at that point\n";
    {
        const MATH::Vec2 size(1280.0f, 720.0f);
        Camera camera;
        camera.fovYDegrees = 60.0f;
        camera.view = MATH::MMath::lookAt(MATH::Vec3(3.0f, 4.0f, 5.0f), MATH::Vec3(0.0f, 1.0f, 0.0f), yAxis);

        /* The projection VulkanRenderer hands the GPU, element 5 negated */
        MATH::Matrix4 projection = MATH::MMath::PerspectiveVulkan(camera.fovYDegrees, size.x / size.y,
                                                                  camera.nearPlane, camera.farPlane);
        projection[5] *= -1.0f;

        for (const MATH::Vec3& point : {MATH::Vec3(0.5f, 1.5f, -0.5f), MATH::Vec3(-1.0f, 0.5f, 1.0f),
                                        MATH::Vec3(2.0f, 2.0f, 0.0f)})
        {
            const MATH::Vec4 clip = projection * (camera.view * MATH::Vec4(point, 1.0f));
            const MATH::Vec2 pixel((clip.x / clip.w + 1.0f) * 0.5f * size.x, (clip.y / clip.w + 1.0f) * 0.5f * size.y);
            const PHYSICS::Ray ray = PHYSICS::MakePickRay(pixel, size, camera);

            Check("the ray points at the projected point",
                  Near(ray.direction, MATH::VMath::normalize(point - ray.origin)));
        }
    }

    std::cout << "\nTEST CASE: a whole click, mouse to world ray to local ray to a hit, gives t in metres\n";
    {
        /* World radius 2 at the origin and a camera at z = 10, so the near surface is 8 metres away */
        Camera camera;
        camera.view = MATH::MMath::lookAt(MATH::Vec3(0.0f, 0.0f, 10.0f), MATH::Vec3(0.0f, 0.0f, 0.0f), yAxis);
        const Transform transform{.scale = 2.0f};
        const PHYSICS::Ray world = PHYSICS::MakePickRay(MATH::Vec2(400.0f, 300.0f), window, camera);
        const std::optional<PHYSICS::RayHit> hit = PHYSICS::Intersect(PHYSICS::ToLocalRay(world, transform), unitSphere);

        Check("t is 8", hit && Near(hit->t, 8.0f));
        Check("the local point is (0, 0, 1)", hit && Near(hit->intersectionPosition, MATH::Vec3(0.0f, 0.0f, 1.0f)));
    }

    std::cout << "TEST CASE: gravity and drag change the velocity\n";
    {
        const MATH::Vec3 rest(0.0f, 0.0f, 0.0f);
        const MATH::Vec3 moving(10.0f, 0.0f, 0.0f);

        Check("gravity gives v.y = -9.8 * dt", Near(PHYSICS::ApplyForces(rest, 1.0f, 0.1f), MATH::Vec3(0.0f, -0.98f, 0.0f)));
        Check("gravity does not depend on mass", Near(PHYSICS::ApplyForces(rest, 5.0f, 0.1f), MATH::Vec3(0.0f, -0.98f, 0.0f)));
        Check("drag slows a 1 kg body by 0.25 * v / m * dt", Near(PHYSICS::ApplyForces(moving, 1.0f, 0.1f).x, 9.75f));
        Check("the same drag slows a 5 kg body less", Near(PHYSICS::ApplyForces(moving, 5.0f, 0.1f).x, 9.95f));
    }

    std::cout << "TEST CASE: the velocity moves the position\n";
    {
        const MATH::Vec3 moved = PHYSICS::UpdatePosition(MATH::Vec3(1.0f, 2.0f, 3.0f), MATH::Vec3(2.0f, 0.0f, -4.0f), 0.5f);

        Check("position + v * dt", Near(moved, MATH::Vec3(2.0f, 2.0f, 1.0f)));
    }

    std::cout << "TEST CASE: the angular velocity turns the orientation\n";
    {
        const MATH::Vec3 quarterTurnAboutY(0.0f, MATH::kPi * 0.5f, 0.0f);
        const MATH::Quaternion start;

        const MATH::Quaternion turned = PHYSICS::UpdateOrientation(start, quarterTurnAboutY, 1.0f);
        Check("a quarter turn about Y takes +X to -Z",
              Near(MATH::QMath::rotate(MATH::Vec3(1.0f, 0.0f, 0.0f), turned), MATH::Vec3(0.0f, 0.0f, -1.0f)));
        Check("the result has unit length", Near(MATH::QMath::magnitude(turned), 1.0f));

        /* Rolled first: the step must turn about the world's Y, so the rotation goes on the left */
        const MATH::Quaternion rolled = MATH::QMath::angleAxisRotation(90.0f, MATH::Vec3(0.0f, 0.0f, 1.0f));
        const MATH::Quaternion rolledThenTurned = PHYSICS::UpdateOrientation(rolled, quarterTurnAboutY, 1.0f);
        Check("the step multiplies on the left of the orientation",
              Near(MATH::QMath::rotate(MATH::Vec3(1.0f, 0.0f, 0.0f), rolledThenTurned), MATH::Vec3(0.0f, 1.0f, 0.0f)));

        const MATH::Quaternion still = PHYSICS::UpdateOrientation(rolled, MATH::Vec3(0.0f, 0.0f, 0.0f), 1.0f);
        Check("zero angular velocity changes nothing and does not throw",
              Near(still.w, rolled.w) && Near(still.i, rolled.i) && Near(still.j, rolled.j) && Near(still.k, rolled.k));
    }

    std::cout << "TEST CASE: the slides' mouse constraint\n";
    {
        constexpr float deltaTime = 1.0f / 60.0f;
        /* kBeta in RigidBody.cpp */
        constexpr float beta = 0.15f;
        const Transform transform{.position = MATH::Vec3(1.0f, 2.0f, 3.0f)};
        const auto length = [](const MATH::Vec3& v) { return std::sqrt(MATH::VMath::dot(v, v)); };

        /* Mass 1, identity inertia and no gap: the slides' form must give the handout's answer.
         * r = (1, 0, 0), so Meff is diag(1, 2, 2). Worked by hand */
        const PHYSICS::MassProperties unit{};
        const MATH::Vec3 arm(1.0f, 0.0f, 0.0f);
        const PHYSICS::VelocityChange known = PHYSICS::MouseConstraint(
            transform, arm, transform.position + arm, unit,
            MATH::Vec3(0.0f, -1.0f, 0.0f), MATH::Vec3(0.0f, 0.0f, 0.0f), deltaTime);
        Check("mass 1, identity, no gap: the linear change is the handout's (0, 0.5, 0)",
              Near(known.changeInLinearVel, MATH::Vec3(0.0f, 0.5f, 0.0f)));
        Check("mass 1, identity, no gap: the angular change is the handout's (0, 0, 0.5)",
              Near(known.changeInAngularVel, MATH::Vec3(0.0f, 0.0f, 0.5f)));

        /* A real body with a gap: after the change, JV + b = 0 */
        const PHYSICS::BoxCollider box{MATH::Vec3(0.5f, 1.0f, 0.25f)};
        const PHYSICS::MassProperties heavyBox{2.0f, PHYSICS::InertiaTensor(box, 2.0f, 1.0f)};
        const MATH::Vec3 localPoint(0.5f, 1.0f, 0.25f);
        const MATH::Vec3 target = transform.position + localPoint + MATH::Vec3(0.2f, -0.1f, 0.3f);
        const MATH::Vec3 velocity(1.0f, -2.0f, 0.5f);
        const MATH::Vec3 angularVelocity(0.3f, 1.0f, -0.7f);
        const PHYSICS::VelocityChange change = PHYSICS::MouseConstraint(
            transform, localPoint, target, heavyBox, velocity, angularVelocity, deltaTime);
        const MATH::Vec3 gap = transform.position + localPoint - target;
        const MATH::Vec3 pointVelocity = velocity + change.changeInLinearVel +
            MATH::VMath::cross(angularVelocity + change.changeInAngularVel, localPoint);
        Check("after the change, v + w x r = -b", length(pointVelocity + gap * (beta / deltaTime)) < 1.0e-3f);

        /* The target sits on the clicked point at the centre, so r is 0 and only the velocity can change */
        const PHYSICS::VelocityChange centre = PHYSICS::MouseConstraint(
            transform, MATH::Vec3(0.0f, 0.0f, 0.0f), transform.position, heavyBox, velocity, angularVelocity,
            deltaTime);
        Check("r = 0, no gap: the linear change cancels the velocity", Near(centre.changeInLinearVel, velocity * -1.0f));
        Check("r = 0, no gap: the angular change is 0", Near(centre.changeInAngularVel, MATH::Vec3(0.0f, 0.0f, 0.0f)));

        /* More mass with the same inertia: sliding gets harder, so less of the fix is a slide */
        const PHYSICS::MassProperties light{1.0f, MATH::Matrix3()};
        const PHYSICS::MassProperties dense{10.0f, MATH::Matrix3()};
        const PHYSICS::VelocityChange lightChange = PHYSICS::MouseConstraint(
            transform, localPoint, target, light, velocity, angularVelocity, deltaTime);
        const PHYSICS::VelocityChange denseChange = PHYSICS::MouseConstraint(
            transform, localPoint, target, dense, velocity, angularVelocity, deltaTime);
        Check("more mass, same inertia: the linear change is smaller",
              length(denseChange.changeInLinearVel) < length(lightChange.changeInLinearVel));

        /* A box rolled 30 degrees about Z must act like an unrolled body whose tensor and point
         * are turned by hand. The turned tensor is written out here, not built with toMatrix3 */
        const float c = std::cos(30.0f * MATH::kDegreesToRadians);
        const float s = std::sin(30.0f * MATH::kDegreesToRadians);
        const MATH::Matrix3 local = PHYSICS::InertiaTensor(box, 2.0f, 1.0f);
        const float a = local[0], b = local[4], z = local[8];
        MATH::Matrix3 turnedTensor;
        turnedTensor[0] = a * c * c + b * s * s;
        turnedTensor[4] = a * s * s + b * c * c;
        turnedTensor[8] = z;
        turnedTensor[1] = (a - b) * c * s;
        turnedTensor[3] = (a - b) * c * s;
        const Transform rolled{
            .position = transform.position,
            .orientation = MATH::QMath::angleAxisRotation(30.0f, MATH::Vec3(0.0f, 0.0f, 1.0f))
        };
        const MATH::Vec3 turnedPoint = MATH::QMath::rotate(localPoint, rolled.orientation);
        const PHYSICS::VelocityChange rolledChange = PHYSICS::MouseConstraint(
            rolled, localPoint, target, heavyBox, velocity, angularVelocity, deltaTime);
        const PHYSICS::VelocityChange byHand = PHYSICS::MouseConstraint(
            transform, turnedPoint, target, {2.0f, turnedTensor}, velocity, angularVelocity, deltaTime);
        Check("rolled box: the linear change matches the box turned by hand",
              length(rolledChange.changeInLinearVel - byHand.changeInLinearVel) < 1.0e-3f);
        Check("rolled box: the angular change matches the box turned by hand",
              length(rolledChange.changeInAngularVel - byHand.changeInAngularVel) < 1.0e-3f);

        /* The whole step, 10 s at 60 Hz: a 2 kg box held by a corner, mouse still and then moving 5 m in 1 s.
         * No grip, no cap and no angular drag yet */
        for (const bool moving : {false, true})
        {
            Transform body{};
            MATH::Vec3 v, w;
            float worst = 0.0f;
            float last = 0.0f;
            for (int step = 0; step < 600; ++step)
            {
                const float seconds = static_cast<float>(step) * deltaTime;
                const float slide = moving ? std::min(seconds, 1.0f) * 5.0f : 0.0f;
                const MATH::Vec3 mouse = localPoint + MATH::Vec3(slide, 0.0f, 0.0f);
                v = PHYSICS::ApplyForces(v, heavyBox.mass, deltaTime);
                const PHYSICS::VelocityChange step2 = PHYSICS::MouseConstraint(
                    body, localPoint, mouse, heavyBox, v, w, deltaTime);
                v += step2.changeInLinearVel;
                w += step2.changeInAngularVel;
                body.position = PHYSICS::UpdatePosition(body.position, v, deltaTime);
                body.orientation = PHYSICS::UpdateOrientation(body.orientation, w, deltaTime);
                last = length(body.position + MATH::QMath::rotate(localPoint, body.orientation) - mouse);
                worst = std::max(worst, last);
            }
            std::cout << "  box held by a corner, mouse " << (moving ? "moving" : "still")
                << ": final gap " << last << " m, worst " << worst << " m\n";
            Check(moving ? "moving mouse: the corner ends on the mouse" : "still mouse: the corner stays on the mouse",
                  last < 0.02f);
        }
    }

    std::cout << "TEST CASE: angular drag slows the spin\n";
    {
        constexpr float deltaTime = 1.0f / 60.0f;
        const MATH::Vec3 spin(0.0f, 2.0f, 1.0f);

        const MATH::Vec3 once = PHYSICS::ApplyAngularDrag(spin, deltaTime);
        Check("one step scales the spin by 1 - 0.25 dt", Near(once, spin * (1.0f - 0.25f * deltaTime)));
        Check("the axis does not change", Near(MATH::VMath::normalize(once), MATH::VMath::normalize(spin)));
        Check("zero stays zero",
              Near(PHYSICS::ApplyAngularDrag(MATH::Vec3(0.0f, 0.0f, 0.0f), deltaTime), MATH::Vec3(0.0f, 0.0f, 0.0f)));

        MATH::Vec3 spinning = spin;
        for (int step = 0; step < 600; ++step) spinning = PHYSICS::ApplyAngularDrag(spinning, deltaTime);
        const float left = std::sqrt(MATH::VMath::dot(spinning, spinning) / MATH::VMath::dot(spin, spin));
        std::cout << "  after 10 s, " << left * 100.0f << "% of the spin is left\n";
        Check("10 s leaves about e^-2.5 of the spin, 8.2%", std::abs(left - std::exp(-2.5f)) < 0.002f);
    }

    std::cout << "TEST CASE: grip and the spin limit on a held shape\n";
    {
        constexpr float deltaTime = 1.0f / 60.0f;
        const auto length = [](const MATH::Vec3& v) { return std::sqrt(MATH::VMath::dot(v, v)); };
        const MATH::Vec3 spin(0.0f, 2.0f, 1.0f);
        const MATH::Vec3 none(0.0f, 0.0f, 0.0f);

        const MATH::Vec3 gripped = PHYSICS::ApplyGrip(spin, deltaTime);
        Check("grip divides the spin by 1 + 7 dt", Near(gripped, spin * (1.0f / (1.0f + 7.0f * deltaTime))));
        Check("grip keeps the axis", Near(MATH::VMath::normalize(gripped), MATH::VMath::normalize(spin)));
        Check("grip leaves zero at zero", Near(PHYSICS::ApplyGrip(none, deltaTime), none));

        const MATH::Vec3 slow(3.0f, 0.0f, 4.0f);
        const MATH::Vec3 fast(12.0f, 0.0f, 16.0f);
        Check("a spin of 5 is under the limit and stays the same", Near(PHYSICS::LimitSpin(slow), slow));
        Check("a spin of 20 comes out at 12", Near(length(PHYSICS::LimitSpin(fast)), 12.0f));
        Check("the limit keeps the axis", Near(MATH::VMath::normalize(PHYSICS::LimitSpin(fast)), MATH::VMath::normalize(fast)));
        Check("the limit leaves zero at zero", Near(PHYSICS::LimitSpin(none), none));

        /* The held step as MouseConstraintSystem runs it: grip, solve, limit. A 1 kg pill held at its tip,
         * the mouse jerked 3 m sideways at once, then still for 10 s */
        const PHYSICS::CapsuleCollider pill{0.6f, 0.7f};
        const PHYSICS::MassProperties pillMass{1.0f, PHYSICS::InertiaTensor(pill, 1.0f, 1.0f)};
        const MATH::Vec3 tip(0.0f, 1.3f, 0.0f);
        const MATH::Vec3 mouse = tip + MATH::Vec3(3.0f, 0.0f, 0.0f);
        Transform body{};
        MATH::Vec3 v, w;
        float fastest = 0.0f;
        for (int step = 0; step < 600; ++step)
        {
            v = PHYSICS::ApplyForces(v, pillMass.mass, deltaTime);
            w = PHYSICS::ApplyAngularDrag(w, deltaTime);
            w = PHYSICS::ApplyGrip(w, deltaTime);
            const PHYSICS::VelocityChange change = PHYSICS::MouseConstraint(body, tip, mouse, pillMass, v, w, deltaTime);
            v += change.changeInLinearVel;
            w = PHYSICS::LimitSpin(w + change.changeInAngularVel);
            fastest = std::max(fastest, length(w));
            body.position = PHYSICS::UpdatePosition(body.position, v, deltaTime);
            body.orientation = PHYSICS::UpdateOrientation(body.orientation, w, deltaTime);
        }
        const float gap = length(body.position + MATH::QMath::rotate(tip, body.orientation) - mouse);
        std::cout << "  jerked pill: fastest spin " << fastest << " rad/s, final gap " << gap << " m\n";
        Check("the held spin never goes over 12", fastest <= 12.0f + kTolerance);
        Check("the tip still settles on the mouse", gap < 0.02f);
    }

    std::cout << "TEST CASE: the inertia tensor of each shape\n";
    {
        /* Diagonal only, so each check reads m[0], m[4] and m[8], with the rest 0 */
        const auto diagonalIs = [](const MATH::Matrix3& tensor, const MATH::Vec3& expected)
        {
            return Near(tensor[0], expected.x) && Near(tensor[4], expected.y) && Near(tensor[8], expected.z) &&
                Near(tensor[1], 0.0f) && Near(tensor[2], 0.0f) && Near(tensor[3], 0.0f) &&
                Near(tensor[5], 0.0f) && Near(tensor[6], 0.0f) && Near(tensor[7], 0.0f);
        };

        const PHYSICS::BoxCollider box{MATH::Vec3(1.0f, 2.0f, 3.0f)};
        Check("box: m/3 times the other two half sizes squared",
              diagonalIs(PHYSICS::InertiaTensor(box, 3.0f, 1.0f), MATH::Vec3(13.0f, 10.0f, 5.0f)));

        const PHYSICS::SphereCollider sphere{1.0f};
        Check("sphere: 2/5 m r^2 on every axis",
              diagonalIs(PHYSICS::InertiaTensor(sphere, 2.0f, 1.0f), MATH::Vec3(0.8f, 0.8f, 0.8f)));
        Check("scale 2 multiplies it by 4",
              diagonalIs(PHYSICS::InertiaTensor(sphere, 2.0f, 2.0f), MATH::Vec3(3.2f, 3.2f, 3.2f)));

        const PHYSICS::CylinderCollider can{1.0f, 1.0f};
        Check("can: m (3r^2 + 4h^2) / 12 across, m r^2 / 2 along Y",
              diagonalIs(PHYSICS::InertiaTensor(can, 12.0f, 1.0f), MATH::Vec3(7.0f, 6.0f, 7.0f)));
        const MATH::Matrix3 longCan = PHYSICS::InertiaTensor(PHYSICS::CylinderCollider{0.7f, 1.3f}, 1.0f, 1.0f);
        Check("a long can turns most easily about its long Y axis", longCan[4] < longCan[0]);

        const MATH::Matrix3 flatPill = PHYSICS::InertiaTensor(PHYSICS::CapsuleCollider{1.0f, 0.0f}, 2.0f, 1.0f);
        Check("a pill with no tube is a sphere", diagonalIs(flatPill, MATH::Vec3(0.8f, 0.8f, 0.8f)));

        /* Expected values from 2,000,000 random points inside this pill, so the tolerance is wider */
        const MATH::Matrix3 pill = PHYSICS::InertiaTensor(PHYSICS::CapsuleCollider{0.6f, 0.7f}, 1.0f, 1.0f);
        Check("pill matches sampling across (0.5066)", std::abs(pill[0] - 0.5066f) < 0.002f);
        Check("pill matches sampling along Y (0.1669)", std::abs(pill[4] - 0.1669f) < 0.002f);
    }

    std::cout << "\n" << (failures ? Fail : Pass)
        << checks - failures << " of " << checks << " checks passed" << Reset << "\n";

    return failures == 0 ? 0 : 1;
}
