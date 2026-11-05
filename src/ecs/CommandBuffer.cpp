#include <cassert>
#include <cstring>
#include "ecs/CommandBuffer.h"

#include "ecs/World.h"

Entity CommandBuffer::CreateEntityByComponentIds(const std::span<const ComponentValue> componentValues)
{
    const Entity placeholder{.index = kPlaceholderBit | placeholderCount, .version = 0};
    ++placeholderCount;

    const uint32_t start = static_cast<uint32_t>(payload.size());
    for (const auto& [id, bytes, size] : componentValues)
    {
        const ComponentHeader header{id, size};
        AppendPayload(reinterpret_cast<const std::byte*>(&header), sizeof(header));
        if (size > 0) AppendPayload(bytes, size);
    }

    Record(CommandKind::CreateEntityWith, placeholder, kNoComponent, start,
           static_cast<uint32_t>(payload.size()) - start);
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
        case CommandKind::AddComponent:
            world.AddComponentById(Resolve(target), componentType,
                                   payloadSize > 0 ? payload.data() + payloadOffset : nullptr,
                                   payloadSize);
            break;
        case CommandKind::CreateEntityWith:
            {
                ComponentValue list[kMaxComponents];
                const uint32_t count = ReadComponents(payloadOffset, payloadSize, list);
                created.push_back(world.CreateEntityByComponentIds({list, count}));
                break;
            }
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

uint32_t CommandBuffer::ReadComponents(const uint32_t offset, const uint32_t size, ComponentValue* out) const
{
    uint32_t count = 0;
    uint32_t position = offset;
    const uint32_t end = offset + size;
    while (position < end)
    {
        /* A header can sit at any byte, so it's safer to copy it */
        ComponentHeader header;
        std::memcpy(&header, payload.data() + position, sizeof(header));
        position += sizeof(header);
        assert(
            count < kMaxComponents &&
            "CommandBuffer::ReadComponents: A create holds more components than a mask has bits");
        out[count] = {
            .id = header.id,
            .bytes = header.size > 0 ? payload.data() + position : nullptr,
            .size = header.size
        };
        count++;
        position += header.size;
    }
    return count;
}

void CommandBuffer::Record(const CommandKind kind, const Entity target, const ComponentId type,
                           const uint32_t payloadOffset,
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
