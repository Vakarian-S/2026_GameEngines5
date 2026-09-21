/*
 * Tests for MemoryMonitor, built as their own program.
 *
 * They stay out of Main.cpp because several of them make real mistakes on purpose
 * (freeing twice, writing before a block). That output does not belong in the demo.
 *
 * This file includes only the public header, so it uses the monitor like any other code would.
 */

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <new>

#include "utils/MemoryMonitor.h"

namespace
{
    int checks = 0;
    int failures = 0;

    /* Plain const char*, not string_view: these go straight to printf's %s */
    constexpr const char* Pass = "\x1b[1;32m";
    constexpr const char* Fail = "\x1b[1;91m";
    constexpr const char* Dim = "\x1b[90m";
    constexpr const char* Reset = "\x1b[0m";

    void Expect(const bool ok, const char* what)
    {
        ++checks;
        if (!ok) ++failures;
        std::printf("  %s%-4s%s %s\n",
                    ok ? Pass : Fail, ok ? "ok" : "FAIL", Reset, what);
    }

    void Section(const char* name)
    {
        std::printf("\n%s-- %s --%s\n", Dim, name, Reset);
    }

    /**
     * Hides a pointer from the optimizer. Without it, the compiler could see the mistakes
     * below and remove them, or the code around them.
     */
#if defined(_MSC_VER)
    __declspec(noinline)
#endif
    void* Launder(void* p) { return p; }


    /* ============================== Correct use ============================== */

    void TestHeaderShape()
    {
        Section("header shape");

        /* Debug: the leak list fields round up to 64 on x64 and 32 on Win32, measured by probe */
        const std::size_t size = MemoryMonitor::HeaderOverhead();
        const std::size_t expected = MemoryMonitor::HasCheckedHeaders()
                                         ? (sizeof(void*) == 8 ? 64 : 32)
                                         : __STDCPP_DEFAULT_NEW_ALIGNMENT__;

        Expect(size == expected, "header is the expected size for this build");
        Expect((size & (size - 1)) == 0, "header size is a power of two");
        Expect(size >= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "header is at least the default alignment");
    }

    void TestCleanRoundTrip()
    {
        Section("correct code raises nothing");

        const MemoryStats before = MemoryMonitor::GetStats();
        delete new int(1);
        delete[] new int[8];
        const MemoryStats after = MemoryMonitor::GetStats();
        MemoryMonitor::Report(before);

        Expect(after.allocationCount == before.allocationCount + 2, "two allocations counted");
        Expect(after.freeCount == before.freeCount + 2, "two frees counted");
        Expect(after.badBlockCount == before.badBlockCount, "no block refused");
        Expect(after.mismatchCount == before.mismatchCount, "no operator mismatch");
        Expect(after.LiveBytes() == before.LiveBytes(), "nothing left live");
    }

    void TestAlignedRoundTrip()
    {
        Section("over-aligned types round trip");

        struct alignas(64) CacheLine { std::uint64_t counter; };

        const MemoryStats before = MemoryMonitor::GetStats();

        auto* one = new CacheLine;
        Expect(reinterpret_cast<std::uintptr_t>(one) % 64 == 0, "scalar is 64 byte aligned");
        delete one;

        auto* many = new CacheLine[4];
        Expect(reinterpret_cast<std::uintptr_t>(many) % 64 == 0, "array is 64 byte aligned");
        delete[] many;

        const MemoryStats after = MemoryMonitor::GetStats();
        MemoryMonitor::Report(before);
        Expect(after.badBlockCount == before.badBlockCount, "no block refused");
        Expect(after.mismatchCount == before.mismatchCount, "no operator mismatch");
        Expect(after.LiveBytes() == before.LiveBytes(), "nothing left live");
    }

    void TestSmallAlignmentRoundTrip()
    {
        Section("alignments at or below the header size");

        /*
         * Here the header size goes straight to _aligned_malloc. A header that is not a
         * power of two kills the process in this test, while the 64 byte test above still passes.
         */
        struct alignas(16) Align16 { std::uint64_t value; };
        struct alignas(32) Align32 { std::uint64_t value; };

        const MemoryStats before = MemoryMonitor::GetStats();

        auto* a = new Align16;
        Expect(reinterpret_cast<std::uintptr_t>(a) % 16 == 0, "16 byte type is 16 byte aligned");
        delete a;

        auto* b = new Align32[3];
        Expect(reinterpret_cast<std::uintptr_t>(b) % 32 == 0, "32 byte array is 32 byte aligned");
        delete[] b;

        const MemoryStats after = MemoryMonitor::GetStats();
        MemoryMonitor::Report(before);
        Expect(after.badBlockCount == before.badBlockCount, "no block refused");
        Expect(after.mismatchCount == before.mismatchCount, "no operator mismatch");
        Expect(after.LiveBytes() == before.LiveBytes(), "nothing left live");
    }


    /* ============================== Deliberate mistakes ============================== */

