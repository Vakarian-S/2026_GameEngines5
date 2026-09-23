#include <cstdio>
#include <format>
#include <iostream>
#include <Vector.h>
#include <vector>

#include "utils/MemoryMonitor.h"

/*
 * A walk through every path the monitor watches, built as its own program.
 *
 * It used to be Main.cpp. It moved here so the engine's entry point is free for engine code,
 * the same way the tests live in tests\MemoryMonitorTests.cpp.
 *
 * Nothing here is a test. It allocates correctly throughout, leaks two blocks on purpose at
 * the end, and prints what the monitor saw. The deliberate mistakes are in the tests.
 */

/*
 * Run this in both x64 and Win32. The default new alignment is 16 on x64 and 8 on Win32,
 * so Vec4 (alignas(16)) is the one type that changes path between the two builds:
 * plain new on x64, aligned new on Win32.
 */


/* ============================== Test types ============================== */

struct Tiny
{
    /** alignof 1 */
    char c;
};

struct Plain4
{
    /** alignof 4 */
    int values[4];
};

struct Plain8
{
    /** alignof 8, the Win32 limit: 8 is not greater than 8, so still the plain path */
    double value;
};

struct alignas(16) Vec4
{
    /** alignof 16: plain path on x64, aligned path on Win32 */
    float x, y, z, w;
};

struct alignas(32) Wide
{
    /** alignof 32, like an AVX vector */
    float values[8];
};

struct alignas(64) CacheLine
{
    /** alignof 64, one cache line, the false sharing trick */
    std::uint64_t counter;
};

struct WithDtor
{
    int value;

    /** A destructor makes new[] store an array cookie, so delete[] gets a size */
    ~WithDtor()
    {
    }
};

struct alignas(64) AlignedWithDtor
{
    float values[3];

    /** Cookie and over-aligned at the same time */
    ~AlignedWithDtor()
    {
    }
};


/* ============================== Helpers ============================== */

/**
 * Prints whether an address really is a multiple of the alignment, in the same columns as the operator log
 */
