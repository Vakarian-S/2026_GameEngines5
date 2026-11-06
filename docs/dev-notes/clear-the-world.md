# Clear the World in one call

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-11-04 (commit `c643e0d`) and built 2026-11-05 (commit `e72e731`), as step 3 of the
plan that ended on 2026-11-05, which hardened the ECS storage. It came from ROADMAP.md, ECS,
Hardening, row 4. "Step N" below means a step of that plan; README.md maps each one to its note.

A restart destroys every entity one at a time. One call should empty every archetype and reset the
entity slots. With chunks, this means handing every chunk back to the pool.

Bevy's `World` has `clear_entities`. From memory, EnTT has `registry.clear()`.

**Where the need came from.** On `main`, nothing destroys entities one by one: F3 in `PhysicsScene`
goes through `SceneManager`, which destroys the whole scene and its World and builds new ones. The
row came from the prototype games, which restart a round inside a running scene: Scene2's
`RestartRound` and Scene3's R key (findings/2026-10-prototype-game.md, findings 8 in both sections).
They could not ask `SceneManager` for a rebuild, because a scene cannot reach it. A clear keeps what
lives outside the World (score, settings, camera), the systems and their cached queries, and the
World's slab.

Later callers: loading a save (File I/O: clear, then `CreateEntityById` per saved entity), a network
resync (Networking: the server sends the full state), `EcsBench` reusing one World between cases, and
an editor's play mode restoring the level, which builds on loading a save.

## Decisions

1. **Keep the step, with `PhysicsScene`'s restart as the caller.** Restarting a round is the normal
   case in a game, the prototypes needed it twice, and File I/O becomes the real caller later.
2. **`Clear` keeps the slot table, bumps the version of every live slot and marks it free.** A handle
   held from before must read as dead. Emptying the slot vector would restart versions at 1, so an
   old `{index 3, version 1}` would match the first new entity at index 3. With step 4, marking free
   becomes the same two-line link as in `DestroyEntity`.
3. **`Clear` keeps every archetype.** Each one hands its chunks back to the pool and drops to 0 rows;
   its mask and layout stay, and `archetypeGeneration` does not change. A restart spawns the same
   kinds of entities again. Destroying archetypes would leave dead copies in the arena, which cannot
   free, so the slab would fill a little more every round, and every cached query would search
   again. The respawn reuses the chunks from the pool's free list.
4. **`Clear` is a direct World call, made only from scene code outside the phases. It is not a
   command.** Outside the phases no system is mid-loop, and the command buffer is empty, because
   `RunPhase` plays it back at the end of every phase. Inside a system, a loop would keep a pointer
   into a chunk that went back to the pool. As a command, a clear would run between other commands,
   and a bullet queued before it would appear in the fresh world. No debug check: it would need the
   World to see the registry's running flag.
5. **`PhysicsScene` gets `SpawnShapes()`.** `OnCreate` keeps the asset loads and every
   `systems.Add`, then calls `SpawnShapes()`, which makes the two cubes, the four Marios and the
   `PickRay` entity. R in `PhysicsScene::Update`, next to the C and M toggles, calls `world.Clear()`
   and then `SpawnShapes()`. Not F3: F1 to F4 belong to `Application` for switching scenes, and F3's
   rebuild stays, so the two paths can be compared. Both prototypes used R.

```cpp
void World::Clear()
{
    for (Archetype* archetype : archetypes)
    {
        archetype->ReleaseChunks();
    }

    for (uint32_t index = 0; index < entitySlots.size(); ++index)
    {
        EntitySlot& slot = entitySlots[index];
        if (slot.archetype == kInvalidArchetype) continue;
        slot.version++;
        slot.archetype = kInvalidArchetype;
        freeSlots.push_back(index);
    }
}
```

## Build notes

- Step 2 lands first, so `SpawnShapes()` uses `CreateEntity(values...)`: one call per shape instead of
  seven.
- In the counters, `chunksReturned` jumps on the clear frame and `chunksTaken` matches it on the
  respawn, with the arena's used bytes unchanged.
- A test: hold a handle, clear, and check that `DoesEntityExist` is false and that the next entity
  at that index has a higher version.

## What the build changed

- **`Archetype::ReleaseChunks()`** gives every chunk back to the pool, empties the chunk list and sets
  the row count to 0. The destructor now calls it, so one loop does both jobs.
- **`World::Clear()`** is the sketch above: release every archetype's chunks, then bump and free each
  live slot. It does not count `entitiesDestroyed`, so a scene's ECS report shows more creates than
  live entities after a clear.
- **`PhysicsScene::SpawnShapes()`** holds the six shapes and the `PickRay`. `OnCreate` calls it before
  adding the systems, and R in `Update` calls `world.Clear()` and then `SpawnShapes()`.
- **No system keeps an `Entity` across frames.** `PickSystem`'s `pickedEntity` is a local, and a grab
  lives in the `Picked` component, so R while holding a shape clears the grab with the rest.
- **Scene1's slab went from 16 KB to 64 KB.** The game run below found Scene1 stopping on the arena
  assert at startup. Since step 1 an archetype with rows holds at least one 16 KB chunk, and Scene1
  has two, so 16 KB could not even hold the slots and one chunk. Step 1 checked `PhysicsScene`, not
  Scene1, so it went unseen from `5de98168` on.

## What the build checked

**Tests.** `scripts\run-tests.bat` passes in all four configurations. `EcsTests` runs 281 checks, 24
of them in a new "clear" section, on a world of 1,010 entities in two archetypes:

- A handle from before the clear reads as gone and reads no component, before and after the respawn.
- The clear leaves no rows but keeps every archetype, the generation and the slot table, and returns
  every chunk in use to the free list.
- The respawn takes back exactly the chunks the clear returned, with no new arena bytes, nothing sent
  to the heap and no new archetype. Every respawned entity has version 2, with its new values.
- 50 more clear and respawn rounds leave the arena's used bytes, the heap count and the free chunks
  unchanged.
- Two clears in a row free each slot once, so no two entities share an index.
- Clearing an empty world is harmless.

**The game,** in Debug, through `scripts\run-game.ps1`:

| run | steps | result |
|---|---|---|
| R in `PhysicsScene` | `-Scene 0`: F3, then R three times | exit 0, no asserts, warnings or errors, 0 fell to the heap, 0 leaked blocks. The ECS report: 28 created (7 shapes, 4 times), 20 chunks taken and 15 returned, 0 migrations, 7 live. The screenshot after R shows the six shapes back where they start |
| Scene1 before the fix | `-Scene 1`, wait | the arena assert, exit code 3 |
| Scene1 after the fix | `-Scene 1`, wait | exit 0, clean, 0 leaked blocks |
| Every scene, five times | `-Scene 1`: F1, F2, F3, R, five times | 16 scenes closed, no asserts, warnings or errors, 0 fell to the heap in every world and the frame arena, 0 leaked blocks |
