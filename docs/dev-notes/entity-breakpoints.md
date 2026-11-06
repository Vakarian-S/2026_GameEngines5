# Entity breakpoints: the DebugWatch tag

Designed 2026-11-04 (commit `c643e0d`) and built 2026-11-05 (commit `d145432`), as step 5 of the
plan that ended on 2026-11-05, which hardened the ECS storage. It came from ROADMAP.md, ECS,
Hardening, row 7. "Step N" below means a step of that plan; README.md maps each one to its note.

A data breakpoint cannot follow an entity: adding or removing a component moves its bytes with a
memcpy, and the breakpoint keeps watching the old address (ECS.md house rule 11). A conditional
breakpoint on the entity index works, but you must find the index first, and the debugger tests the
condition on every row of every frame, so the scene crawls.

**The case.** Cube 2 jitters after a release. You add `DebugWatch` to cube 2 in the code and run
under Rider. Every system that reaches cube 2 stops, and only for cube 2. The tag moves with the cube
through every migration, because it is part of the entity's archetype.

**A change from the roadmap row.** The row asked for a flag per block (a chunk, in this plan). A
`DebugWatch` tag already puts the entity in its own archetype, so the archetype's mask answers the
question, once per archetype. A per-chunk flag only helps if watching becomes a bit that does not
move the entity (roadmap row 10). So this step does not need chunks. With step 1's `ForEachChunk`,
the check can live inside the query, so no system writes it by hand.

Unreal Mass has the same idea: select an entity, then break when a processor reaches it, or when
code writes one of its fragments. Its breakpoints fire only in loops that use Mass's own entity
iterator. Unity has inspection and Journaling, a record of what happened to each entity, but no
breakpoints.

## Decisions

1. **The check lives inside `ForEachChunk`.** The query tests each matching archetype's mask for
   `DebugWatch` once, then calls `DEBUG::BreakOnWatch()` before it hands each chunk of a watched
   archetype to the system's lambda. No system writes the check, and no system needs `DebugWatch` in
   its query: a query matches any archetype that has its components, extra ones included, and the
   check reads the archetype's mask, not the query's. Adding the tag makes a new archetype, which
   bumps the generation, so every cached query picks it up. After the stop, F11 steps into the
   system's row loop; with one entity watched, its archetype holds one row, and row 0 is the entity.
2. **The break is armed by default, and Rider turns it off and on.** `DEBUG::watchBreakArmed` starts
   `true`. Set it to `false` in Rider's watch window to let the game run past the watched entity, and
   back to `true` for the next stop. Mass works the same way: its breakpoint is cleared by setting
   `bDisableThisBreakpoint` in the IDE's watch window. A global flag, because the World store
   (roadmap row 5) does not exist yet and the flag belongs to the debugger, not the game. Rejected: a
   key that arms the break for one frame. It needed a system to read the key, and a frame with no
   Fixed step would skip the Fixed systems.
3. **Without a debugger attached, nothing happens.** `BreakOnWatch` returns unless
   `IsDebuggerPresent()` is true. That Windows function answers whether any debugger (Rider, Visual
   Studio, WinDbg) is attached, whatever the build type, so the guard also works in Release under
   Rider. `__debugbreak()` with no debugger would raise an exception nothing handles, and Windows
   would close the game. Rejected: printing a line per system instead. No log is needed now.
4. **No tagging system. The developer adds the tag in their own code while debugging, and removes
   it afterwards.** A system that tagged the held shape would depend on picking and be useless in a
   scene without it. The tag can go in at creation, or at the moment a bug appears, through the
   command buffer.

```cpp
/* core/debug: the flag and the break. In a .cpp, so <windows.h> stays out of every header. */
namespace DEBUG
{
    /** Set to false in Rider's watch window to let the game run past watched entities. */
    inline bool watchBreakArmed = true;
}

void DEBUG::BreakOnWatch()
{
    if (!watchBreakArmed || !IsDebuggerPresent()) return;
    __debugbreak();
}

/* Query: the check, once per archetype. */
template <typename F>
void Query::ForEachChunk(F&& visit)
{
    for (Archetype* archetype : Matches())
    {
        const bool watched = (archetype->Mask() & COMPONENTS::MaskOf<DebugWatch>()) != 0;
        for (Chunk& chunk : archetype->Chunks())
        {
            if (watched) DEBUG::BreakOnWatch();
            visit(ChunkView{ archetype, &chunk });
        }
    }
}

/* Game code, while debugging: at creation, or when the bug appears. */
world.AddComponent<DebugWatch>(cube2);
context.commands.AddComponent<DebugWatch>(owners[row]);
```

## Cost

The check runs once per archetype per query, not once per entity: 10,000 entities in one archetype
pay one mask test, and the row loop holds no check. With no tag in the code, `BreakOnWatch` never
runs, so `IsDebuggerPresent` never runs either. From memory, it reads one flag in the process's own
memory. Release keeps all of it; a shipping build would put it behind the define from the roadmap's
"A shipping build" row.

## Build notes

- `DebugWatch` lives in `ecs/`, because `Query` reads it and `ecs` must not include `physics` or
  `core`. `DEBUG::BreakOnWatch` and the flag live in `core/debug`, next to the line helpers; check
  that `ecs` may call into `core/debug`, or move the two into `ecs/`.
- `FixedStep` caps the catch-up time (`kMaxAccumulated`), so physics should not leap after a long
  stop. Vulkan and SDL after a pause need a real run.

Checks for the build:

| # | check | how |
|---|---|---|
| 1 | The break hits only the watched entity | Tag cube 2 in `SpawnShapes`, run under Rider, stop in `ApplyForcesSystem`, read row 0's entity |
| 2 | The tag survives grab and release | Unit test: tag, add `Picked`, remove `Picked`, check the tag |
| 3 | Turning the flag off lets the game run | Set `DEBUG::watchBreakArmed` to `false` in Rider's watch window, continue |
| 4 | No crash without a debugger | Run the plain exe with cube 2 tagged |
| 5 | The game recovers after a long pause | Wait a minute in the debugger, then continue |
| 6 | The check costs nothing with no tag | `EcsBench` at 100,000 rows, with and without the check |

## What the build changed

- **`include/ecs/debug/DebugWatch.h` and `src/ecs/debug/DebugWatch.cpp`,** not `core/debug`. `ecs`
  never includes `core` (STATE.md, architecture rules 2 and 3). The tag, the flags and
  `BreakOnWatch` sit in the `DEBUG` namespace, so the tag is `DEBUG::DebugWatch`. Only the `.cpp`
  includes `<windows.h>`.
- **A system filter, added during the build.** With no filter, the game stopped in every system on
  the first frame, rendering included, so a bug could never show before the first stop.
  `RunPhase` writes the running system's name to `DEBUG::currentSystem` before each `Run` and
  clears it to `nullptr` after the loop. `BreakOnWatch` returns unless `DEBUG::watchSystem` is
  `nullptr` or names that system, compared by content with `std::string_view`. A query run outside
  any system never matches a filter.
- **Decision 2 changed: the break starts off.** Rider's watch window would not edit the flag, so
  `watchBreakArmed` defaults to `false`, and the developer turns it on in code. To debug: tag the
  entity, set `DEBUG::watchBreakArmed = true`, optionally set `DEBUG::watchSystem` to one system's
  name, run under Rider, and remove the lines afterwards. `MouseConstraint` makes a good filter for
  a held shape: its query needs `Picked`, so it reaches the shape only while it is held. `Pick`
  does not: its collider loop visits every pickable shape on every click.
- **`[[unlikely]]` on the check in `ForEachChunk`.** Without it, the call to `BreakOnWatch` slowed
  two of the three `EcsBench` systems by about 45%; see the table below. The mask of the tag is
  built once per call, above the archetype loop.

## What the build checked

**Tests.** `scripts\run-tests.bat` passes in all four configurations. `EcsTests` runs 314 checks,
9 more than step 4, in a "debug watch" section:

- The tag moves the entity to its own archetype, and survives a component added and then removed
  (check 2), with the data intact.
- A query that never names the tag still reaches the watched entity and visits both chunks. CTest
  runs with no debugger, so this also shows `BreakOnWatch` returns and the loop goes on.
- `RunPhase` names the running system in `DEBUG::currentSystem` and clears it after the phase.

**Under Rider** (checks 1, 3 and 5), run by Vakarian with cube 2 tagged: every stop was cube 2's
archetype, holding one row; the game ran on after the flag was turned off; and it recovered after a
long pause. A dump from the stop in `UpdateOrientationSystem` showed `watched = true` on the
archetype with mask 127 (bit 6 is the tag) and `count = 1`, while the untagged cube sat alone in
mask 63. Rider's display of the lambda there was wrong: every system's first lambda is named
`<lambda_1>`, and it showed another system's captures. Setting the flag from the watch window
later failed, which led to the default change above.

**The plain exe** (check 4), through `scripts\run-game.ps1` with cube 2 tagged, `-Scene 1`: F3,
R twice, F1, F2, F3. Exit 0 in Debug and Release, no asserts, warnings or errors, 0 fell to the
heap, 0 leaked blocks in Debug. `PhysicsScene` had 6 archetypes instead of 5, and the report read
14 entities destroyed after two R presses.

**The cost** (check 6). `EcsBench`, x64 Release, 100,000 rows, arena, 16 KB chunks. Each cell is
the mean of the average frame time over runs 2 to 4 of four, measured with a scratchpad script
around `.\bin\x64\Release\EcsBench.exe`:

| build | Movement | ToggleTags | CullAndRespawn |
|---|---|---|---|
| step 4, committed | 0.075 ms | 0.112 ms | 0.190 ms |
| the check, no `[[unlikely]]` | 0.075 ms | 0.161 ms | 0.247 ms |
| the check with `[[unlikely]]` | 0.074 ms | 0.113 ms | 0.189 ms |

The first "after" run showed no cost: that build happened to get a good code layout. A scratch
build with `/QIntel-jcc-erratum`, which pads jumps so their alignment stops mattering, showed the
cost every time. Variants in that build:

- the branch with a cheap write instead of the call: fast, so the call costs, not the check;
- two loops, one for watched archetypes and one for the rest: still slow;
- `[[unlikely]]`: fast.

Movement's loop is plain math with no calls, and it never changed. Why the stride loop suffers is a
guess, not checked in the assembly: its lambda keeps `firstRow` and `current` across chunks, so
they live across the call.

`[[unlikely]]` on `Archetype::AddRow`'s new-chunk branch was tried too, and gained nothing beyond
the noise: Spawn measured 2.81 ms and 3.14 ms on two passes of the same build. It was removed.

**The game,** with no tag, Scene0 (1,000 cubes) for 15 seconds, three runs each, Release, step 4's
committed build against this one: every system within 0.004 ms, and every run exited 0. At 1,000
rows the systems take a few microseconds, below what this test can separate.
