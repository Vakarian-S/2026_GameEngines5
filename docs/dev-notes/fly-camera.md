# Fly camera: why it stores angles, not a dual quaternion

Decided 2026-10-21, while building step 1b. Designed 2026-10-20 and 2026-10-21,
built 2026-10-22. The rest of step 1b (the `Camera`, the input, the wiring) is at the end, under
"The rest of step 1b".

## The decision

`FlyCameraController` stores a `Vec3` position, a yaw angle and a pitch angle. It rebuilds its
quaternion from the two angles each time it needs one, and never stores it. This is the usual
design for an FPS or fly camera.

## What the first design did

The first design stored the whole pose as one `MATHEX::DualQuat`, to earn the course notes' extra
mark for using dual quaternions on the camera. Each frame changed the stored pose in place:

- move: `pose = pose * translate(localMove)`, and `pose = translate(worldMove) * pose`
- yaw: split the pose into translation `T` and rotation `R`, then `pose = T * yaw * R`
- pitch: `pose = pose * pitch`, then `DQMath::normalize(pose)`

## What the test found

A test compared the controller's view matrix with `MMath::lookAt` after each kind of move. Ten
checks passed, to within 0.0001 on all 16 entries: an off-axis start, local and world moves, yaw,
pitch, both turn orders (no roll), and the pitch clamp. So the maths and the library's sign
conventions were right.

The eleventh check failed. 3,600 one-degree turns in place (10 full turns) should leave the camera
where it started. They moved it outward:

| distance from origin | turns | drift, yaw only | drift, pitch only |
|---|---|---|---|
| 4 m | 3,600 | 0.44 mm | 1.7 mm |
| 40 m | 3,600 | 13.3 mm | 13.7 mm |

Rebuilding the pose after every turn from a clean position and a normalized rotation,
`translate(getTranslation(pose)) * rotate(normalize(getRotation(pose)))`, still drifted 12.8 mm
at 40 m.

These were Debug builds, measured with a temporary check in `Scene0`'s constructor.

## Why it drifts

- A float holds about 7 significant digits. At 40 m, the smallest step it can hold is about
  0.000004 m, so every multiply rounds to the nearest step.
- A dual quaternion does not keep the position in its own three numbers. Its dual part holds the
  position multiplied by the rotation. So every rotation, even a turn in place, recomputes the
  numbers that hold the position, and rounds them.
- `getTranslation` pulls the position back out with another multiply, which rounds again.
- The drift grows with the distance from the origin, which is how float rounding scales. That
  points to rounding, not to a wrong formula. The rebuild test says the same thing: it goes through
  the same mixed form, so it rounds the same way.

This is not a bug in the library, and it is not unique to dual quaternions. A 4x4 matrix that has
rotations multiplied into it every frame drifts too: its axes slowly stop being perpendicular.

## Options weighed

1. **Keep the dual quaternion pose and accept the drift.** About 1 cm per 10 full turns at 40 m,
   too small to see. Rejected: the drift is systematic and grows with distance and with play time.
2. **A `Vec3` position plus a stored quaternion.** Turning no longer touches the position, so the
   position drift is gone. But multiplying each turn into the stored quaternion still rounds: its
   length drifts from 1, which a normalize fixes, and a tiny roll can build up, which a normalize
   cannot fix, because the quaternion still has length 1. The roll is reasoning, not measured: this
   option was never built. Rejected for the angle design below.
3. **A position and a rotation-only dual quaternion.** Rejected: a dual quaternion with no
   translation is a quaternion with four extra zeros.
4. **A position plus yaw and pitch angles, with the quaternion rebuilt from them.** Chosen. Turning
   only adds to two floats, so nothing builds up. The camera cannot roll, because the state holds
   no roll. The pitch angle is the state, so the clamp needs no extra float.

Engines store a transform the same way, as a separate position and rotation: Unreal's `FTransform`
and Unity's `Transform`.

## What the chosen design measured

The same checks ran against the angle design, in a scratch copy, plus a 1 m sideways move and a
start check. 12 of 13 passed:

- 3,600 one-degree turns in place ended exactly where they started: a difference of 0 in all 16
  entries. The position never changes while turning.
- That needed one addition: the yaw is wrapped into ±180 with `std::remainder`. Without the wrap,
  the yaw grew to 3,600 degrees, the sine and cosine of that large float lost precision, and the
  same check ended 0.0005 off.
- The 13th check turned 3,600 times by `0.1f`. As a float, `0.1f` is a little over 0.1, so the
  inputs add up to 360.0036 degrees, and the camera ended 0.0036 degrees past its start, with its
  position unchanged. The camera turned by exactly what it was told, so the check was wrong, not the
  camera.

