# Physics scene: colliders, their wires, and picking

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-10-27 and built 2026-10-28 (4a, commits `866e096` to `832b8e3` plus the toggles), then
designed 2026-10-28 and built 2026-10-30 (4b, picking), as step 4 of the plan that ended on
2026-11-03. Step 4c, the dangle, built on the same scene: see dangle-physics.md. Course work: Game
Physics 3, assignment 2, "Mouse Picking".

## Sources

- The handout: `GAME_309_Assignment2_Mouse_Picking_Noor_f25.pdf`, and the ray-cylinder notes, parts
  1 and 2. The handout's theory is kept; its class shapes are not.
- Ericson, *Real-Time Collision Detection*, 5.3.2, for the rejected short sphere test.
- Graphics Gems IV, Cychosz and Waggenspack, `ray_cyl.c`, as a check for the can.
- Jolt's `DebugRenderer`, for wire helpers that take a matrix.
- For collider scale: Unity Physics, Unity's GameObject colliders, Bevy with Rapier, and Stride. For
  collider sizes: Rapier, Jolt, Avian and Unity Physics. For a ray starting inside: Unity's
  `Physics.Raycast`, Godot, Jolt and Rapier. For switching systems: Unity's `SystemState.Enabled`,
  Bevy's `run_if` and Unreal Mass's `MakeInactive`. For ray values: Unity Physics' `RaycastInput`
  and Avian's `RayCaster`. Overwatch, for a command per simulation tick.
- The prototype game: findings/2026-10-prototype-game.md, for the inverse that crashed Debug.

## What it does

`PhysicsScene` sits at F3 and holds six entities in a row. Each one draws its mesh with a green wire
of its collider over it. C toggles the wires and M toggles the meshes. A left click casts a ray from
the mouse, prints the nearest shape with its `t` and the world point, and turns that shape's wire
yellow. A click on empty space clears it. The pick ray stays drawn, with a cross where it hit. A
camera inside a collider clicks through it.

| # | mesh | collider | shows |
|---|---|---|---|
| 1 | cube | box, defaults | a perfect fit: unit mesh, unit collider |
| 2 | Mario | sphere | a collider approximating a detailed mesh |
| 3 | Mario | capsule | the pill's straight part and round ends |
| 4 | Mario | cylinder | the can's end circles and side lines |
| 5 | cube | box, spinning and rolled | the wire turning with the mesh |
| 6 | Mario | sphere, scale 2 | mesh and wire doubling together |

One scene holds all of step 4, because each part builds on the shapes the part before made. It is
named for what it holds rather than numbered, because Scene2 is taken on the space prototype branch.

## The layers

| layer | folder | holds | knows the ECS? |
|---|---|---|---|
| math | `include/physics/` | `Ray`, `RayHit`, `SolveQuadratic`, one `Intersect` per collider, `ToLocalRay`, `MakePickRay` | no |
| data | `include/physics/Components.h` | the four colliders, `Picked`, `PickRay`, `Velocity` | plain data |
| systems | registered by the scene | `DrawColliders`, `PickSystem`, `DrawPickRay` | yes |

Our types sit in a `PHYSICS` namespace, because the math library already has `MATH::Ray` and
`MATH::Sphere`.

**Components and systems are split by module.** The test for each type: could another game use it
unchanged? `Transform`, `Renderable` and `ModelMatrix` moved to `core/Components.h`, and
`DrawRenderablesSystem` to `core/systems`. The colliders and `Velocity` live in
`physics/Components.h`. `game/` keeps `Hit` and `Damageable`. Each module includes only the ones
below it, and nothing in `physics/` includes `game/`. Each module names its file `Components.h`, so
the name says it holds ECS components.

## Colliders

- Each shape is its own component (house rule 6). It sits in the entity's local space: centred on
  `Transform.position`, axis on local Y, turned by the orientation, sized by the scale.
- **Scale sizes the collider.** A collider's numbers describe the shape on the model at scale 1, and
  the engine multiplies them by `Transform.scale`. Scale is uniform, so every shape keeps its kind,
  and mesh and collider always match. Picking depends on it: the ray moves into local space through
  the full transform, scale included, without renormalizing, so `t` compares across shapes. A model
  in the wrong units is fixed at import, not with `Transform.scale`.
- **Every size is measured from the centre out.** A collider stores only its size; the `Transform`
  gives the centre and the axis. The hit tests and the wire drawing read half sizes directly, and one
  rule for all four shapes stops a box's numbers being copied into a can at twice the size. A
  capsule's half height counts only the straight part, as every ECS engine checked does.
- Defaults are a unit shape, 1 metre across, so an `AddComponent` with no value fits a unit mesh and
  never gets size 0.

