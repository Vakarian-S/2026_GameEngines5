# Transform: one component with a quaternion

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Outlined 2026-10-26, designed and built 2026-10-27 (commit `ffeeb1b`), as step 4.0 of the plan that
ended on 2026-11-03. It came before the physics scene, because a body that rotates about more than
the up axis needs somewhere to write its orientation.

## Sources

- The space flight prototype: findings/2026-10-space-prototype.md has the measurements and the
  engines it was checked against.
- Unity Entities: split components in 0.x, one `LocalTransform` from 1.0, still the shape in 6.7,
  and `PostTransformMatrix` for a stretch. Bevy and Unreal Mass also merge; flecs keeps them split.
- GLM 0.9.9.7, vendored, for the rejected option: `setup.hpp:813-818`.

## The problem

`Rotation` held one yaw angle. `DrawRenderablesSystem` built its matrix from that angle, so nothing
that pitches or rolls could be drawn. The prototype's ship needed its own draw system, the third copy
of that loop in the repository, which is what the system registry exists to stop.

## Why a component could not hold a quaternion

- `Quaternion.h` declared its own copy constructor and `operator=`, and `Vector.h` did the same for
  `Vec3`. Declaring them makes the type fail `std::is_trivially_copyable_v`, so `Register<T>()`
  failed its `static_assert` at compile time.
- Both bodies copied members one at a time, exactly as the compiler would. So the declarations cost
  nothing at runtime and bought nothing either. `Matrix4` and `Matrix3` already defaulted their copy
  constructors, so the hand-written ones are a teaching style, not a rule the library depends on.
- The math library is ours now. It stays in `external/MathLibrary`, but it will never be updated from
  its source, so no `static_assert` list guards these types against an update.

## What it does

**Nine math types became trivially copyable.** Each hand-written copy constructor and `operator=`
became `= default`, declared in the class. A copy defaulted on its first declaration is not
user-provided, so the type is trivially copyable. No behaviour changed. Each defaulted line carries
a comment saying why.

| type | stored later as |
|---|---|
| `Vec3` | `Transform.position`, `Velocity` |
| `Vec4` | a colour, a homogeneous point |
| `Vec2` | UVs, screen points for picking |
| `Quaternion` | `Transform.orientation` |
| `Matrix4` (only `operator=`) | `StaticMatrix` |
| `Matrix3` (only `operator=`) | a rigid body's inertia tensor |
| `Matrix2` | nothing yet; kept with its family |
| `Sphere` | a bounding sphere for frustum culling |
| `Plane` | frustum planes |

- `Ray` holds two `Vec3` and declares no copies, so it follows `Vec3` with no edit.
- The conversions stay: `Vec3` from `Vec4`, and `Matrix3` from `Matrix4`. They are not copies. The
  identity default constructors stay too.
- Left alone: `Triangle` and `Quad`, whose copies re-check the shape in Debug, which is real
  behaviour. `Randomizer`, which owns state and deletes its copies on purpose. `DualQuat` and
  `Euler`, which nothing plans to store. `Point2d`, `Line2d`, `Flector2d` and `Motor2d`, which the
  engine does not use.

**`Position` and `Rotation` became one `Transform`:** a `Vec3` position (12 bytes), a uniform `float`
scale of 1 by default (4 bytes), and a `MATH::Quaternion` orientation, identity by default (16
bytes). The yaw angle went away.

- The draw system still builds translate × rotate × scale in Extract every frame.
- One component, not three: a query has no optional components, so with three every drawn entity
  carries all three anyway, and each extra component is one more migration per create (about 8 per
  create, in the space prototype).
- Uniform scale only, so a sphere collider stays a sphere. A mesh that ever needs a stretch gets its
  own component, as Unity's `PostTransformMatrix` does. That component would break one assumption:
  the renderer skips the normal matrix's inverse-transpose because scale is uniform.
- `Spin` kept its meaning: it still turned an entity about the up axis, by multiplying a quaternion
  step instead of adding to an angle. Step 4c later deleted `Spin`: see dangle-physics.md.

**A const `World::GetComponent`.** A duplicate of the mutable one, returning `const T*`, with no
`const_cast`, the same way `Archetype::Column<T>()` has both. Inside a const `World` method,
`archetypes[i]` gives `Archetype* const&`: the pointer is const, the archetype is not. So a plain
`->Column<T>()` would call the mutable `Column`. The body reads the archetype through a
`const Archetype*`, so the const method takes the const path.

**The zero-bytes rule.** It now lives in living/ECS.md, under "Rules of this engine", with one
assert in `World::AddComponentById`.

## Rejected

- GLM: in the vendored 0.9.9.7 a `glm::quat` is either trivially copyable with `{}` giving the zero
  quaternion, or identity by default with hand-written copies (`GLM_FORCE_CTOR_INIT`). Never both.
  This library gives both once its copies are defaulted.
- Three components for position, orientation and scale: see the reason above.
- The world matrix as a component. The source of truth stays position, orientation and scale. 4,001
  matrices built in Extract cost 0.124 ms in the prototype, so there is nothing to save yet.
  ROADMAP.md holds the `StaticMatrix` decision for later, and the two alternatives that were
  rejected.

## What the build found

- **Two more files used `Position`.** `bench/EcsBench.cpp` and `demo/ArenaDemo.cpp` included
  `game/Components.h`. Their targets do not see the math library, so the full build broke. Each now
  defines its own `Position` and `Velocity`, and the bench its own `Hit`, the way `EcsTests` does.
  The bench keeps a 12-byte `Position`, so its numbers still compare with earlier runs.
- **Spin's order.** The step goes on the left, `normalize(step * orientation)`, so it turns about the
  world's up axis. `*=` in `Quaternion.h` computes `q * this`, the reverse of what it looks like, so
  the code writes the product out in full.
- **The assert covers the size too.** `AddComponentById` returned early after `MoveEntity` on
  `size == 0` as well as on `nullptr`, so a value passed with size 0 skipped the size check and left
  a zeroed row. A scratch program showed it. The assert checks both, before `MoveEntity`.
- **The const `GetComponent` had no caller.** A scratch compile showed it returns `const T*`, refuses
  writes and reads through the const `Column`. No `EcsTests` check called it.
- **The roll check stays in the scene.** Scene1's left cube is rolled 30 degrees about Z after its 45
  degree yaw, which `Rotation` could not express. A misplaced parenthesis on the first try passed
  the Z axis through the library's `Vec3 * Quaternion` teaching operator and lost the yaw. The cube
  still looked tilted, so check that both cubes show the same two faces.
- **Checked.** The full build has no warnings. `run-tests.bat` passed in all four configurations.
  Scene0 and Scene1 ran with no `[WARNING]` or `[ERROR]` and 0 leaked blocks.
