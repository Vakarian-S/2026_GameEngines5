#include <cstdio>
#include <format>
#include <iostream>
#include <Vector.h>
#include <vector>

#include "utils/MemoryMonitor.h"

// Run this in both x64 and Win32. The default new alignment is 16 on x64 and 8 on Win32,
// so Vec4 (alignas(16)) is the one type that changes path between the two builds:
// plain new on x64, aligned new on Win32.


// ============================== Test types ==============================

struct Tiny
{
    char c; // alignof 1
};

struct Plain4
{
    int values[4]; // alignof 4
};

struct Plain8
{
    double value; // alignof 8, the Win32 limit: 8 is not greater than 8, so still the plain path
};

struct alignas(16) Vec4
{
    float x, y, z, w; // alignof 16: plain path on x64, aligned path on Win32
};

struct alignas(32) Wide
{
    float values[8]; // alignof 32, like an AVX vector
};

struct alignas(64) CacheLine
{
    std::uint64_t counter; // alignof 64, one cache line, the false sharing trick
};

struct WithDtor
{
    int value;

    ~WithDtor() // a destructor makes new[] store an array cookie, so delete[] gets a size
    {
    }
};

struct alignas(64) AlignedWithDtor
{
    float values[3];

    ~AlignedWithDtor() // cookie and over-aligned at the same time
    {
    }
};


// ============================== Helpers ==============================

/**
 * Prints whether an address really is a multiple of the alignment, in the same columns as the operator log
 */
void CheckAlignment(const std::string_view what, const void* address, const std::size_t alignment)
{
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    const std::size_t remainder = value % alignment;
    const bool ok = remainder == 0;

    // format_to_n into a stack buffer and fputs, like LogOperation does.
    // std::format and std::cout both allocate, and those allocations would show up in the log and the counters.
    // The address prints as a void* with a plain {}: in a Debug build MSVC allocates 16 bytes for {:#x},
    // and for any other base or padded number, because those go through an internal temporary.
    char buffer[256];
    const char* const end = buffer + sizeof(buffer) - 1; // leave room for '\0'
    char* out = buffer;

    out = std::format_to_n(out, end - out, "{}Alignment: {:<7}{}",
                           ok ? MemColor::Ok : MemColor::Fail, ok ? "ok" : "FAIL", MemColor::Reset).out;
    out = std::format_to_n(out, end - out, "| {}{:<37}{}",
                           MemColor::Check, what, MemColor::Reset).out;
    out = std::format_to_n(out, end - out, "| address = {}{}{}",
                           MemColor::Value, address, MemColor::Reset).out;
    out = std::format_to_n(out, end - out, " | % {}{}{}",
                           MemColor::Value, alignment, MemColor::Reset).out;
    out = std::format_to_n(out, end - out, " = {}{}{}",
                           MemColor::Value, remainder, MemColor::Reset).out;
    *out = '\0';

    std::fputs(buffer, stdout);
    std::fputc('\n', stdout);
}

/**
 * Allocates one T and an array of T, checks both addresses, then frees them.
 * The log lines above each check say which operator the compiler picked.
 */
template <typename T>
void TestType(const char* name)
{
    const bool overAligned = alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    std::cout << MemColor::Section << "-- " << name
        << " | sizeof = " << sizeof(T)
        << " | alignof = " << alignof(T)
        << " | expected path: " << (overAligned ? "ALIGNED" : "plain")
        << MemColor::Reset << std::endl;

    T* one = new T;
    CheckAlignment("scalar", one, alignof(T));
    delete one;

    T* many = new T[4];
    CheckAlignment("array", many, alignof(T));
    delete[] many;

    std::cout << std::endl;
}


int main(int argc, char* argv[])
{
    // The runtime allocates before main, so measure against this point
    const MemoryStats baseline = MemoryMonitor::GetStats();
    MemoryMonitor::SetLogging(true);

    std::cout << MemColor::Section << "-- Build --" << MemColor::Reset << std::endl;
    std::cout << "pointer size                     = " << sizeof(void*) << " bytes" << std::endl;
    std::cout << "__STDCPP_DEFAULT_NEW_ALIGNMENT__ = " << __STDCPP_DEFAULT_NEW_ALIGNMENT__ << std::endl;
    std::cout << "anything above that goes to the align_val_t overloads" << std::endl;
    std::cout << std::endl;


    // ============================== Plain path in both builds ==============================

    TestType<Tiny>("Tiny, char");
    TestType<Plain4>("Plain4, four ints");
    TestType<Plain8>("Plain8, one double");
    TestType<WithDtor>("WithDtor, array cookie");


    // ============================== The build-dependent case ==============================

    // x64: alignof 16 is not greater than 16, so plain new, and the 16 byte Header keeps it aligned.
    // Win32: 16 is greater than 8, so the compiler calls new(size_t, align_val_t) instead.
    TestType<Vec4>("Vec4, alignas(16)  <-- switches path between builds");


    // ============================== Aligned path in both builds ==============================

    TestType<Wide>("Wide, alignas(32)");
    TestType<CacheLine>("CacheLine, alignas(64)");
    TestType<AlignedWithDtor>("AlignedWithDtor, alignas(64) + cookie");


    // ============================== Operators called by hand ==============================

    // The compiler prefers the sized deletes, so these are the only sure way to run the unsized ones
    std::cout << MemColor::Section << "-- Direct operator calls --" << MemColor::Reset << std::endl;

    void* rawOne = operator new(sizeof(Plain4));
    CheckAlignment("plain scalar", rawOne, alignof(Plain4));
    operator delete(rawOne); // delete(void*), unsized

    void* rawMany = operator new[](4 * sizeof(Plain4));
    CheckAlignment("plain array", rawMany, alignof(Plain4));
    operator delete[](rawMany, 4 * sizeof(Plain4)); // delete[](void*, size_t), sized

    constexpr std::align_val_t wideAlignment{alignof(Wide)};
    void* rawAlignedOne = operator new(sizeof(Wide), wideAlignment);
    CheckAlignment("aligned scalar", rawAlignedOne, alignof(Wide));
    operator delete(rawAlignedOne, wideAlignment); // delete(void*, align_val_t), unsized

    void* rawAlignedMany = operator new[](4 * sizeof(Wide), wideAlignment);
    CheckAlignment("aligned array", rawAlignedMany, alignof(Wide));
    operator delete[](rawAlignedMany, wideAlignment); // delete[](void*, align_val_t), unsized
    std::cout << std::endl;


    // ============================== A library container ==============================

    std::cout << MemColor::Section << "-- std::vector --" << MemColor::Reset << std::endl;
    {
        std::vector<MATH::Vec3> vertices;
        vertices.reserve(1000);
        for (int i = 0; i < 1000; i++)
        {
            vertices.push_back(MATH::Vec3());
        }
    } // vertices frees its memory here, so the report below shows only real leaks
    std::cout << std::endl;


    // ============================== Leak check ==============================

    // Uncomment to see the report catch a leak: one block that is never freed
    // int* leaked = new int(42);

    MemoryMonitor::Report(baseline);

    return 0;
}
