#include "utils/MemoryMonitor.h"

#include <cstddef>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void EnableConsoleColors()
{
    const HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (console == INVALID_HANDLE_VALUE || !GetConsoleMode(console, &mode))
    {
        return;
    }
    SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}


namespace
{
    /**
     * Stored in front of every block, used to access the number of bytes on delete
     */
    struct alignas(std::max_align_t) Header
    {
        std::size_t numBytes;
    };

    /**
     * Allocates a block with a Header in front and logs the operation
     * @param size bytes requested by the caller, not counting the Header
     * @param functionColor
     * @param function
     * @return pointer to the memory just past the Header
     */
    void* Allocate(const std::size_t size, const std::string_view functionColor, const std::string_view function)
    {
        LogOperation(MemColor::NewPrefix, "New", functionColor, function, size);
        auto* header = static_cast<Header*>(std::malloc(sizeof(Header) + size));
        if (!header) throw std::bad_alloc();
        header->numBytes = size;
        return header + 1;
    }

    /**
     * Frees a block made by Allocate, reads its size from the Header and logs the operation
     * @param block pointer returned by Allocate, can be nullptr
     * @param functionColor
     * @param function
     */
    void Free(void* block, const std::string_view functionColor, const std::string_view function) noexcept
    {
        if (!block) return;
        Header* header = static_cast<Header*>(block) - 1;
        LogOperation(MemColor::DeletePrefix, "Delete", functionColor, function, header->numBytes);
        std::free(header);
    }
}


void* operator new(std::size_t _Size)
{
    return Allocate(_Size, MemColor::Plain, "new(size_t)");
}

void* operator new [](std::size_t _Size)
{
    return Allocate(_Size, MemColor::PlainArray, "new[](size_t)");
}

void operator delete(void* _Block) noexcept
{
    Free(_Block, MemColor::Plain, "delete(void*)");
}

void operator delete(void* _Block, std::size_t _Size) noexcept
{
    Free(_Block, MemColor::Sized, "delete(void*, size_t)");
}

void operator delete [](void* _Block) noexcept
{
    Free(_Block, MemColor::PlainArray, "delete[](void*)");
}

void operator delete[](void* _Block, std::size_t _Size) noexcept
{
    Free(_Block, MemColor::SizedArray, "delete[](void*, size_t)");
}


// Aligned overloads: memory still comes from MSVC's default aligned new, so no Header yet.
void operator delete(void* _Block, std::align_val_t _Al) noexcept
{
    LogOperation(MemColor::DeletePrefix, "Delete", MemColor::Aligned, "delete(void*, align_val_t)",
                 std::nullopt, static_cast<std::size_t>(_Al));
    std::free(_Block);
}

void operator delete(void* _Block, std::size_t _Size,
                     std::align_val_t _Al) noexcept
{
    LogOperation(MemColor::DeletePrefix, "Delete", MemColor::SizedAligned, "delete(void*, size_t, align_val_t)",
                 _Size, static_cast<std::size_t>(_Al));
    std::free(_Block);
}

void operator delete[](void* _Block, std::align_val_t _Al) noexcept
{
    LogOperation(MemColor::DeletePrefix, "Delete", MemColor::AlignedArray, "delete[](void*, align_val_t)",
                 std::nullopt, static_cast<std::size_t>(_Al));
    std::free(_Block);
}

void operator delete[](void* _Block, std::size_t _Size,
                       std::align_val_t _Al) noexcept
{
    LogOperation(MemColor::DeletePrefix, "Delete", MemColor::SizedAlignedArray, "delete[](void*, size_t, align_val_t)",
                 _Size, static_cast<std::size_t>(_Al));
    std::free(_Block);
}
