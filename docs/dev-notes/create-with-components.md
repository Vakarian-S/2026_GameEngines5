# Create with components: one call places an entity

Designed 2026-11-04 (commit `c643e0d`) and built 2026-11-05 (commit `66e17cf`), as step 2 of the
plan that ended on 2026-11-05, which hardened the ECS storage. It came from ROADMAP.md, ECS,
Hardening, row 2. "Step N" below means a step of that plan; README.md maps each one to its note.

Each spawn made about 9 migrations: 686,534 in 90 seconds in Scene3, and `Playback` was the most
expensive system. The space prototype paid 8 per entity through plain `AddComponent`, so the fix
belongs in the World, not only in playback.

The idea, as a sketch:

```cpp
const Entity rock = world.CreateEntity(
    Transform{ .position = { 0.0f, 5.0f, 0.0f } },
    RigidBody{ .mass = 2.0f },
    MeshRef{ cubeMesh });
```

The World finds the final archetype from all the types at once and copies each value's bytes. No
constructor runs inside the World: the caller's code builds each value, so `Transform{}` already
holds scale 1 and the identity quaternion when the World sees it. The command buffer gets the same
call, because systems spawn through it. `AddComponent` and `RemoveComponent` stay as they are, for
gameplay changes such as adding `Picked` on a grab.

Bevy spawns a tuple of components in one call, and its required components fill missing ones from
`Default`. Unity creates an entity straight from an archetype.

## Decisions

1. **No default bytes in this step.** No path here leaves a column without a value: the typed call
   takes one per component, tags have no column, and the command buffer copies every value when it
   records. No caller, no code. The first caller is File I/O, where a saved entity can miss a field;
   networking has the same need. Adding them then is pure addition: a `defaultBytes` pointer in
   `ComponentInfo`, filled in `Register<T>()` from a `static const T defaultValue{}`, and one branch
   in `CreateEntityById` that copies it for a column with no value.
2. **A typed `CreateEntity(values...)` wraps an untyped `CreateEntityById`,** the same pattern as
   `AddComponent` and `AddComponentById`. Only the untyped call places the row, so the typed call,
   playback, and File I/O later share one path. The command buffer needs the untyped call because
   its payload holds ids and bytes, not types.
3. **The command buffer records one `CreateEntityWith` command** holding every component of the new
   entity. The payload stores id, size and bytes per component, as offsets, because the payload
   vector can move as it grows. Playback builds a `ComponentValue` list with pointers into the payload
   and calls `CreateEntityById`. The placeholder works as `CreateEntity()`'s does today. Rejected:
   keeping `CreateEntity` plus `AddComponent` commands and merging them at playback. The merge breaks
   when a system interleaves commands (create A, create B, add to A), and it hides the cost instead
   of fixing the call.
4. **The same type passed twice fails.** A `static_assert` in the typed calls of the World and the
   command buffer catches it at compile time. A debug `assert` in `CreateEntityById` catches a
   repeated id while the mask is built, for playback and File I/O.

## The shape

```cpp
/** One component's id and bytes, for paths that have no type in hand. A tag passes nullptr and 0. */
struct ComponentValue
{
    ComponentId id;
    const std::byte* bytes;
    uint32_t size;
};

/** Makes an entity straight in the archetype of all the given components. */
template <typename... Ts>
Entity CreateEntity(const Ts&... values)
{
    static_assert(kAllDifferent<Ts...>, "CreateEntity got the same component type twice.");
    const ComponentValue list[] = { ValueOf(values)... };
    return CreateEntityById(list);
}

/** Makes an entity straight in the archetype of every listed component, copying each value in. */
Entity CreateEntityById(std::span<const ComponentValue> values);
```

`CreateEntityById` ORs the ids into a mask, finds or makes the archetype with `ArchetypeIndexFor`,
takes a slot, adds one row and copies each value into its column. It counts 1 in `entitiesCreated` and
one per component in `componentsAdded`, and zero migrations. Today's `CreateEntity()` with no
arguments stays; C++ prefers it over the template with an empty pack.

Playback:

```cpp
case CommandKind::CreateEntityWith:
{
    ComponentValue list[kMaxComponents];
    const uint32_t count = ReadComponents(payloadOffset, payloadSize, list);
    created.push_back(world.CreateEntityById({ list, count }));
    break;
}
```

