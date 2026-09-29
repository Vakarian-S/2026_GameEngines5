#include "ecs/CommandBuffer.h"

#include "ecs/World.h"

void CommandBuffer::DestroyEntity(const Entity entity)
{
    Record(CommandKind::DestroyEntity, entity);
}

void CommandBuffer::Playback(World& world)
{
    for (const auto& [kind, componentType, target, payloadOffset, payloadSize] : commands)
    {
        switch (kind)
        {
        case CommandKind::DestroyEntity:
            world.DestroyEntity(target);
            break;
        case CommandKind::RemoveComponent:
            world.RemoveComponentById(target, componentType);
            break; 
        case CommandKind::CreateEntity:
            break;
        case CommandKind::AddComponent:
            world.AddComponentById(target, componentType,
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

void CommandBuffer::Record(CommandKind kind, Entity target, ComponentId type, uint32_t payloadOffset,
    uint32_t payloadSize)
{
    commands.push_back({kind, type, target, payloadOffset, payloadSize});
}
