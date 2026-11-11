# Live fps and the pacing scene

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-11-05 and built 2026-11-11, as steps 6 and 7 of the frame pacing plan, which ended on
2026-11-11 (step 6 in commit `392c61f`, step 7 in `1eb05c6`). The `FrameStats` → `WorldFrameStats`
rename that step 6 needed is commit `04cd0e8`. It came from ROADMAP.md, Rendering, "Frame pacing and
the present mode", and from the pacing prototype (`findings/2026-11-pacing-prototype.md`). "Step N" below means a step of that plan; README.md maps each one
to its note. Each section below is the step's whole plan section.

The plan's goal: the display paces the loop, not a hand-written sleep. The player picks the present
mode and a frame cap, and the engine shows what each choice costs. frame-cap-and-timer.md and
present-mode-and-fullscreen.md hold the other steps.

## Step 6: Live fps

The engine has no text. The window title costs a few lines. It can't be seen in fullscreen, so the
console prints the same line once a second.

**Decisions**

1. **A `FrameTimeStats` class in `include/core/`,** owned by `Application`. Its one input is
   `AddFrame(uint64_t nanoseconds)`, fed with the frame time `Timer` measured in step 1. It never reads
   a clock, so a unit test can feed it fixed frame times. Rejected: the prototype's loose fields and
   `UpdateStats` on `Application`, which give `Application` a second job; building it on the profiler,
   whose job is span timings in a dump, compiled for profiling runs only.
2. **Four numbers per window:** the frame count, the total time, the shortest frame and the longest
   frame. The report gives fps (frames divided by total time), the average frame in ms, and the
   shortest and longest. The longest frame catches a stutter that an average hides. Rejected: "1% low"
   or percentiles, which need every frame time stored and sorted; tools like PresentMon give those from outside the engine.
   Averaging 1/frame time, which a few short frames pull up.
3. **One window of 1 second** feeds both the title and the console line, so every frame lands in
   exactly one printed line. Rejected: the prototype's 0.25 s title and 1 s console, where the console
   printed only every fourth window and missed the longest frame of the other three; setting the title
   every frame, an OS call that would cost more than the frame at 3,000 fps.
4. **`AddFrame` returns `true` when the summed frame times reach 1 second.** `Application` then reads
   a `FrameTimeReport` struct (`fps`, `averageMs`, `shortestMs`, `longestMs`), and the counts reset. A new
   `Window::SetTitle(const std::string&)` sets the title. Rejected: `Application` calling
   `SDL_SetWindowTitle` through `GetSDLWindow()`, which spreads SDL window calls, as in step 4.
5. **One line for both the title and the console:**
   `FIFO | cap off | 239.8 fps | 4.17 ms (min 4.10, max 4.25)`. The mode and the cap come first, so a
   screenshot or a log line records the settings beside the numbers. Rejected:
   numbers only; the prototype's load and Hz fields, since the load belongs to step 7's scene and the
   Hz rarely changes.
6. **No switch for the console line.** It prints once a second in every configuration, so every
   `run-game.ps1` run records its frame rate in `out.txt`. Rejected: a toggle key, which nobody needs
   yet; a future ImGui menu is the place for it.
7. **The check:**
   1. Unit tests for `FrameTimeStats`:
      - 240 frames of 4,166,667 ns: `AddFrame` returns `true` on the last one, with fps 240.0,
        average 4.17 ms, and min and max 4.17.
      - One 50 ms frame among short ones: max reads 50.00, and the average stays near the short
        frames.
      - After a report, the next second starts from zero.
   2. A Release run with `run-game.ps1`:
      - FIFO, no cap: about 240 fps in `out.txt`.
      - `hold Ctrl,L 100` to cap 30, then cap 60: 30.0 and 60.0 fps, with min and max at 33.33 and
        16.67.
      - `hold Alt,Enter 100`: the console lines keep coming in fullscreen.
      - A `shot` of the window: the title shows the line.
   3. Step 5's temporary min/max print gets deleted. This line replaces it.
   4. A Debug run (no `[WARNING]`, no `[ERROR]`, 0 leaked blocks) and `run-tests.bat` in all four
      configurations.

**Changes during the build**

- The class is `FrameTimeStats` and the struct `FrameTimeReport`. The plan's `FrameStats` already
  existed in the ECS as a struct in the global namespace, so the ECS one became `WorldFrameStats`
  (04cd0e8) and this one got a name that says it holds frame times.
- `Timer` gained `GetDeltaNanoseconds()`, so `AddFrame` gets the exact integer, not a `float`.
- A first build appended the `double` fields to a `std::string` with `+=`, which takes one `char`
  (C4244). The title never changed. An `std::ostringstream` builds the line now.

