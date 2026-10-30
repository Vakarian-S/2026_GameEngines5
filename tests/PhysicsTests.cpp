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

    std::cout << "\n" << (failures ? Fail : Pass)
        << checks - failures << " of " << checks << " checks passed" << Reset << "\n";

    return failures == 0 ? 0 : 1;
}