Built 2026-10-22 in commit `2216b4f`.

## Where dual quaternions fit instead

They work well when each one is built fresh from clean values and used once, not changed in place
many times a second:

- **Skinning.** Blending bone transforms as dual quaternions keeps a twisting joint's volume,
  where blending matrices collapses it into a "candy wrapper". The bones are rebuilt from the
  animation each frame. This is their best-known use in game engines.
- **Interpolating between two fixed poses.** `DQMath::slerp` follows a screw path, rotating and
  sliding together, for keyframes or a camera glide between two saved views.
- **Moving lines and planes.** The library is projective geometric algebra, so one motor moves a
  point, a line or a plane with the same `rigidTransformation` call. Step 4b can move the pick ray
  into a shape's local space this way. `DQMath::lookAt` uses the same idea.
- **Hierarchies rebuilt each frame.** A world pose is the parent pose times the local pose, from
  clean local values.

## What it cost

The course notes' extra mark for using dual quaternions on the camera. Step 4b is a natural place
to earn a dual quaternion mark back, if the course accepts it there.

## The rest of step 1b

### `Camera` stays as it is

`Camera` holds the view matrix, the field of view and the near and far planes, and gains no
position. The renderer keeps reading the camera position from the inverse of the view, as it has
since milestone 8, and the pick ray in step 4b does the same.

- A position field would hold the same fact as the view, twice. A scene that set the view and
  forgot the position would get wrong highlights with no warning. The inverse can never disagree
  with the view, and one 4x4 inverse per frame is too small to matter (a guess, not measured).
- It is the same rule as the model matrix: the editable values stay on one side, and only the
  finished matrix crosses to the renderer.
- The old `2026_GPUProgramming` project sent no position either: it lit in view space, where the
  camera sits at the origin.

### Input comes in two kinds

**Held keys** (WASD, Q, E, Shift) are read in `Update(dt)` from `SDL_GetKeyboardState`. The camera
moves by direction × speed × `dt`, so the speed is the same at any frame rate.

- Scancodes, so the keys sit in the same place on any keyboard layout.
- SDL updates its key array while events are pulled, so `Update` reads it after the event loop.
- No booleans of our own for held keys. A key-up that lands on another window would leave one
  stuck, and SDL clears its own array when the window loses focus.
- A diagonal (W and D together) has length 1.41, so the local direction is normalized, but only
  when it is not zero. In Debug, `VMath::normalize` throws a `std::string` on a zero vector, and
  `main` caught only `std::exception`, so the first build aborted on its first frame with exit code
  3 and no message. Step 3 later replaced the length check with `SafeNormalize`.
- Shift multiplies the speed by 4.

**Mouse look** comes from events.

- A right button press turns on `SDL_SetWindowRelativeMouseMode` and sets the one flag the
  controller keeps. A release turns relative mode off. The event's `windowID` gives the window,
  through `SDL_GetWindowFromID`.
- Mouse motion while the button is held changes yaw by `xrel` and pitch by `yrel`, times the
  sensitivity of 0.1 degrees per pixel. There is no `dt`, because `xrel` is already a distance.
- Both deltas are negated. Positive yaw turns left and positive pitch looks up, while a mouse moved
  right or down gives a positive `xrel` or `yrel`.
- `SDL_EVENT_WINDOW_FOCUS_LOST` turns looking and relative mode off, because a button released over
  another window would leave the cursor hidden and captured.

### Wiring and keys

- `Scene::HandleEvents` passes each event to the controller. `Scene::Update(dt)` calls the
  controller's `Update(dt)`, then writes `camera.view = controller.View()`.
- WASD, Q, E, Shift and the right mouse button belong to the camera in every scene. F1 to F4 switch
  scenes. Scene1 keeps 1 to 5, and the physics scene keeps C and M. Scene0's "W pressed" print went.
- Scene1's lighting keys now sit inside a key-down check instead of an early return, so mouse events
  still reach the camera. Without any check, a key-up matched too and the lighting changed twice per
  press.
- Rejected: an orbit camera that circles a target. It is simpler for inspecting one model, but it
  cannot walk around a scene.

### The check for step 1b

- A script drove both scenes with W, D and Shift held together, Q, and a right-mouse drag sideways
  and then hard upwards past the clamp. Both exited with code 0, 0 leaked blocks and no validation
  output.
- 20 scene switches gave the same: 21 closes, and a peak of 592,599 live bytes, 32 more than before,
  because each scene now holds a controller.
- Vakarian checked the picture: W goes where the camera looks, the mouse turns the right way, the
  pitch stops short of straight up and down, and Mario's lighting holds from behind.