**What the build checked**

Unit tests, in `EcsTests` beside `FixedStep` ("frame time stats", 12 checks):

- 239 frames of 4,166,667 ns give no report; the 240th does, with 240 fps, and 4.167 ms for the
  average, shortest and longest.
- One 50 ms frame and 228 short ones: longest 50 ms, shortest 4.167 ms, average under 4.5 ms.
- After a report, 100 frames of 10 ms make the next one: 100 fps, shortest and longest 10 ms, so the
  last second's 4.17 ms is gone.
- `scripts\run-tests.bat`: all four configurations pass.

Release, Scene0, the 4060, windowed on the 240 Hz monitor. `run-game.ps1 -Config Release -Steps
"wait 4000","shot title","hold Ctrl,L 100","wait 4000","hold Ctrl,L 100","wait 4000",
"hold Alt,Enter 100","wait 3000","hold Alt,Enter 100","wait 2000"`. Lines from `out.txt`, leaving out
the second each change lands in:

| setting | line |
|---|---|
| FIFO, no cap | `FIFO \| cap off \| 239.8 fps \| 4.17 ms (min 0.11, max 11.42)`; 234.5 to 239.8 fps over the run |
| FIFO, cap 30 | `30.0 fps \| 33.33 ms`, min 32.63 to 33.05, max 33.61 to 33.85 |
| FIFO, cap 60 | `60.0 fps \| 16.67 ms`, min 16.58 to 16.64, max 16.71 to 16.81 |
| FIFO, cap 60, fullscreen | the lines keep coming: `60.0 fps \| 16.67 ms (min 16.65, max 16.69)` |

- The `shot` shows the line in the title bar: `FIFO | cap off | 234.5 fps | 4.26 ms (min 0.12, max 13.26)`.
- Cap 30 and 60 read 30.0 and 60.0 fps, but each second's min and max sit up to 0.7 ms off 33.33 and
  16.67, not the plan's "min and max at 33.33 and 16.67". On the laptop panel in step 5, cap 30 held
  33.32 / 33.36 once the dump stopped. One run here. Step 5 (frame-cap-and-timer.md) later found the
  NVIDIA driver's power saving behind uneven frames; these runs were not repeated with it off.
- The second a fullscreen switch lands in reads a 51 ms frame: the swapchain rebuild.
- Debug, Ctrl+L, Ctrl+P and F2: no `[WARNING]`, no `[ERROR]`, no asserts, 0 leaked blocks. The second
  with the scene switch reads `min 0.00`, the frame right after `timer.Start()`, as expected.
- The temporary prints from steps 1 and 5 are gone from `Application.cpp`.

## Step 7: Pacing scene

A test scene to see tearing and judder by eye. The prototype's scene
used two sweeping bars of cubes. This one uses Marios, so it tests the OBJ path too.

**Decisions**

1. **A new `PacingScene`, written fresh, at row 3 of `kScenes`** ("Pacing Scene"). It has two rows of
   10 Marios from `assets/meshes/Mario.obj`, loaded the way Scene1 and PhysicsScene load it. The top
   row moves in the Update phase, once per frame. The bottom row moves in the Fixed phase, 60 times a
   second. The scene is new, so the check measures its own numbers. Rejected: copying the prototype's
   cube scene, which skips the OBJ path; putting the rows into Scene0, whose 1,000 cubes would change
   the frame times.
2. **The motion wraps around.** Each Mario moves right with `x += speed * deltaTime`. Past the right
   edge, the code subtracts the path's full width, so the leftover distance carries over. The 10
   Marios in a row are spaced evenly along the path, so the row always fills the screen. The path is
   wider than the camera's view, so the jump back happens off screen. Rejected: resetting `x` to the
   start, which drops the leftover, so the rows drift apart and the speed reads wrong; wrapping at the
   edge of the view, where the pop looks like a pacing bug.
3. **The fake load is a `LoadSystem` in the scene's Update phase.** It spins on `SDL_GetTicksNS` for
   the chosen time, so it works the CPU like real work, and the profiler shows it as its own span.
   Rejected: the prototype's `AddLoad` in `Application`, which puts one scene's test feature in every
   scene's loop; sleeping, which lets the CPU rest and can wake late.
