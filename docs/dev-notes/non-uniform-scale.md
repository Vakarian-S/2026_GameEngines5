# Non-uniform scale

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed and built 2026-11-14, as the bonus at the end of step 7 of the ray query shadows plan, which
ended on 2026-11-14 (commit `bd86c2f`, together with step 7's shadow work). It was there so step 7's
floor could become a flat box. Claude built it at Vakarian's request, and he reviewed the diff. "Step
N" below means a step of that plan; README.md maps each one to its note. The plan section below is
the bonus part of step 7's section; the shadow part is in ray-query-shadows.md.

## Step 7, bonus: non-uniform scale

Independent of the shadows. The TLAS already takes any 3x4 transform, so shadows need no change.
Decided: round colliders need uniform scale. The rest is a list of what changes; it gets a short
design pass before the bonus is built:

- **Data:** `Transform.scale` goes from `float` to `MATH::Vec3` (`core/Components.h`), and the model
  matrix build passes the three components to `MMath::scale`. Every `.scale = 1.0f` in the scenes
  changes.
- **Normals:** `FrameLoop.cpp` builds the normal matrix as the model's plain 3x3. It becomes the
  inverse transpose of that 3x3, one 3x3 inverse per draw on the CPU. The push constant already holds a
  3x3 (`Pipeline.h`).
- **Physics, per axis:** colliders are sized at scale 1 and multiplied by `Transform.scale`. The pick
  ray's move into local space (`Ray.cpp`), the contact point and the box inertia tensor
  (`RigidBody.cpp`) become per axis and stay correct. `PickSystem`'s near-zero scale checks change too.
- **Round colliders need uniform scale.** A sphere, capsule or cylinder scaled unevenly is a different
  shape, and its ray test and inertia tensor assume the real one. They read one component, and a debug
  assert checks that x, y and z are equal.
- **Docs:** STATE.md, architecture rule 10, names the uniform-scale dependency and changes with it.
- **The floor:** the floor cube from decision a becomes a flat box, such as (20, 0.5, 20). A wrong normal matrix
  lights a flattened box wrong, so the floor also tests the change.

**The design pass**

- **a. Per-component `Vec3` math lives in `core/VectorMath.h`,** next to `SafeNormalize`. The math
  library has no per-component multiply or divide for `Vec3`. Architecture rule 9 asks for free
  functions on plain values, and that header has no Vulkan or ECS type, so the tests build it on Win32.
  Rejected: adding them to the external math library, which the engine does not own.
- **b. The normal matrix is the cofactor matrix, not the inverse transpose.** The library's
  `Matrix3` inverse throws in Debug when the determinant is near zero, so a tiny object would crash.
  The cofactor matrix is the inverse transpose times the determinant; the shader normalizes normals,
  so the factor drops out. It has no division.
- **c. The cylinder needs only x equal to z.** Stretched along its own Y axis it is still a cylinder.
  The sphere and the capsule need all three components equal: stretched, they become an ellipsoid and a
  capsule with half-ellipsoid ends, which their inertia formulas do not describe.

**What the build changed**

- `Transform.scale` is a `Vec3` that defaults to `(1, 1, 1)`, and `ModelMatrix` scales each axis.
- `core/VectorMath.h` gained `MultiplyComponents`, `DivideComponents` and `MinComponent`.
- `NormalMatrix(model)` in `core/Components.h` is the cofactor matrix: three cross products of the
  3x3's columns, sign-flipped when the determinant is negative so normals stay outward. It replaces
  the plain 3x3 in `FrameLoop`.
- `ToLocalRay` and the mouse constraint's arm scale per axis; `PickSystem`'s near-zero checks use
  `MinComponent`.
- The box inertia tensor scales each half size by its own axis. The sphere and capsule read one
  component through `UniformScale`, which asserts in Debug that all three match. The cylinder asserts
  only x equals z, then reads x for the radius and y for the height.
- `Scene1`'s floor is `scale (20, 0.5, 20)` at `y = -1.5`. `PhysicsScene` passes `Vec3` scales.
- STATE.md, architecture rule 10, rewritten.

**What the build checked**

- **Tests.** `scripts\run-tests.bat` passes in all four configurations. `PhysicsTests` gained 16
  checks, 218 in all: per-component math; a box scaled `(4, 1, 1)` hit at `t = 8`; the box tensor
  scaled per axis; a can scaled `(2, 3, 2)` matching the bigger can; `NormalMatrix` keeping a normal
  perpendicular on a squashed, turned surface where the plain 3x3 does not, matching the plain 3x3 under
  uniform scale, staying outward under a mirror, and working at scale 0.001.
- **Debug runs** of Scene1, PhysicsScene, Scene0 and PacingScene: exit 0, 0 warnings, 0 errors,
  0 asserts, 0 leaked blocks. The flat floor lights as the big cube did. Clicks in PhysicsScene picked
  the cylinder, a box and the sphere at scale 2.
- **Not checked:** an unevenly scaled sphere or capsule hitting the assert, since no scene has one.

**What the build found**

- **Only the inertia tensor needs real round shapes today.** Rendering, the collider wires, picking
  and the mouse constraint's arm are exact for any scale: picking moves the ray into local space, where
  a stretched sphere is the unit sphere. Shape-against-shape collision, once it exists, would need the
  real shapes too, because its fast tests assume round.
- **Scale lives in two places in `PhysicsScene`:** the `Transform` and the `InertiaTensor` argument.
  Stretching a sphere's `Transform` alone gave a correct mesh, wires and picking, but a round sphere's
  tensor, and no assert. ROADMAP.md holds the fix.
- **The inertia tensor is a feel setting as much as a physical one.** Nothing but the rotation reads
  it, so a tensor apart from the mesh changes only how an object turns when grabbed off centre.
- **Engines tie colliders to the transform, and neither stretches a sphere.** Unreal documents the
  smallest axis (`USphereComponent::GetShapeScale`); Unity's manual says only that a sphere has one
  radius, and a forum post claims the largest axis. This engine asserts in Debug instead of choosing.
- **The two normal-matrix sources disagree on the sign under a mirror.** Lengyel, "Transforming
  Normals" (https://terathon.com/blog/transforming-normals.html), and graphitemaster,
  "normals_revisited" (https://github.com/graphitemaster/normals_revisited). The engine keeps stored
  normals outward, as the inverse transpose does.
- **Review catches:** the first `Vec3` scale had no default, which would have made every unscaled
  object vanish.