| component | fields | default |
|---|---|---|
| `SphereCollider` | `radius` | 0.5 |
| `BoxCollider` | `halfExtents` | 0.5 on each axis |
| `CylinderCollider` | `radius`, `halfHeight` | 0.5, 0.5 |
| `CapsuleCollider` | `radius`, `halfHeight` of the straight part | 0.5, 0.5 |

- Mario measures about 2.6 × 2.6 × 1.2, so no shape fits him snugly, which is what real colliders
  look like. His colliders were set by eye: sphere radius 1.3, capsule 0.6 and 0.7, can 0.7 and 1.3.
  His origin is near his centre, so a collider centred on the `Transform` sits on him with no import
  work.
- The mesh is only what you see, and physics reads only the collider. So the scene reuses Mario and
  the cube instead of new sphere and cylinder meshes.

## Collider wires

- Collision is math on the collider's numbers; only the drawing is debug. Wires are built from the
  collider's own numbers, not from meshes, so they show exactly what the physics tests against.
- The wire helpers join `AddLine`, `AddAxes` and `AddGrid` in `core/debug/DebugLines`:
  `AddWireSphere`, `AddWireBox`, `AddWireCylinder` and `AddWireCapsule`. Each takes a world matrix,
  the shape's sizes and a colour, never a component, because `core` knows no game type.
- Each helper builds its points around the origin and multiplies each one by the world matrix.
- **`ModelMatrix` is the one place translate × rotate × scale is built.** `DrawRenderables` and
  `DrawColliders` both call it, so a mesh and its wire always land in the same place.
- Every circle is 32 segments, one shared constant. The side lines sit on the axes, so a turn about Y
  shows in the wire.

| shape | lines | at 32 segments |
|---|---|---|
| ball | three circles, one around each axis | 96 |
| box | its 12 edges | 12 |
| can | a circle at each end, plus 4 side lines at +X, -X, +Z, -Z | 68 |
| pill | the can's lines at ± half height, plus a half circle over each end in the XY and ZY planes | 132 |

- `DrawColliders` is an ordinary Extract system with four queries, one per shape. It owns the colours
  as constants. `WireColour` reads each archetype's mask once: yellow with `Picked`, green without.
  No per-row branch (house rule 6), and no second query per shape, which would double the loops to
  eight.
- No colour field on the collider: a collider is physics data, and colour belongs to the debug view.
- `DrawColliders` reserves nothing. The debug list has several writers (the grid, the axes, the
  wires, the pick ray), so only `Application`, which sees the total, reserves last frame's count.
- The box finds its 12 edges with a bit trick: corners whose indices differ in one bit share an edge.

## Input, and the C and M keys

- Input is the first data crossing into the ECS from outside. `InputState` in `core/InputState.h` is
  plain data: keys held, keys pressed this frame, the mouse position, clicks, and the window size.
  `Application` owns it and fills it from SDL once per frame. It is the one place input enters the
  engine.
- `Application` clears the pressed lists before polling and records each press in `HandleEvent`
  before the F-key and G checks return, skipping repeats. After the event loop it copies `held` from
  `SDL_GetKeyboardState`, which keeps keys from sticking when the window loses focus.
- `SystemContext` carries a `const InputState*`, so systems read it and never write it. The ECS
  headers only forward-declare `InputState`.
- Presses are read in Update. Fixed runs 0, 1 or 2 times per frame, so a press read in Fixed is
  missed or counted twice. Held keys are safe in any phase.
- C and M are app commands, not game commands: they change how the world is shown, not what happens
  in it. `Scene::Update` takes the `InputState`, and the scene flips `SetEnabled` on `DrawColliders`
  and `DrawRenderables`. It runs once per frame before the phases, so a toggle applies to the same
  frame's Extract.
- The registry skips a disabled system, so neither system holds on/off code, and a disabled one builds
  nothing. Systems never switch other systems.
- The flag is a private `bool enabled` on `System`, and `SystemRegistry` is its friend. `SetEnabled`
  and `IsEnabled` are the only way in, and `Print` marks a disabled system "(OFF)". `Add` returns the
  `System*` it stored. The pointer stays valid when the vector grows, because the `unique_ptr` keeps
  the object in place. A scene switch builds a fresh registry, so the toggles reset.

## The normal matrix without an inverse

`FrameLoop` inverted every draw item's matrix for the normal matrix. Below a determinant of `1e-7`
the math library throws in Debug, and shrinking debris in Scene3 hit it within seconds.

- The inverse was removed instead of guarded. A normal is an arrow pointing straight out of the
  surface. Moving, turning and resizing equally on all axes keep it pointing out; only stretching one
  axis tilts it wrong, and the inverse-transpose is the fix for stretching.
- Scale is uniform and `phong.frag` normalizes the normal, so `FrameLoop` uses the model matrix's 3x3
  part as the normal matrix. Its determinant is `scale³`, which fell below `1e-7` under a scale of
  about 0.0046. With no inverse, nothing can throw, and each draw item saves a 3x3 inverse and a
  transpose.