## Build notes

- The new call only helps where code uses it. The spawn sites in the scenes and systems switch from
  `CreateEntity()` plus `AddComponent` calls to the new call, and the migration counter shows the
  drop.
- The spawn cost from step 1's measurements belongs here: each in-between hop also took and returned
  a chunk.

## What the build changed

- **The untyped call is `CreateEntityByComponentIds`, not `CreateEntityById`.** In `AddComponentById`,
  "ById" sits next to "Component". In `CreateEntityById` it sat next to "Entity" and read as the
  entity's id. A File I/O load may later want that name, to make an entity at a saved index.
- **One function places every new entity.** `CreateEntity()` with no arguments is gone, in the World
  and in the command buffer. The typed template takes the empty list with
  `if constexpr (sizeof...(Ts) == 0)` and calls `CreateEntityByComponentIds({})`. The `else` branch
  is required: C++ rejects the zero-size `list[]` that an empty list would build.
- **`CommandKind::CreateEntity` is gone.** A plain create records a `CreateEntityWith` command with an
  empty payload block.
- **`ComponentValue`, `kAllDifferent` and `COMPONENTS::ValueOf` live in `Component.h`,** because the
  World and the command buffer both use them. `kAllDifferent` is a variable template: a fold over
  `std::is_same_v` checks the first type against the rest, and a partial specialization recurses on
  the rest.
- **The payload block holds an `{id, size}` header and the bytes for each component.** The command
  keeps the block's offset and size. `ReadComponents` copies each header out with `memcpy`, because a
  component whose size is not a multiple of 4 leaves the next header at an address `uint32_t` may not
  be read from.
- **`TakeNextEntitySlot`, private to the World, holds the slot code** that `CreateEntity()` had.
- **Every spawn site uses the new call:** `PhysicsScene`, `Scene0`, `Scene1`, and `EcsBench`'s
  `Spawn` and `CullAndRespawn`. `Scene0` makes its cubes with two calls, one with the `Damageable`
  tag and one without, because the tag picks the archetype.
- **The tests:** a new "create with components" section in `EcsTests`. 257 checks pass in all four
  configurations. It checks zero migrations, zero bytes copied, one archetype and one chunk per
  create, values in the right columns whatever the order, and slot reuse. Through the command buffer
  it checks a shared placeholder count, 100 creates that make the payload grow, and an add on the
  placeholder in the same pass.

## What the build measured

`bin\x64\Release\EcsBench.exe`, against the same bench built from `5de98168`. Each is the mean of 3
warm runs, with the cold run dropped. 100,000 entities, arena mode, 16 KB chunks:

| | step 1 | step 2 |
|---|---|---|
| Spawn, ms | 20.32 | 2.78 |
| Spawn migrations | 200,000 | 0 |
| Spawn chunk takes | 200,196 | 196 |
| Chunk takes in the whole run | 1,206,688 | 6,490 |
| Playback, ms per frame | 0.418 | 0.339 |
| Movement, ns per entity | 0.73 | 0.71 |
| Arena high water | 4,758 KB | 4,726 KB |

- **Done when:** a spawn makes zero migrations, in the World (the Spawn span) and through the
  command buffer (`CullAndRespawn`'s 500,099 replacements). The run's migrations fell by 1,200,198,
  2 for each of its 600,099 creates. The 2,762,715 left all come from `ToggleTags`.
- **Spawn** is about 7 times faster than in step 1, and 16% of its time before chunks (17.1 ms). The
  196 chunk takes are what 100,000 rows need at 511 rows per chunk. At 4 and 64 KB, Spawn takes 2.83
  and 2.88 ms. At 10,000 entities it falls from 2.08 to 0.28 ms.
- **Playback** is 19% faster, above the 8% noise floor: the replacements no longer hop.
- **Decision 6 holds.** After the spawn, the run takes 6,294 chunks in 1,000 frames and returns as
  many, about 6 a frame. They come from the `Hit` toggle, which empties the tagged archetype, and
  from cull and respawn. Each is one push or pop on the free list. Not timed on its own, but 6 a
  frame against a 0.34 ms playback does not pay for a spare chunk per archetype.
