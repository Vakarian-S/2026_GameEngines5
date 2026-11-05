#include "ecs/Archetype.h"

#include <cassert>
#include <cstring>

#include "ecs/ChunkPool.h"

Archetype::Archetype(const ComponentMask mask, std::pmr::memory_resource* memory, ChunkPool* pool)
    : mask(mask),
      columns(memory), chunks(memory), pool(pool)
{
    /* First pass: the totals, so the layout list is allocated once and the row count is known. */
    uint32_t layoutCount = 0;
    uint32_t rowBytes = sizeof(Entity);
    uint32_t worstPadding = 0;
    for (ComponentId id = 0; id < kMaxComponents; id++)
    {
        if ((mask & ComponentMask{1} << id) == 0) continue;
        const ComponentInfo& info = COMPONENTS::InfoOf(id);
        if (info.size == 0) continue;
        ++layoutCount;
        rowBytes += info.size;
        worstPadding += info.alignment - 1;
    }

    /* Taking the worst padding off first means the offsets below can never run past the chunk. */
    rowsPerChunk = (pool->ChunkBytes() - worstPadding) / rowBytes;
    assert(rowsPerChunk > 0 && "one row is bigger than a chunk, raise chunkBytes");

    /* Second pass: the entity slice sits at 0, then each column in ascending id order. */
    columns.reserve(layoutCount);
    uint32_t offset = rowsPerChunk * static_cast<uint32_t>(sizeof(Entity));
    for (ComponentId id = 0; id < kMaxComponents; id++)
    {
        if ((mask & ComponentMask{1} << id) == 0) continue;
        const ComponentInfo& info = COMPONENTS::InfoOf(id);
        if (info.size == 0) continue;
        offset = (offset + info.alignment - 1) & ~(info.alignment - 1);
        columns.push_back({id, info.size, offset});

        offset += rowsPerChunk * info.size;
    }
    assert(offset <= pool->ChunkBytes() && "the layout ran past the end of the chunk");
}

Archetype::~Archetype()
{
    for (const auto& [bytes, count] : chunks)
    {
        pool->ReturnChunk(bytes);
    }
}

std::byte* Archetype::ElementAt(const ComponentId type, const uint32_t row)
{
    const ColumnLayout* column = FindLayout(type);
    if (column == nullptr) return nullptr;
    const uint32_t chunkNumber = row / rowsPerChunk;
    return chunks[chunkNumber].bytes + column->offset + row % rowsPerChunk * column->elementSize;
}

const std::byte* Archetype::ElementAt(const ComponentId type, const uint32_t row) const
{
    const ColumnLayout* column = FindLayout(type);
    if (column == nullptr) return nullptr;
    const uint32_t chunkNumber = row / rowsPerChunk;
    return chunks[chunkNumber].bytes + column->offset + row % rowsPerChunk * column->elementSize;
}

uint32_t Archetype::AddRow(const Entity entity)
{
    if (chunks.empty() || chunks.back().count >= rowsPerChunk)
    {
        chunks.push_back({pool->TakeChunk(), 0});
    }
    auto& [bytes, count] = chunks.back();
    const uint32_t localChunkRow = count;
    std::byte* startOfBuffer = bytes;
    std::memcpy(
        startOfBuffer + localChunkRow * sizeof(Entity),
        &entity,
        sizeof(Entity)
    );
    for (const ColumnLayout& column : columns)
    {
        std::memset(
            startOfBuffer + column.offset + localChunkRow * column.elementSize,
            0,
            column.elementSize
        );
    }
    count++;
    return rowsCount++;
}

Entity Archetype::RemoveRow(const uint32_t row)
{
    assert(row < rowsCount);

    const uint32_t lastRow = rowsCount - 1;
    auto& [lastChunkBytes, lastChunkCount] = chunks.back();
    const uint32_t lastLocalRow = lastChunkCount - 1;
    Entity moved{.index = 0, .version = 0};

    if (row != lastRow)
    {
        const auto& [targetChunkBytes, targetChunkCount] =
            chunks[row / rowsPerChunk];
        const uint32_t targetLocal = row % rowsPerChunk;

        std::memcpy(targetChunkBytes + targetLocal * sizeof(Entity),
                    lastChunkBytes + lastLocalRow * sizeof(Entity),
                    sizeof(Entity));
        for (const ColumnLayout& column : columns)
        {
            std::memcpy(targetChunkBytes + column.offset + targetLocal * column.elementSize,
                        lastChunkBytes + column.offset + lastLocalRow * column.elementSize,
                        column.elementSize);
        }

        /* Read before the counts change, because the last chunk may go back to the pool below. */
        std::memcpy(&moved, targetChunkBytes + targetLocal * sizeof(Entity), sizeof(Entity));
    }
    lastChunkCount--;
    rowsCount--;

    if (lastChunkCount == 0)
    {
        pool->ReturnChunk(lastChunkBytes);
        chunks.pop_back();
    }

    return moved;
}


uint32_t Archetype::ColumnCount() const
{
    return static_cast<uint32_t>(columns.size());
}

ComponentMask Archetype::Mask() const
{
    return mask;
}

uint32_t Archetype::Count() const
{
    return rowsCount;
}

ColumnLayout* Archetype::FindLayout(const ComponentId type)
{
    for (ColumnLayout& column : columns)
    {
        if (column.type == type)
        {
            return &column;
        }
    }
    return nullptr;
}

const ColumnLayout* Archetype::FindLayout(const ComponentId type) const
{
    for (const ColumnLayout& column : columns)
    {
        if (column.type == type)
        {
            return &column;
        }
    }
    return nullptr;
}