- Scene0 and Scene1 lit the same as before.
- If non-uniform scale ever arrives, the fix comes back as the cofactor matrix of the 3x3 part, not
  the library's inverse. The inverse-transpose is the cofactor matrix divided by the determinant; the
  division only changes the arrow's length, and its sign when the determinant is negative. So the
  cofactor matrix, times the sign of the determinant, gives the right normals with no division, at
  any scale, and never throws.

## Picking

Every shape and the pick ray use the course's method, with the cases it misses fixed inside it. On
2026-10-29 a scratch test compared the course can, pill and pick ray with the alternatives first
designed: same answers, and the course one was faster or fast enough each time. The alternatives
were dropped.

**`Ray` and `RayHit` are plain structs, never components.** A click casts one ray once, and nothing
needs it after `PickSystem` returns.

- `Ray` holds an origin and a direction, with `PointAt(t)`. The direction is normalized once, in
  `MakePickRay`, so `t` is metres in the world.
- `ToLocalRay` and `Intersect` never normalize. The local direction's length is 1 / scale, and that
  keeps `t` in metres in every shape's space. With uniform scale, moving the ray into a shape's space
  is subtract the position, turn back by the conjugate, divide by the scale. No general inverse.
- `RayHit` holds the hit point in the shape's local space, and `t`. No normal: nothing reads one.
- A miss is an empty `std::optional<RayHit>`. The handout's `bool isIntersected` leaves a `t` of 0
  that reads as a hit when the bool is not checked.

**`SolveQuadratic`** returns the number of roots and both roots; one root fills both. A discriminant
within `kVerySmall` of 0 counts as one root, checked before "below 0", because float rounding can push
a true 0 one step negative. With `a` near 0 it returns no roots. For the can and pill that means a ray
along the axis, which "misses the side wall", not "misses the can".

**A ray that starts inside a shape ignores that shape.** For picking, a camera inside a collider
clicks through to what it can see. Every shape uses one rule: an entry `t` below 0 is a miss.

**One `Intersect` overload per collider**, in local space. The collider is plain data, so the math
layer still knows no ECS.

- **Sphere:** the handout's form, `a = d·d`, `b = 2(S·d)`, `c = S·S − r²`.
- **Can: the notes' tube, then the caps.** Two branches the notes miss come first. If the start is
  within the radius, inside the can is a miss, and above or below it only the nearer cap can be hit.
  A ray along the axis from outside the radius is a miss. Then the smaller root must be ≥ 0; a hit
  between the two ends is a side hit, and outside that the cap on that side is tested.
- **Pill: the handout's tube, then the sphere caps.** The same steps as the can, with sphere caps in
  place of flat caps. A cap test is the sphere's `Intersect` with the ray moved so the cap sits at
  the origin.
- **Box: the handout's slabs,** with `tmin` starting at −∞, not 0, and an explicit branch for a
  direction part near 0: outside that slab is a miss, inside it skips the axis. A comment says why
  `tmin` differs from the handout.

**`MakePickRay`** is Vakarian's course code with three Vulkan changes. The inverse of
`MMath::NDCtoViewport` takes the pixel to NDC. Depth is set to 0, Vulkan's near plane (OpenGL's was
−1). Then the inverse of plain `PerspectiveVulkan`, a divide by w, and the inverse view.

- The origin is the camera's position, read from the inverse view's last column, because `Camera`
  has no position (see fly-camera.md).
- `NDCtoViewport` already flips y, so the projection does not. Negating element 5 here, as the
  renderer does for the GPU, would turn every click upside down.
- `windowSize` sits in `InputState` beside the mouse position, so both are in window units. The
  swapchain size is in pixels and differs on a high-DPI screen.
- `SystemContext` carries a `const Camera*`, set from the scene, null in tests.

**`PickSystem` runs in Update.** On a left click it builds the ray and runs four queries, one per
collider type. An entity with a scale near 0 is skipped, because nothing can hit it. The smallest `t`
wins. A default `Entity{}` has version 0, which no real entity has, so version 0 means no hit.

- A hit queues removing `Picked` from every other holder and adding it to the new entity. A second
  click on the picked shape only adds, because `AddComponentById` copies new bytes over old. A miss
  removes it from every holder.
- `Picked` holds the clicked point in the shape's own space, so it stays on the body as the shape
  moves. `DrawColliders` uses `Picked` only in a mask, so its size costs that loop nothing.
- `PickRay` sits on one entity the scene creates, and `PickSystem` writes it on every click. A miss
  sets `t` to the camera's far plane. `DrawPickRaySystem` in Extract draws the ray, and a 3-line
  cross on a hit. C toggles it with `DrawColliders`.
