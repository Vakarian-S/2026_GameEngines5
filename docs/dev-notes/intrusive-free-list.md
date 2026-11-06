# An intrusive free list for entity slots

Designed 2026-11-04 (commit `c643e0d`) and built 2026-11-05 (commit `8ba89f3`), as step 4 of the
plan that ended on 2026-11-05, which hardened the ECS storage. It came from ROADMAP.md, ECS,
Hardening, row 12. "Step N" below means a step of that plan; README.md maps each one to its note.

A dead slot already has `archetype = kInvalidArchetype`, and its `row` holds nothing useful. The free
list stores the next free index in that field, and the World keeps one `firstFree` index instead of
the `freeSlots` vector.

```cpp
/* DestroyEntity: the dead slot points at the old first free slot. */
slot.archetype = kInvalidArchetype;
slot.row = firstFree;
firstFree = entity.index;

/* CreateEntity: take the first free slot, and move firstFree to the next one. */
index = firstFree;
firstFree = entitySlots[index].row;
```

`DoesEntityExist` already tests `archetype != kInvalidArchetype`, so it does not change. The reuse
order stays the same: the slot freed last comes back first. The risk: `row` now means two things, so
code that reads `row` without checking `archetype` first would read a free index as a row.

The gain is small, one vector less. It is in this plan because it touches the same code as step 3.
EnTT uses the same trick (skypjack, ECS back and forth, part 3). The row is not from the findings;
git first shows it in commit `9f4bc97`.

## Decisions

1. **The World keeps one `uint32_t firstFree`; a dead slot's `row` holds the next dead slot's index,
   and `kNoFreeSlot = 0xFFFFFFFF` ends the list.** It keeps today's order, so every existing slot test
   should pass unchanged and serves as the proof. No real index can reach `0xFFFFFFFF`. Threading
   changes nothing: structural changes run on one thread, and the Concurrency topic keeps playback
   sequential. The vector is no safer, since `push_back` is not thread-safe either. A lock-free list
   would face the ABA problem; a plain lock avoids it.
2. **`maxEntities` reserves only `entitySlots`.** The `freeSlots.reserve` line goes with the vector,
   and step 1's `WorldConfig` comment loses "and `freeSlots`". The saving is `maxEntities × 4` bytes
   of arena: 16 KB at 4,096.
3. **An `assert` in `CreateEntity` and a `CheckFreeList()` for the tests guard the double meaning of
   `row`.** The real risk is a stray write to a dead slot's `row`, which would break the chain with no
   error, where the vector would not care. The assert catches a broken link at the point of use.
   `CheckFreeList()`, debug only, walks the list and checks that every linked slot is free, that the
   length equals the number of free slots, and that the walk ends within `entitySlots.size()` steps.
   `EcsTests` calls it after every create, destroy and clear sequence. Today only the swap-remove
   fix-up in `DestroyEntity` writes `row` on purpose, and it writes a live slot.

```cpp
/** Ends the free list. No real slot index reaches it, so it can never collide. */
constexpr uint32_t kNoFreeSlot = 0xFFFFFFFF;

/* DestroyEntity and Clear: link the dead slot in front. */
slot.archetype = kInvalidArchetype;
slot.row = firstFree;
firstFree = index;

/* CreateEntity: unlink the front slot. */
if (firstFree != kNoFreeSlot)
{
    index = firstFree;
    assert(entitySlots[index].archetype == kInvalidArchetype && "the free list points at a live slot");
    assert((entitySlots[index].row == kNoFreeSlot || entitySlots[index].row < entitySlots.size())
           && "the free list links past the slot table");
    firstFree = entitySlots[index].row;
}
```

## Build notes

- Step 3's `Clear` sketch switches its `freeSlots.push_back(index)` to the same link as
  `DestroyEntity`.
- The memory-manager dev-note and `docs/tools/arena.md` name the free list as a vector; check them
  at commit time.

## What the build changed

- **`row` keeps its two meanings, with comments.** A separate `nextFree` field was weighed and
  rejected: `EntitySlot` would grow from 12 to 16 bytes, which costs back the same `maxEntities × 4`
  the step saves. Dropping the step and keeping `freeSlots` was the other honest choice; Bevy keeps a
  vector (`pending: Vec<u32>`), Flecs splits one dense array by `alive_count`, and EnTT links through
  its entries as here. The `row` comment in `EntitySlot.h` names both meanings and says `archetype`
  decides which one holds.
- **The World's head is `firstFreeEntitySlot`.** `TakeNextEntitySlot` unlinks the front slot,
  `DestroyEntity` and `Clear` link a freed slot in front.
- **`Clear` counts `entitiesDestroyed`,** and `SceneManager`'s ECS report prints "entities
  destroyed". Before this, the report showed more creates than live entities after R, with no line
  to explain it.
- **`WorldStats::totalRows` is gone.** Every live entity owns exactly one row, so it always equalled
  `liveEntities`. `Snapshot` now counts live slots with `count_if`, still adds up the rows for
  `emptyArchetypes`, and asserts that the two counts agree.
- **`CheckFreeList()` is public and built in every configuration.** No game code calls it, so it
  costs nothing in Release, and the Release tests check the list too. Decision 3 had said Debug only.

## What the build checked

**Tests.** `scripts\run-tests.bat` passes in all four configurations. `EcsTests` runs 305 checks,
24 more than step 3:

- `CheckFreeList` passes after every sequence that destroys: the slot tests, the row shuffles, the
  destroys through the command buffer and inside a loop, a create with components that reuses a
  slot, each clear and respawn, all 50 extra rounds, two clears in a row, and an empty world's clear.
- A new "free list" section: slots freed in the order 1, 3, 0 come back as 0, 3, 1. An empty list
  grows the table again. 300 mixed creates and destroys across two archetypes keep the list whole
  after every change.
- A clear counts every entity it destroys, 1,010, and a second clear in a row counts none.
- A respawn after a clear takes the highest index first, the same order the vector gave.
- The slot tests from before pass unchanged.

**The arena.** A new World with `maxEntities = 4096` and a 1 MB slab, printed by `EcsTests`
("a new World with maxEntities 4096 uses … arena bytes"), from `scripts\run-tests.bat <platform>
<config>`:

| config | before | after | drop |
|---|---|---|---|
| x64 Debug | 66,096 | 49,696 | 16,400 |
| x64 Release | 66,048 | 49,664 | 16,384 |
| Win32 Debug | 65,816 | 49,424 | 16,392 |
| Win32 Release | 65,792 | 49,408 | 16,384 |

Release drops exactly `4096 × 4`. Debug drops 16 bytes more on x64 and 8 on Win32, a guess: the
Debug runtime gives every vector a small bookkeeping block, and `freeSlots` took its own with it.

**The game,** in Debug, through `scripts\run-game.ps1`:

| run | steps | result |
|---|---|---|
| Every scene, R in `PhysicsScene` | `-Scene 1`: F1, F2, F3, then R three times, all three times over | exit 0, no asserts, warnings or errors, 10 scenes closed, 0 fell to the heap in every world and the frame arena, 0 leaked blocks. The last `PhysicsScene` report: 28 created, 20 chunks taken and 15 returned, 0 migrations, 7 live |

The ECS report does not print `entitiesDestroyed`, so the game run cannot show the clear's count;
the tests check it.
