#include "ecs/Archetype.h"

#include <cassert>
#include <cstring>

ComponentColumn::ComponentColumn(const ComponentId type_, const uint32_t elementSize_,
                                 std::pmr::memory_resource* memory)
    : type(type_), elementSize(elementSize_), data(memory)
{
}

Archetype::Archetype(const ComponentMask mask, std::pmr::memory_resource* memory, const uint32_t reservedRows)
    : mask(mask), componentColumns(memory), entities(memory), reservedRows(reservedRows)
{
    /* Counted first, so the column list is allocated once at its final size. */
    uint32_t columnCount = 0;
    for (ComponentId id = 0; id < kMaxComponents; id++)
    {
        if ((mask & ComponentMask{1} << id) != 0 && COMPONENTS::InfoOf(id).size > 0) ++columnCount;
    }
    componentColumns.reserve(columnCount);

    /* Navigate the bits in ascending order so component always land in ascending id order */
    for (ComponentId id = 0; id < kMaxComponents; id++)
    {
        if ((mask & ComponentMask{1} << id) == 0) continue;
        /* If its a tag, it stores nothing */
        const uint32_t elementSize = COMPONENTS::InfoOf(id).size;
        if (elementSize == 0) continue;
        componentColumns.emplace_back(id, elementSize, memory);
        componentColumns.back().data.reserve(static_cast<size_t>(reservedRows) * elementSize);
    }

    entities.reserve(reservedRows);
}

std::byte* Archetype::ElementAt(ComponentId type, uint32_t row)
{
    ComponentColumn* column = FindColumn(type);
    if (column == nullptr) return nullptr;
    return column->data.data() + row * column->elementSize;
}

uint32_t Archetype::AddRow(const Entity entity)
{
    const uint32_t row = static_cast<uint32_t>(entities.size());

    /* Past the reserve a column takes a new block and abandons the old one inside the arena. */
    assert((reservedRows == 0 || row < reservedRows) && "the archetype is full, raise maxEntities");

    entities.push_back(entity);

    for (ComponentColumn& column : componentColumns)
    {
        column.data.resize(static_cast<size_t>(row + 1) * column.elementSize);
    }
    return row;
}

Entity Archetype::RemoveRow(uint32_t row)
{
    assert(row < entities.size());

    const uint32_t last = static_cast<uint32_t>(entities.size()) - 1;
    Entity moved{0, 0};

    if (row != last)
    {
        for (ComponentColumn& column : componentColumns)
        {
            std::byte* base = column.data.data();
            std::memcpy(base + static_cast<size_t>(row) * column.elementSize,
                        base + static_cast<size_t>(last) * column.elementSize,
                        column.elementSize);
        }
        entities[row] = entities[last];
        moved = entities[row];
    }

    entities.pop_back();
    for (ComponentColumn& column : componentColumns)
    {
        column.data.resize(last * column.elementSize);
    }
    return moved;
}


uint32_t Archetype::ColumnCount() const
{
    return static_cast<uint32_t>(componentColumns.size());
}

ComponentMask Archetype::Mask() const
{
    return mask;
}

uint32_t Archetype::Count() const
{
    return static_cast<uint32_t>(entities.size());
}

ComponentColumn* Archetype::FindColumn(const ComponentId type)
{
    for (ComponentColumn& column : componentColumns)
    {
        if (column.type == type)
        {
            return &column;
        }
    }
    return nullptr;
}