- `PickSystem` writes both `Picked` and `PickRay`, accepted under house rule 4: both record one click.
- Each click prints one line: `Picked entity 4 (BoxCollider): t = 7.31, world point (1.20, 0.50,
  -0.38)`, or `Nothing picked`. `HasComponent` on the picked entity finds the shape name, so the loops
  save no collider id. An entity with two colliders would print the first match.
- The playback rule in living/ECS.md came from here: `PickSystem` queues `Picked` in Update, so
  `DrawColliders` draws the yellow wire in the same frame's Extract.

**`PhysicsTests`** is its own program, like `EcsTests`, on all four configurations, with the math
library and no ECS. `MATHEX`'s `Ray.h`, `RMath.h` and `Quadratic.h`, which `PHYSICS` replaces, were
deleted.

## Rejected

- One `components/` folder: it splits a feature across two folders.
- A collider that ignores the transform's scale, as Stride does.
- The handout's `Shape` base class with a virtual call: the colliders are separate components.
- Ericson's shorter sphere form: it assumes a normalized direction and would give a wrong `t`.
- Letting a divide by 0 in the box make ±∞: it gives NaN when the start sits on a slab plane.
- A hit at the start for a ray inside a shape, as Jolt reports.
- A closed-form pick ray (`tan(fov/2)` and the view's 3x3 transposed): the same rays at 10 ns against
  about 70 ns for inverse matrices, but one call per click makes that difference worthless.
- The camera as an entity, which would need a copy each frame.
- A separate `GrabPoint` component, always added and removed with `Picked`, and storing `t` or the
  world point in `Picked`.
- The on/off flag in the registry's vectors: the same result, more code. Lookup by name: a typo fails
  at runtime.
- A cos/sin table for the wires, and transforming the axes instead of the points: about 400 points
  per frame cost nothing measurable, and the code got harder to read.

## What the build found

- The first `AddArc` transformed only the centre, so a turned or scaled circle stayed upright and
  unit sized. A translate-only test could not show it. A temporary demo of rotated and scaled shapes,
  each inside a grey `AddWireBox` drawn with the same matrix, did. `AddArc` is private to
  `DebugLines.cpp` and takes a sweep in radians. Line counts match the plan: 96, 12, 68, 132.
- **Pi is a constant now.** `FLOAT_PI`, `DEGREES_TO_RADIANS` and `RADIANS_TO_DEGREES` became
  `MATH::kPi`, `MATH::kDegreesToRadians` and `MATH::kRadiansToDegrees`, `inline constexpr` in
  `ConstantsConversions.h`, and all 27 uses switched. The old `FLOAT_PI` was `3.141592f`, three float
  steps short. That error was far too small to see, so the reasons were the correct rounding and a
  typed constant in place of a macro. `VERY_SMALL` and `VERY_CLOSE_TO_ONE` are still macros.
- The first `DrawColliders` reserved entities instead of vertices, and copied `DrawRenderables`'
  reads mask. The startup print showed the mask.
- `PhysicsScene`'s lights became a `static constexpr` array. The first version returned a
  `std::span` over a temporary array, the span pointed at dead memory, and Mario lit half green.
- Including `InputState.h` in the ECS headers pulled SDL into `EcsTests`, `EcsBench` and `ArenaDemo`,
  which cannot see SDL, and the full build broke. The headers now forward-declare it.
- `MovementSystem` moved to `physics/systems`, but its box wrap belongs to Scene0. Step 4c split it.
- Scene1 keeps `BoxCollider`s on its two cubes and runs `DrawColliders`.
- With M off, the mesh pass draws nothing and the frame stays clean.

## Measured

- **The can,** 2026-10-29, on 2,000,000 random rays against a tube-and-slab version and the Graphics
  Gems IV port: identical answers, 15 ns per ray against 25 ns for the Gems port.
- **The pill,** 2026-10-29, on 1,000,000 random rays: identical to a can-plus-two-spheres version,
  and right against a sphere-traced ground truth on 200,000 of them. The one disagreement was the
  ground truth stalling on a graze. 23 ns per ray against 50.
- **The pick ray,** 2026-10-30, on 200,000 clicks on points projected with the GPU's matrices and
  random cameras: worst error 1.4e-5 radians.
- **4a, checked 2026-10-28.** The full build has no warnings, and `run-tests.bat` passed in all four
  configurations. Scene0, Scene1 and `PhysicsScene` ran with no `[WARNING]` or `[ERROR]` and 0 leaked
  blocks, and so did a scripted run that switched scenes and toggled C and M. Vakarian checked the
  wires by eye: the fits, the spinning cube's wire and the scale-2 Mario.
- **4b, checked 2026-10-30.** Vakarian clicked through the done test by hand: nearest shape, every
  shape type, the yellow wire, a click on empty space, the drawn ray and cross, and a camera inside a
  collider clicking through it.
