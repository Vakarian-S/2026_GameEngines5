# Dangle physics: a picked shape hangs from the mouse

Built 2026-11-02 (milestone 1, commit `5457cd6`) and 2026-11-03 (milestone 2), as step 4c of the
physics-scene plan. Course work: Game Physics 3, assignment 3, "Constrained Motion".

## Sources

- The handout: `GAME_309_Assignment3_Constrained_Motion_Noor_f25.pdf`, formulas on pages 4 and 5.
- Allen Chou's slides: `constraint-based-physics.pdf`, "Mouse Constraints" and "Merging Constraints".
- Box2D v2.4.1, `b2_mouse_joint.cpp`, Erin Catto's version of the same constraint.
- The inertia formulas: standard tables, and the GameDev.net capsule article for the pill.
- A throwaway prototype on branch `prototype/dangle`: findings/2026-10-dangle-prototype.md.

## What it does

A left press on a shape grabs the clicked point. Holding the button hangs the shape from the mouse.
The shape swings, turns and follows the mouse. Releasing the left button, or pressing the right one,
drops it. A shape falls for good after its first click, because nothing has a floor. F3 restarts the
scene.

## Two milestones, each its own commit

Course work keeps the course's version as its own result. Milestone 1 builds the handout's version as
written. Milestone 2 improves it on the same setup, so the diff between the commits shows only the
improvement.

**The handout is the slides with two substitutions:**

- `r = mouseTarget − centre`. The gap `C` is then always 0, so nothing pulls the clicked point
  toward the mouse.
- Mass 1 and identity inertia. `M⁻¹` is then the identity, so every shape turns alike.

**The slide's `J` has a typo.** Its third row prints `r_y −r_z 0`; it should be `r_y −r_x 0`. The code
uses the cross product `v + ω × r` instead.

## The math

All of it is free functions in `PHYSICS` (`RigidBody.h`), with no ECS, so `PhysicsTests` can run them.
The systems only loop, call and write the result.

**Milestone 1, the handout:**

- `r = mouseTarget − position`
- `JV = v + ω × r`, `λ = Meff⁻¹ (−JV)`, `v += λ`, `ω += r × λ`

**Milestone 2, the slides:**

- `r = q × (scale × localPoint)`: the arm now points at the clicked point.
- `I⁻¹_world = R I⁻¹ Rᵀ`, with `R = toMatrix3(q)`.
- `Meff = J M⁻¹ Jᵀ = (1/m)·I₃ + [r]× I⁻¹_world [r]×ᵀ`. `Matrix3` has no `+`, so the code adds `1/m`
  to the three diagonal entries by hand.
- `C = position + r − mouseTarget`, `b = (β / Δt) · C`, with β = 0.15. The bias closes 15% of the
  gap each step, which is what makes the shape follow.
- `λ = Meff⁻¹ (−JV − b)`, `Δv = λ / m`, `Δω = I⁻¹_world (r × λ)`. `MouseConstraint` returns `v + Δv`
  and `ω + Δω`, like the other functions.
- The lag is the known cost: `gap ≈ speed × Δt × (1 − β) / β`, about 0.47 m at 5 m/s.

**Inertia, about the shape's own axes, sizes after scale (Y runs along the can and the pill):**

| shape | across | along Y |
|---|---|---|
| box | `m/3 × (the other two half sizes squared)`, per axis | same rule |
| sphere | `2/5 m r²` | `2/5 m r²` |
| can | `m (3r² + 4h²) / 12` | `½ m r²` |
| pill | `m_t (r²/4 + h²/3) + m_c (2/5 r² + h² + ¾ h r)` | `½ m_t r² + 2/5 m_c r²` |

The pill splits its mass between the tube (`m_t`) and the two caps (`m_c`) by volume.

**Damping, from the hand test:**

- Angular drag: `ω × (1 − 0.25 Δt)`, on every falling body. Without it a released shape turns forever.
- Grip: `ω / (1 + 7 Δt)`, on a held shape, before the solve. A point constraint is a needle pinch, so
  a held shape did laps after a quick move. Box2D does the same with `wB *= 0.98f` (lines 120 and 121).
- Spin limit: 12 rad/s on a held shape, after the solve. It keeps the axis and shortens the length.

## How it fits the ECS

**Components:** `Velocity`, `AngularVelocity`, `MassProperties` (mass and inertia tensor), the
`Falling` tag, and `Picked { localPoint, grabT, mouseTarget }`.

- They are split by who reads what. Scene0's cubes use `Velocity` and never turn. Velocities change
  every step, while mass and inertia stay fixed.
- `Falling` marks a shape that has been clicked once. `Picked` means held now.
- `PickSystem` writes `mouseTarget = mouseRay.PointAt(grabT)` every frame. The target slides on a
  sphere around the camera, and flying the camera moves it too.

**Systems, in Fixed, in this order.** This is the handout's order: velocity before the constraint,
position after it.

