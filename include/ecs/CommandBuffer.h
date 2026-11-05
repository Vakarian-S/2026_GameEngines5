#pragma once
#include <cstdint>
#include <vector>
#include <cstddef>
#include <span>

#include "Component.h"
#include "Entity.h"

class World;

/** Which of the four structural changes a Command names. */
enum class CommandKind : uint8_t
{
    DestroyEntity,
    AddComponent,
    RemoveComponent,
    CreateEntityWith
};

/** One recorded structural change. Any value it carries lives in the buffer's payload. */
struct Command
{
    CommandKind kind;
    ComponentId componentType;
    Entity target;
    uint32_t payloadOffset;
    uint32_t payloadSize;
};

/**
 * Records structural changes while systems run and applies them at a sync point.
 *
 * Recording touches nothing outside this object, so a system can queue a change while
 * walking the very block that change would reshuffle.
 */
class CommandBuffer
{
public:
    /**
    * Queues an entity made straight in the archetype of all the given components, copying each value now.
    * @return A placeholder to pass to this buffer's other calls in the same pass
    */
    template <typename... Ts>
    Entity CreateEntity(const Ts&... values)
    {
        static_assert(kAllDifferent<Ts...>, "CreateEntity got the same component type twice.");
        if constexpr (sizeof...(Ts) == 0) return CreateEntityByComponentIds({});
        else
        {
            const ComponentValue list[] = {COMPONENTS::ValueOf(values)...};
            return CreateEntityByComponentIds(list);
        }
    }

    /**
     * Records a create command on an entity with a group of components defined by their ids.
     * @param componentValues values of each of the components to add to the entity
     * @return placeholder entity handle of the created entity.
     */
    Entity CreateEntityByComponentIds(std::span<const ComponentValue> componentValues);
    
    /**
     * Records a command to delete an entity in a given handle.
     * @param entity handle of the entity to delete.
     */
    void DestroyEntity(Entity entity);

    /**
     * Queues the removal of a component. A component the entity does not have is ignored
     * at playback.
     * @tparam T Type of the component to remove
     * @param entity Entity to remove the component from
     */
    template <typename T>
    void RemoveComponent(const Entity entity)
    {
        Record(CommandKind::RemoveComponent, entity, COMPONENTS::IdOf<T>());
    }

    /**
     * Queues adding a component, copying its value into the payload now.
     * A component the entity already has has its value overwritten at playback.
     * @tparam T Type of the component to add
     * @param entity Entity to add the component to
     * @param value Value the component will hold
     */
    template <typename T>
    void AddComponent(const Entity entity, const T& value = T{})
    {
        if constexpr (std::is_empty_v<T>)
        {
            Record(CommandKind::AddComponent, entity, COMPONENTS::IdOf<T>());
        }
        else
        {
            const uint32_t offset = AppendPayload(reinterpret_cast<const std::byte*>(&value), sizeof(T));
            Record(CommandKind::AddComponent, entity, COMPONENTS::IdOf<T>(),
                   offset, static_cast<uint32_t>(sizeof(T)));
        }
    }

    /** Applies every recorded command in order, then clears the buffer. */
    void Playback(World& world);

    /** Drops every recorded command without applying any of them. */
    void Clear();

    /** How many commands are waiting. */
    uint32_t Count() const;

private:
    std::vector<Command> commands;
    std::vector<std::byte> payload;
    uint32_t placeholderCount = 0;
    std::vector<Entity> created;

    /** The world this buffer has been played into, or nullptr before the first playback. */
    const World* playedInto = nullptr;

    /** Copies size bytes onto the end of the payload and returns where they landed. */
    uint32_t AppendPayload(const std::byte* value, uint32_t size);

    /** What the payload holds in front of each component's bytes in a CreateEntityWith command. */
    struct ComponentHeader
    {
        ComponentId id;
        uint32_t size;
    };

    /**
     * Turns one CreateEntityWith block of the payload back into a list of components.
     * @param offset Where the block starts in the payload
     * @param size Bytes the block takes
     * @param out Filled with one entry per component, pointing into the payload
     * @return How many entries were filled
     */
    uint32_t ReadComponents(uint32_t offset, uint32_t size, ComponentValue* out) const;

    /**
     * Appends one command to the list.
     * @param kind Which structural change this command names
     * @param target Entity the command is about
     * @param type Id of the component type, or kNoComponent when the kind names none
     * @param payloadOffset Where the recorded value starts in the payload
     * @param payloadSize Bytes the recorded value takes, or zero when it carries none
     */
    void Record(CommandKind kind, Entity target, ComponentId type = kNoComponent,
                uint32_t payloadOffset = 0, uint32_t payloadSize = 0);

    /**
    * Turns a placeholder into the entity it became. A real handle passes through unchanged.
    * @param target Handle recorded on a command
    * @return The real entity, or a default handle when the placeholder names nothing
    */
    Entity Resolve(Entity target) const;
};
