#pragma once
#include <cstddef>
#include <memory_resource>
#include <vector>

#include "Component.h"
#include "Entity.h"

/* One column for each component type's bytes */
struct ComponentColumn
{
    ComponentId type;
    uint32_t elementSize;
    std::pmr::vector<std::byte> data;

    /**
     * @param type_ component type this column stores
     * @param elementSize_ bytes one element takes
     * @param memory where the bytes come from
     */
    ComponentColumn(ComponentId type_, uint32_t elementSize_, std::pmr::memory_resource* memory);

    /* A copied pmr vector lands on the default heap, so a column can only be moved. */
    ComponentColumn(const ComponentColumn&) = delete;
    ComponentColumn& operator=(const ComponentColumn&) = delete;
    ComponentColumn(ComponentColumn&&) noexcept = default;
};

/* A collection of entities with the same components */
class Archetype
{
public:
    /**
     * @param mask one bit per component type this archetype holds
     * @param memory where the columns and the entity list allocate
     * @param reservedRows rows to reserve up front. Zero reserves nothing and the columns grow.
     */
    explicit Archetype(ComponentMask mask, std::pmr::memory_resource* memory, uint32_t reservedRows = 0);

    /**
     * Raw bytes of one component in one row, or nullptr if this archetype has no such column. 
     * @param type Component Id of the component type column to search from
     * @param row row where the component lives
     * @return bytes of the component or nullptr if component is not found
     */
    std::byte* ElementAt(ComponentId type, uint32_t row);

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

    /**
     * Gets the Mask of the Component, bits based on the components that it owns (i.e 1001100)
     * @return uint64_t mask of the archetype
     */
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
    std::pmr::vector<ComponentColumn> componentColumns;
    std::pmr::vector<Entity> entities;
    /** Rows reserved at creation, or zero when the columns grow on demand */
    uint32_t reservedRows;

    /**
     * Get the component column of the given component type
     * @param type Type of component to find
     * @return The column
     */
    ComponentColumn* FindColumn(ComponentId type);
};
