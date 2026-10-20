#include <cassert>
#include "ecs/CommandBuffer.h"

#include "ecs/World.h"

Entity CommandBuffer::CreateEntity()
{
    const Entity placeholder{.index = kPlaceholderBit | placeholderCount, .version = 0};
    ++placeholderCount;
    Record(CommandKind::CreateEntity, placeholder);
    return placeholder;
}

void CommandBuffer::DestroyEntity(const Entity entity)
{
    Record(CommandKind::DestroyEntity, entity);
}

void CommandBuffer::Playback(World& world)
{
    assert((playedInto == nullptr || playedInto == &world) &&
        "A command buffer holds handles from one world and cannot be played into another.");
    playedInto = &world;
    created.reserve(placeholderCount);
    for (const auto& [kind, componentType, target, payloadOffset, payloadSize] : commands)
    {
        switch (kind)
        {
        case CommandKind::DestroyEntity:
            world.DestroyEntity(Resolve(target));
            break;
        case CommandKind::RemoveComponent:
            world.RemoveComponentById(Resolve(target), componentType);
            break;
        case CommandKind::CreateEntity:
            created.push_back(world.CreateEntity());
            break;
        case CommandKind::AddComponent:
            world.AddComponentById(Resolve(target), componentType,
                                   payloadSize > 0 ? payload.data() + payloadOffset : nullptr,
                                   payloadSize);
            break;
        default:
            break;
        }
    }

    Clear();
}

void CommandBuffer::Clear()
{
    commands.clear();
    payload.clear();
    created.clear();
    placeholderCount = 0;
}

uint32_t CommandBuffer::Count() const
{
    return static_cast<uint32_t>(commands.size());
}

uint32_t CommandBuffer::AppendPayload(const std::byte* value, const uint32_t size)
{
    const uint32_t offset = static_cast<uint32_t>(payload.size());
    payload.insert(payload.end(), value, value + size);
    return offset;
}

void CommandBuffer::Record(const CommandKind kind, const Entity target, const ComponentId type, const uint32_t payloadOffset,
                           const uint32_t payloadSize)
{
    commands.push_back({kind, type, target, payloadOffset, payloadSize});
}

Entity CommandBuffer::Resolve(const Entity target) const
{
    if ((target.index & kPlaceholderBit) == 0) return target;

    const uint32_t ordinal = target.index & ~kPlaceholderBit;
    assert(ordinal < created.size());

    if (ordinal >= created.size()) return Entity{};
    return created[ordinal];
}
