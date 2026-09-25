#include "ecs/Archetype.h"

Archetype::Archetype(const ComponentMask mask) : mask(mask)
{
    /* Navigate the bits in ascending order so component always land in ascending id order */
    for (ComponentId id = 0; id < kMaxComponents; id++)
    {
        if ((mask & ComponentMask{1} << id) == 0) continue;
        /* If its a tag, it stores nothing */
        const uint32_t elementSize = COMPONENTS::InfoOf(id).size;
        if (elementSize == 0) continue;
        componentColumns.push_back({id, elementSize, {}});
    }
}

uint32_t Archetype::AddRow(const Entity entity)
{
    const uint32_t row = static_cast<uint32_t>(entities.size());
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
