#pragma once
#include <vector>
#include <glm/fwd.hpp>

#include "Component.h"
#include "Entity.h"

/* One column for each component type's bytes */
struct ComponentColumn
{
    ComponentId type;
    uint32_t elementSize;
    std::vector<std::byte> data;
};

/* A collection of entities with the same components */
class Archetype
{
public:
    explicit Archetype(ComponentMask mask);

    /**
     * Appends a row (entity and components). The component bytes start zeroed
     * @param entity entity to add
     * @return index of the added row.
     */
    uint32_t AddRow(Entity entity);

    /**
     * Removes a row by moving the last one into it. 
     * @param row the row of the entity to delete  
     * @return The moved entity, or {0,0} if none was moved.
     */
    Entity RemoveRow(uint32_t row);

    /**
     * First element of a column, or nullptr if the archetype has no such column
     * @tparam T 
     * @return 
     */
    template <typename T>
    T* Column()
    {
        ComponentColumn* column = FindColumn(COMPONENTS::IdOf<T>());
        return column != nullptr ? reinterpret_cast<T*>(column->data.data()) : nullptr;
    }

    /**
     * Returns the amount of component columns in the archetype, this does not count tags.
     * @return Amount of component columns not counting tags.
     */
    uint32_t ColumnCount() const;

    ComponentMask Mask() const;

    /**
     * Gets the amount of rows on the archetype
     * @return rows as an int.
     */
    uint32_t Count() const;
    
    /** The entity owning each row, parallel to every component column. */
    const Entity* EntityColumn() const { return entities.data(); }

private:
    ComponentMask mask;
    std::vector<ComponentColumn> componentColumns;
    std::vector<Entity> entities;

    /**
     * Get the component column of the given component type
     * @param type Type of component to find
     * @return The column
     */
    ComponentColumn* FindColumn(ComponentId type);
};