    bool SkipUnlessChecked()
    {
        if (MemoryMonitor::HasCheckedHeaders()) return false;
        std::printf("  %sskipped%s Release builds carry no check value\n", Dim, Reset);
        return true;
    }

    void TestDoubleFree()
    {
        Section("double free");
        if (SkipUnlessChecked()) return;

        /* The report covers the whole test */
        const MemoryStats start = MemoryMonitor::GetStats();
        int* p = new int(42);
        delete p;

        const MemoryStats before = MemoryMonitor::GetStats();
        /* The same pointer, a second time */
        delete static_cast<int*>(Launder(p));
        const MemoryStats after = MemoryMonitor::GetStats();
        MemoryMonitor::Report(start);

        Expect(after.badBlockCount == before.badBlockCount + 1, "reported as a refused free");
        Expect(after.freeCount == before.freeCount, "not counted as a real free");
        Expect(after.bytesFreed == before.bytesFreed, "bytesFreed did not move");
    }

    void TestUnderrunHitsMagic()
    {
        Section("small underrun, grid[-1]");
        if (SkipUnlessChecked()) return;

        /* Taken before the allocation, so the report shows the leak */
        const MemoryStats start = MemoryMonitor::GetStats();
        int* grid = new int[100];
        /* Lands on the last bytes of the header */
        static_cast<int*>(Launder(grid))[-1] = 0;

        const MemoryStats before = MemoryMonitor::GetStats();
        delete[] grid;
        const MemoryStats after = MemoryMonitor::GetStats();
        MemoryMonitor::Report(start);

        /* The block is refused, so this test leaks it on purpose. Refusing is the safe outcome. */
        Expect(after.badBlockCount == before.badBlockCount + 1, "caught at free");
        Expect(after.freeCount == before.freeCount, "not counted as a real free");
    }

    void TestNonHeapPointer()
    {
        Section("delete of something that never came from new");
        if (SkipUnlessChecked()) return;

        /*
         * A stack buffer, so reading a header in front of the pointer stays in bounds.
         * Zeroed, so the bytes where magic would sit cannot match by accident.
         */
        alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) unsigned char storage[256] = {};
        void* fake = storage + 128;

        const MemoryStats before = MemoryMonitor::GetStats();
        operator delete(Launder(fake));
        const MemoryStats after = MemoryMonitor::GetStats();
        MemoryMonitor::Report(before);

        Expect(after.badBlockCount == before.badBlockCount + 1, "reported as not ours");
        Expect(after.freeCount == before.freeCount, "not counted as a real free");
    }

    void TestWrongDeleteForm()
    {
        Section("aligned block freed by the plain delete");

        /* The report covers the whole test */
        const MemoryStats start = MemoryMonitor::GetStats();
        void* p = operator new(96, std::align_val_t{64});

        const MemoryStats before = MemoryMonitor::GetStats();
        /* Plain delete on an _aligned_malloc block */
        operator delete(Launder(p));
        const MemoryStats after = MemoryMonitor::GetStats();
        MemoryMonitor::Report(start);

        Expect(after.mismatchCount == before.mismatchCount + 1, "mismatch reported");
        Expect(after.freeCount == before.freeCount + 1, "block still freed, through the right path");
        Expect(after.badBlockCount == before.badBlockCount, "not treated as a bad block");
    }

    void TestWrongAlignmentValue()
    {
        Section("aligned block freed with the wrong alignment");

        /* The report covers the whole test */
        const MemoryStats start = MemoryMonitor::GetStats();
        void* p = operator new(96, std::align_val_t{64});

        const MemoryStats before = MemoryMonitor::GetStats();
        /* The caller says 16, the block says 64 */
        operator delete(Launder(p), std::align_val_t{16});
        const MemoryStats after = MemoryMonitor::GetStats();
        MemoryMonitor::Report(start);

        Expect(after.mismatchCount == before.mismatchCount + 1, "mismatch reported");
        Expect(after.freeCount == before.freeCount + 1, "header won, block freed correctly");
    }
}


int main()
{
    std::printf("%s-- MemoryMonitor tests --%s\n", Dim, Reset);
    std::printf("build: %s | header: %zu bytes | checked headers: %s\n",
                MemoryMonitor::HasCheckedHeaders() ? "Debug" : "Release",
                MemoryMonitor::HeaderOverhead(),
                MemoryMonitor::HasCheckedHeaders() ? "yes" : "no");

    /* The tests check counters, the log would only be noise */
    MemoryMonitor::SetLogging(false);

    TestHeaderShape();
    TestCleanRoundTrip();
    TestAlignedRoundTrip();
    TestSmallAlignmentRoundTrip();
    TestDoubleFree();
    TestUnderrunHitsMagic();
    TestNonHeapPointer();
    TestWrongDeleteForm();
    TestWrongAlignmentValue();

    std::printf("\n%s%d of %d checks passed%s\n",
                failures ? Fail : Pass, checks - failures, checks, Reset);
    return failures == 0 ? 0 : 1;
}