4. **Ctrl+H, in the scene's `HandleEvents`, cycles the load:** off → 5 ms every frame → 7.5 ms every
   frame → a 25 ms spike every 30 frames → off. It prints `load: 5 ms every frame`. 5 ms is 1.2
   refreshes at 240 Hz, as in the prototype's "every frame late" run. 7.5 ms is late at 165 Hz too.
   The spike is a single stutter for step 6's max to catch. Rejected: the prototype's "1.25
   refreshes"; the scene can't see the refresh rate, and fixed numbers make runs repeatable.
5. **The bottom row stays.** It steps at 60 Hz at any frame rate, because nothing interpolates
   between fixed steps. That's the case for interpolation, which is out of scope here. Rejected:
   dropping it; it's the only place the engine shows that problem.
6. **The check:**
   1. A Release run of scene 3 with `run-game.ps1`. Measure and record each line, since Marios cost
      more to draw than the prototype's cubes:
      - No load, FIFO: the fps, and a `shot` showing both rows.
      - `hold Ctrl,H 100` once (5 ms): the fps in FIFO. The prototype's cubes read 190.
      - Twice more (the spike): the max near 25 ms in step 6's line, with the average close to the
        no-load run.
   2. By hand, since a script can't see motion:
      - IMMEDIATE: the top row tears, and whether the tear is easy to see on Mario's shape.
      - Cap 100: the top row judders.
      - The bottom row steps at any setting.
   3. A Debug run that switches into the scene and out again. No `[WARNING]`, no `[ERROR]`, 0 leaked
      blocks.

   Rejected: unit tests. The scene's point is what you see, and step 6's line checks the numbers.

**Changes during the build**

- Claude wrote this step's code as blocks, at Vakarian's request: it is a test scene.
- One `SlideRightSystem` class, added twice: in Update with the `FrameStepRow` tag, in Fixed with the
  `FixedStepRow` tag. The tag picks the row and the phase picks how often it moves.
- `LoadSystem` holds its load as fields, and the scene sets them through `SetLoad` on Ctrl+H. It spins
  with `SDL_CPUPauseInstruction()`, like `FrameCap`.
- Both rows start at the same x, so any gap between them is the pacing.
- Path −15 to +15 m, 10 m/s, rows at y = ±2, camera at z = 14. The screenshot shows about 7 Marios per
  row, so the wrap happens off screen. The Marios face away from the camera.

**What the build checked**

Release, `-Scene 3`, the 4060, the 240 Hz monitor, windowed, NVIDIA power mode at the default.
`run-game.ps1 -Config Release -Steps "wait 4000"`, then `"hold Ctrl,H 100","wait 4000"` per load
step. Lines from `out.txt`, leaving out the second each load lands in:

| load | line |
|---|---|
| none | `239.7–239.8 fps \| 4.17 ms`, min 2.09 to 3.92, max 4.39 to 6.02 |
| 5 ms every frame | `197.5 fps \| 5.06 ms (min 5.05, max 5.19)`. The prototype's cubes read 189.7 |
| 7.5 ms every frame | `132.1 fps \| 7.57 ms (min 7.55, max 7.66)` |
| 25 ms every 30 frames | `221.9–226.8 fps \| 4.41–4.51 ms`, max 25.09 to 25.12 |

- A `shot` shows both rows, 7 Marios each in view, evenly spaced and lined up.
- With 5 ms of load every frame, frames read 5.06 ms, not 8.33 ms (two refreshes): with three swapchain
  images FIFO never blocks, as the prototype found. The screen still shows each frame for one or two
  refreshes unevenly; only the eye sees that.
- The spike shows as a 25.1 ms max, while the average stays near the no-load 4.17 ms.
- Debug, from Scene0: F4 into the pacing scene, Ctrl+H, F1 out, F4 in again, Ctrl+P, F2. No
  `[WARNING]`, no `[ERROR]`, no asserts, 4 scenes closed, 0 leaked blocks, 0 fell to the heap.
- `scripts\run-tests.bat`: all four configurations pass.
- By eye, Vakarian's, on the 240 Hz monitor:
  - IMMEDIATE with no cap: no tear seen, and the top row moves smoothly. At thousands of fps Mario
    moves about 3 mm a frame, about 0.2 pixels, so each tear is too small to see (arithmetic from the
    scene's numbers, not measured).
  - IMMEDIATE, cap 100, fullscreen: the top row looks a little blurry and "something feels off", but no
    clear tear. Cap 30, about 30 pixels a frame, showed "small jumps". Whether Windows composes the
    image and blocks tearing in borderless fullscreen was not settled; the check stopped there.
    ROADMAP.md, Miscellaneous, holds the question.
  - The bottom row looks blurry at any setting: it holds each 60 Hz position for 4 refreshes, then
    jumps, and the eye smears it. That is the case for interpolating between fixed steps.