void CheckAlignment(const std::string_view what, const void* address, const std::size_t alignment)
{
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    const std::size_t remainder = value % alignment;
    const bool ok = remainder == 0;

    /*
     * format_to_n into a stack buffer and fputs, like LogOperation does.
     * std::format and std::cout both allocate, and those allocations would show up in the log and the counters.
     * The address prints as a void* with a plain {}: in a Debug build MSVC allocates 16 bytes for {:#x},
     * and for any other base or padded number, because those go through an internal temporary.
     */
    char buffer[256];
    /* Leave room for '\0' */
    const char* const end = buffer + sizeof(buffer) - 1;
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
 * Prints where a block was made, in the same columns as the operator log.
 * Uses the stack buffer pattern from CheckAlignment, so it allocates nothing.
 * @param what the name of the call that made the block
 * @param block pointer from new
 */
void PrintSite(const std::string_view what, const void* block)
{
    const AllocationSite site = MemoryMonitor::GetAllocationSite(block);

    char buffer[256];
    /* Leave room for '\n' and '\0' */
    const char* const end = buffer + sizeof(buffer) - 2;
    char* out = buffer;

    out = std::format_to_n(out, end - out, "{}Made at:   {:<7}{}| {}{:<37}{}",
                           MemColor::Check, "site", MemColor::Reset,
                           MemColor::Check, what, MemColor::Reset).out;

    if (site.file)
    {
        /* Only the file name: a full path can be long enough to cut the line off */
        const std::string_view path = site.file;
        const std::size_t slash = path.find_last_of("/\\");
        const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
        out = std::format_to_n(out, end - out, "| {}{}:{}{}",
                               MemColor::Value, name, site.line, MemColor::Reset).out;
    }
    else
    {
        out = std::format_to_n(out, end - out, "| {}(unknown){}", MemColor::Section, MemColor::Reset).out;
    }
    *out++ = '\n';
    *out = '\0';

    std::fputs(buffer, stdout);
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
    /* The runtime allocates before main, so measure against this point */
    const MemoryStats baseline = MemoryMonitor::GetStats();
    MemoryMonitor::SetLogging(true);

    std::cout << MemColor::Section << "-- Build --" << MemColor::Reset << std::endl;
    std::cout << "pointer size                     = " << sizeof(void*) << " bytes" << std::endl;
    std::cout << "__STDCPP_DEFAULT_NEW_ALIGNMENT__ = " << __STDCPP_DEFAULT_NEW_ALIGNMENT__ << std::endl;
    std::cout << "anything above that goes to the align_val_t overloads" << std::endl;
    std::cout << std::endl;


    /* ============================== Plain path in both builds ============================== */

    TestType<Tiny>("Tiny, char");
    TestType<Plain4>("Plain4, four ints");
    TestType<Plain8>("Plain8, one double");
    TestType<WithDtor>("WithDtor, array cookie");


    /* ============================== The build-dependent case ============================== */

    /*
     * x64: alignof 16 is not greater than 16, so plain new, and the 16 byte Header keeps it aligned.
     * Win32: 16 is greater than 8, so the compiler calls new(size_t, align_val_t) instead.
     */
    TestType<Vec4>("Vec4, alignas(16)  <-- switches path between builds");


    /* ============================== Aligned path in both builds ============================== */

    TestType<Wide>("Wide, alignas(32)");
    TestType<CacheLine>("CacheLine, alignas(64)");
    TestType<AlignedWithDtor>("AlignedWithDtor, alignas(64) + cookie");


    /* ============================== Operators called by hand ============================== */

    /* The compiler prefers the sized deletes, so these are the only sure way to run the unsized ones */
    std::cout << MemColor::Section << "-- Direct operator calls --" << MemColor::Reset << std::endl;

    void* rawOne = operator new(sizeof(Plain4));
    CheckAlignment("plain scalar", rawOne, alignof(Plain4));
    /* delete(void*), unsized */
    operator delete(rawOne);

    void* rawMany = operator new[](4 * sizeof(Plain4));
    CheckAlignment("plain array", rawMany, alignof(Plain4));
    /* delete[](void*, size_t), sized */
    operator delete[](rawMany, 4 * sizeof(Plain4));

    constexpr std::align_val_t wideAlignment{alignof(Wide)};
    void* rawAlignedOne = operator new(sizeof(Wide), wideAlignment);
    CheckAlignment("aligned scalar", rawAlignedOne, alignof(Wide));
    /* delete(void*, align_val_t), unsized */
    operator delete(rawAlignedOne, wideAlignment);

    void* rawAlignedMany = operator new[](4 * sizeof(Wide), wideAlignment);
    CheckAlignment("aligned array", rawAlignedMany, alignof(Wide));
    /* delete[](void*, align_val_t), unsized */
    operator delete[](rawAlignedMany, wideAlignment);
    std::cout << std::endl;


    /* ============================== A library container ============================== */

    std::cout << MemColor::Section << "-- std::vector --" << MemColor::Reset << std::endl;
    {
        std::vector<MATH::Vec3> vertices;
        vertices.reserve(1000);
        for (int i = 0; i < 1000; i++)
        {
            vertices.push_back(MATH::Vec3());
        }
    }
    /* vertices freed its memory at the closing brace above, so the report below shows only real leaks */
    std::cout << std::endl;


    /* ============================== Smart pointers ============================== */

    /*
     * Smart pointers call the same operators, so every block they make is counted here too.
     * std::make_unique writes its new inside the standard library, so its blocks say (unknown).
     * MEMMON_MAKE_UNIQUE makes the same object through our file-tagged new instead.
     * Both give the memory back at the closing brace, so neither shows up in the leak report.
     */
    std::cout << MemColor::Section << "-- Smart pointers --" << MemColor::Reset << std::endl;
    {
        const std::unique_ptr<Plain4> plain = std::make_unique<Plain4>();
        const std::unique_ptr<Plain4> tagged = MEMMON_MAKE_UNIQUE(Plain4);

        PrintSite("std::make_unique<Plain4>()", plain.get());
        PrintSite("MEMMON_MAKE_UNIQUE(Plain4)", tagged.get());
    }
    std::cout << std::endl;


    /* ============================== Leak check ============================== */

    /*
     * Two blocks that are never freed, on purpose, so the leak report has something to show.
     * One is made with MEMMON_NEW, so the report names this file and line.
     * One is a plain new of an aligned type, so the report shows "(unknown)" and its alignment.
     */
    [[maybe_unused]] int* leakedTagged = MEMMON_NEW int(42);
    [[maybe_unused]] auto* leakedPlain = new CacheLine;

    MemoryMonitor::Report(baseline);
    MemoryMonitor::ReportLeaks(baseline);


    /* ============================== Damaged leak list ============================== */

    /*
     * Debug only: Release blocks carry no stamp to break.
     * We overwrite the stamp of one leaked block, like a small underrun would, show that the
     * leak report stops there safely, then put the stamp back so the program stays healthy.
     */
    if (MemoryMonitor::HasCheckedHeaders())
    {
        std::cout << MemColor::Section << "-- Damaged leak list --" << MemColor::Reset << std::endl;

        /* volatile stops the optimizer from reasoning about the deliberate write below */
        int* volatile target = leakedTagged;
        const int savedStamp = target[-1];
        target[-1] = 0;

        MemoryMonitor::ReportLeaks(baseline);

        target[-1] = savedStamp;
    }

    return 0;
}