1. `ApplyForcesSystem`: gravity, drag 0.25, and angular drag.
2. `MouseConstraintSystem`: grip, the solve, then the spin limit.
3. `UpdatePositionSystem`: `position += v × Δt`.
4. `UpdateOrientationSystem`: `q = rotation × q`, normalized.

`DrawGrabLineSystem` runs in Extract. It draws a line from the clicked point to the mouse target, so
the gap is visible.

**Side changes:** `MovementSystem` split into `UpdatePositionSystem` (physics) and
`WrapIntoBoxSystem` (Scene0 only). `SpinSystem` and `Spin` were deleted, and `AngularVelocity` turns
the spinning cube and `Scene1`'s Mario instead.

**Input:** `InputState::mouseHeld` holds the held buttons, filled once per frame from
`SDL_GetMouseState`. Held state is safe to read in Fixed. A right press drops the shape, because it
also starts mouse look, which freezes the mouse position.

## Rejected

- The handout's single `PhysicsComponent` with its own copy of position and orientation: `Transform`
  is the truth.
- A stored acceleration: `a = F / m` is a local value. Add a force sum only if several systems ever add
  forces.
- One `RigidBodySystem`: Scene0 needs the position step alone, and `SetEnabled` switches off one step.
- A grab plane through the clicked point: a ray-plane step for no visible gain.
- A limit on the bias speed: in the hand test, quick mouse moves became very slow and throws died.
- A second constraint holding orientation: it stops the swing.
- More angular drag on everything: released shapes would stop turning in mid-air.

## What failed, and what changed

**Milestone 1, in Vakarian's words (2026-11-02):**

- "Mouse constraints feel weak, it feels like I'm grabbing it with a very flimsy rope instead of
  holding it with a mouse, so objects offer resistance but they slowly fall."
- "Shapes feel unnatural when grabbed, spheres don't feel right and neither do capsules, because all
  logic is being treated the same way."

The causes: nothing in the handout's equation measures the gap to the mouse, and its matrix assumes
mass 1 and identity inertia.

**Milestone 2, in Vakarian's words (2026-11-03):** "Now the constraints follow the mouse, also the
feeling of grabbing now differs per shape, because of the inertia physics. Every shape has a different
distribution of mass which makes the picking more natural, also the spinning is damped when grabbing
something and it slows down when thrown."

## What the build found

- A system's read mask is only a label. Four new systems first copied the query `Transform, Velocity`,
  so `Column<Picked>()` returned null and crashed. Milestone 2 hit it again with `AngularVelocity`
  missing from `ApplyForcesSystem`'s query. STATE.md lists it as a leftover.
- `PickSystem` lost its press check while gaining the drop branch, so it picked every frame (455 prints
  in 8 s). A left press now also drops, so one frame can never leave two shapes held.
- `ApplyForces` first scaled drag by mass. The handout's drag has no mass in it, and a test caught it.
- `mouseHeld` first read the wrong bit. `SDL_BUTTON_MASK(button)` fixed it.
- Two compile errors: a Unicode minus sign copied from a chat message, and `#include <AsyncInfo.h>`,
  which pulls in the Windows `near` and `far` macros and breaks `MMath.h`.
- `VMath::normalize` throws on a zero vector in Debug, so `UpdateOrientation` skips the turn when `ω` is
  near zero, and `LimitSpin` only normalizes above the limit.
- `kGripForce` became `kGripDamping`: grip scales the spin directly and is not a force.
- `PhysicsScene`'s world arena went from 16 KB to 256 KB without a measurement. STATE.md lists it.

## Measured

- **The `MovementSystem` split** cost nothing visible in Scene0, three 20 s Release runs each:
  `Movement` 0.008 to 0.009 ms before, `UpdatePosition` plus `WrapIntoBox` 0.007 to 0.008 ms after.
  The profiler prints three decimals, so smaller changes do not show.
- **A 2 kg box held by a corner,** 10 s at 60 Hz, no grip or limit: 0.00004 m off a still mouse; at
  worst 0.474 m off a mouse moving 5 m in 1 s, close to the lag formula's 0.47 m.
- **A pill held at its tip,** the mouse jerked 3 m at once, with grip and limit: the spin never passed
  12 rad/s, and the tip settled 2.4e-7 m off the mouse.
- **Angular drag** leaves 8.17% of the spin after 10 s.
- **The pill's inertia** matches random sampling of a solid capsule, 2 million points.
- **A rolled box** (30 degrees) matches a box whose tensor was turned by hand, which shows that
  `toMatrix3` and `QMath::rotate` agree.
- The prototype's runs, the handout's numbers and the pendulum swing times are in
  findings/2026-10-dangle-prototype.md.
- Checked at the end: `PhysicsTests` 202 of 202, `EcsTests` 177, `ArenaTests` 73 (72 in Release),
  `MemoryMonitorTests` 81 (42 in Release), x64 and Win32. `PhysicsScene`, Scene0 and `Scene1` run with
  no warning, no error and 0 leaked blocks.
