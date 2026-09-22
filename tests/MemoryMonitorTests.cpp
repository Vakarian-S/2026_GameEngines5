/*
 * Tests for MemoryMonitor, built as their own program.
 *
 * They stay out of Main.cpp because several of them make real mistakes on purpose
 * (freeing twice, writing before a block). That output does not belong in the demo.
 *
 * This file includes only the public header, so it uses the monitor like any other code would.
 */

#include <atomic>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <thread>


#include "utils/MemoryMonitor.h"

namespace
{
    /** A type whose constructor always fails */
    struct Explodes
    {
        Explodes() { throw 42; }
    };

    /** The same, for the aligned new and delete */
    struct alignas(64) ExplodesAligned
    {
        ExplodesAligned() { throw 42; }
    };

    int checks = 0;
    int failures = 0;

    /* Plain const char*, not string_view: these go straight to printf's %s */
    constexpr const char* Pass = "\x1b[1;32m";
    constexpr const char* Fail = "\x1b[1;91m";
    constexpr const char* Dim = "\x1b[90m";
    /** Bold bright cyan, for the test case headers */
    constexpr const char* Title = "\x1b[1;96m";
    constexpr const char* Reset = "\x1b[0m";

    void Expect(const bool ok, const char* what)
    {
        ++checks;
        if (!ok) ++failures;
        std::printf("  %s%-4s%s %s\n",
                    ok ? Pass : Fail, ok ? "ok" : "FAIL", Reset, what);
    }

    /**
     * Prints the header that starts each test case, so they stand out from the reports
     * @param name
     * @param description what the test checks, in plain words
     */
    void Section(const char* name, const char* description)
    {
        std::printf("\n%s==================== TEST CASE: %s ====================%s\n", Title, name, Reset);
        std::printf("%s\n", description);
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
        Section("header shape",
                "We check that the hidden label in front of every block has the size we planned.");

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
        Section("correct code raises nothing",
                "We make and delete memory the normal way, and check that nothing gets flagged.");

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
        Section("over-aligned types round trip",
                "We make objects that must start on a 64 byte boundary, and check that they do.");

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
        Section("alignments at or below the header size",
                "Same idea with 16 and 32 byte boundaries. A label of the wrong size would crash here.");

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


    bool SkipUnlessChecked()
    {
        if (MemoryMonitor::HasCheckedHeaders()) return false;
        std::printf("  %sskipped%s Release builds carry no check value\n", Dim, Reset);
        return true;
    }


    /* ============================== Leak list ============================== */

    void TestLeakListing()
    {
        Section("leak list",
                "We make 3 blocks and delete 1, and check that the leak report lists the other 2.");
        if (SkipUnlessChecked()) return;

        struct alignas(64) CacheLine { std::uint64_t counter; };

        /*
         * The runtime already has live blocks from before main. Listing exactly 2 below also
         * proves the baseline keeps those out.
         */
        const MemoryStats before = MemoryMonitor::GetStats();

        /* Plain and aligned blocks both join the list. It now reads: c, b, a, older blocks */
        int* a = new int(1);
        char* b = new char[20];
        auto* c = new CacheLine;

        /* b sits in the middle, so its neighbors must be hooked together */
        delete[] b;

        const MemoryStats beforeReport = MemoryMonitor::GetStats();
        const LeakSummary two = MemoryMonitor::ReportLeaks(before);
        const MemoryStats afterReport = MemoryMonitor::GetStats();

        Expect(two.listed == 2, "the two live blocks are listed");
        Expect(two.bytes == sizeof(int) + sizeof(CacheLine), "their bytes add up");
        Expect(!two.damaged, "the walk finished");
        Expect(afterReport.allocationCount == beforeReport.allocationCount, "the report allocates nothing");

        /* a is the oldest of the three, then c is the front of the list */
        delete a;
        delete c;

        const LeakSummary none = MemoryMonitor::ReportLeaks(before);
        Expect(none.listed == 0, "nothing listed once everything is freed");
        Expect(none.bytes == 0, "no bytes listed either");
    }

    /**
     * True if the block was made in this file, on the given line
     * @param block
     * @param line
     */
    bool CameFrom(const void* block, const std::uint32_t line)
    {
        const AllocationSite site = MemoryMonitor::GetAllocationSite(block);
        return site.file != nullptr
            && std::string_view(site.file).ends_with("MemoryMonitorTests.cpp")
            && site.line == line;
    }

    void TestLeakFileNames()
    {
        Section("leak list with file names",
                "We make blocks with MEMMON_NEW and check that the leak report "
                "says which file and line made each one.");
        if (SkipUnlessChecked()) return;

        struct alignas(64) CacheLine { std::uint64_t counter; };

        const MemoryStats before = MemoryMonitor::GetStats();

        /* __LINE__ on the same line as the new gives the same number the header stored */
        int* a = MEMMON_NEW int(1);     const std::uint32_t lineA = __LINE__;
        char* b = MEMMON_NEW char[20];  const std::uint32_t lineB = __LINE__;
        auto* c = MEMMON_NEW CacheLine; const std::uint32_t lineC = __LINE__;
        int* d = new int(2);

        Expect(CameFrom(a, lineA), "int: right file and line");
        Expect(CameFrom(b, lineB), "char array: right file and line");
        Expect(CameFrom(c, lineC), "aligned object: right file and line");
        Expect(reinterpret_cast<std::uintptr_t>(c) % 64 == 0, "the file-tagged aligned object is 64 byte aligned");
        Expect(MemoryMonitor::GetAllocationSite(d).file == nullptr, "a plain new has no file");

        const LeakSummary four = MemoryMonitor::ReportLeaks(before);
        Expect(four.listed == 4, "all 4 are listed");

        delete a;
        delete[] b;
        delete c;
        delete d;
    }

    void TestTaggedMakeUnique()
    {
        Section("MEMMON_MAKE_UNIQUE",
                "We make a smart pointer with the tagged macro, and check that the object is right, "
                "that the block knows this file and line, and that it is freed at the closing brace.");

        const MemoryStats before = MemoryMonitor::GetStats();
        {
            /* __LINE__ on the same line as the macro gives the number the header stored */
            const std::unique_ptr<int> p = MEMMON_MAKE_UNIQUE(int, 5); const std::uint32_t line = __LINE__;

            Expect(*p == 5, "the object holds the value we passed");

            if (MemoryMonitor::HasCheckedHeaders()) Expect(CameFrom(p.get(), line), "right file and line");
            else std::printf("  %sskipped%s Release has no file and line\n", Dim, Reset);
        }
        const MemoryStats after = MemoryMonitor::GetStats();

        Expect(after.allocationCount == before.allocationCount + 1, "one block was made");
        Expect(after.freeCount == before.freeCount + 1, "and given back when the pointer went away");
    }

    void TestDamagedLeakList()
    {
        Section("damaged leak list",
                "We break one block's label on purpose in the middle of the list, "
                "and check the leak report stops there safely instead of crashing.");
        if (SkipUnlessChecked()) return;

        const MemoryStats before = MemoryMonitor::GetStats();

        /* The list now reads: c, b, a, older blocks */
        int* a = new int(1);
        int* b = new int(2);
        int* c = new int(3);

        /* b[-1] is the last 4 bytes of b's header: its stamp. Save it so we can repair it. */
        int* hidden = static_cast<int*>(Launder(b));
        const int savedStamp = hidden[-1];
        hidden[-1] = 0;

        const MemoryStats beforeReport = MemoryMonitor::GetStats();
        const LeakSummary broken = MemoryMonitor::ReportLeaks(before);
        const MemoryStats afterReport = MemoryMonitor::GetStats();

        Expect(broken.damaged, "the damage is reported");
        Expect(broken.listed == 1, "only c, in front of the damage, is listed");
        Expect(broken.bytes == sizeof(int), "and only its bytes");
        Expect(afterReport.allocationCount == beforeReport.allocationCount, "the report allocates nothing");

        /* Put the stamp back. If the walk changed nothing, the full list comes back. */
        hidden[-1] = savedStamp;

        const LeakSummary repaired = MemoryMonitor::ReportLeaks(before);
        Expect(!repaired.damaged, "after the repair the walk finishes");
        Expect(repaired.listed == 3, "and lists all 3 blocks");

        delete a;
        delete b;
        delete c;

        const LeakSummary none = MemoryMonitor::ReportLeaks(before);
        Expect(none.listed == 0, "nothing listed once everything is freed");
    }
    
    /* ============================== Threads ============================== */

    /** Cache line sized, so the threads run the aligned path too */
    struct alignas(64) ThreadLine { std::uint64_t counter; };

    constexpr std::size_t ThreadCount = 4;
    constexpr std::size_t RoundsPerThread = 10000;
    /** How many times each thread test repeats its whole workload, so one run tries several times */
    constexpr std::size_t ThreadTestRounds = 3;

    /**
     * The two gates every thread test uses.
     *
     * std::thread allocates its own start data in main and frees it inside the thread once the
     * work returns. The gates keep both of those outside the window we measure: main takes its
     * baseline only after every worker is waiting at the start gate, and the workers wait at the
     * end gate until main has taken its last reading.
     */
    struct ThreadGate
    {
        std::atomic<std::size_t> atStart{0};
        std::atomic<bool> go{false};
        std::atomic<std::size_t> atEnd{0};
        std::atomic<bool> finish{false};

        /** A worker waits here until main says go */
        void WorkerStart()
        {
            ++atStart;
            while (!go.load()) std::this_thread::yield();
        }

        /** A worker waits here until main is done reading */
        void WorkerEnd()
        {
            ++atEnd;
            while (!finish.load()) std::this_thread::yield();
        }

        /** Main waits until every worker is at the start gate */
        void WaitForStart()
        {
            while (atStart.load() < ThreadCount) std::this_thread::yield();
        }

        /** Main waits until every worker has finished its work */
        void WaitForEnd()
        {
            while (atEnd.load() < ThreadCount) std::this_thread::yield();
        }
    };

    /** Bytes each round makes: an int, a ThreadLine and a char[20] */
    constexpr std::size_t RoundBytes = sizeof(int) + sizeof(ThreadLine) + 20;
    /** Bytes each thread keeps at the end, for the leak list: one int and one ThreadLine */
    constexpr std::size_t KeptBytes = sizeof(int) + sizeof(ThreadLine);

    /** The blocks one thread keeps. Main frees them after the report. */
    struct Kept
    {
        int* plain = nullptr;
        ThreadLine* aligned = nullptr;
    };

    void TestThreadedLeakList()
    {
        Section("threads share the counters and the leak list",
                "4 threads make and delete blocks at the same time, 3 times over. We check that no "
                "count is lost and that the leak list shows exactly the blocks they kept.");

        /* One counter per check, raised by any round that gets it wrong */
        std::size_t lostAllocations = 0;
        std::size_t lostFrees = 0;
        std::size_t lostAllocatedBytes = 0;
        std::size_t lostFreedBytes = 0;
        std::size_t flaggedFrees = 0;
        std::size_t damagedRounds = 0;
        std::size_t wrongBlocks = 0;
        std::size_t wrongBytes = 0;

        for (std::size_t round = 0; round < ThreadTestRounds; ++round)
        {
            ThreadGate gate;
            Kept kept[ThreadCount];

            auto work = [&gate](Kept& mine)
            {
                gate.WorkerStart();

                for (std::size_t step = 0; step < RoundsPerThread; ++step)
                {
                    int* a = new int(1);
                    auto* b = new ThreadLine;
                    char* c = new char[20];

                    /* Middle, oldest, then newest: every unlink case, while the other threads work too */
                    delete b;
                    delete a;
                    delete[] c;
                }

                mine.plain = new int(2);
                mine.aligned = new ThreadLine;

                gate.WorkerEnd();
            };

            /* std::thread allocates its start data here, in main, before we take the baseline */
            std::thread threads[ThreadCount];
            for (std::size_t i = 0; i < ThreadCount; ++i)
            {
                threads[i] = std::thread([&work, &kept, i] { work(kept[i]); });
            }

            gate.WaitForStart();
            const MemoryStats before = MemoryMonitor::GetStats();
            gate.go = true;

            gate.WaitForEnd();
            const MemoryStats after = MemoryMonitor::GetStats();
            const LeakSummary leaks = MemoryMonitor::ReportLeaks(before);

            /* Each thread frees its start data after this, outside our measurement */
            gate.finish = true;
            for (std::thread& t : threads) t.join();

            constexpr std::size_t madeInRounds = ThreadCount * RoundsPerThread * 3;
            if (after.allocationCount - before.allocationCount != madeInRounds + 2 * ThreadCount) ++lostAllocations;
            if (after.freeCount - before.freeCount != madeInRounds) ++lostFrees;
            constexpr std::size_t bytesInRounds = ThreadCount * (RoundsPerThread * RoundBytes + KeptBytes);
            if (after.bytesAllocated - before.bytesAllocated != bytesInRounds) ++lostAllocatedBytes;
            if (after.bytesFreed - before.bytesFreed != ThreadCount * RoundsPerThread * RoundBytes) ++lostFreedBytes;
            if (after.badBlockCount != before.badBlockCount || after.mismatchCount != before.mismatchCount)
            {
                ++flaggedFrees;
            }

            /* Release has no leak list, the counters above are all it can check */
            if (MemoryMonitor::HasCheckedHeaders())
            {
                if (leaks.damaged) ++damagedRounds;
                if (leaks.listed != 2 * ThreadCount) ++wrongBlocks;
                if (leaks.bytes != ThreadCount * KeptBytes) ++wrongBytes;
            }

            for (const Kept& k : kept)
            {
                delete k.plain;
                delete k.aligned;
            }
        }

        Expect(lostAllocations == 0, "no allocation lost, in every round");
        Expect(lostFrees == 0, "no free lost, in every round");
        Expect(lostAllocatedBytes == 0, "no allocated byte lost, in every round");
        Expect(lostFreedBytes == 0, "no freed byte lost, in every round");
        Expect(flaggedFrees == 0, "no free refused or mismatched, in every round");

        if (MemoryMonitor::HasCheckedHeaders())
        {
            Expect(damagedRounds == 0, "the leak list is never damaged");
            Expect(wrongBlocks == 0, "every round lists exactly the kept blocks");
            Expect(wrongBytes == 0, "and their bytes");
        }
    }

    /** Every block these threads make has this size, so live bytes stay a multiple of it */
    constexpr std::size_t SnapshotBlockBytes = 32;
    /** How many readings main checks per round, while the threads run */
    constexpr std::size_t SnapshotPolls = 40000;
    /** How many leak reports main asks for per round, while the threads run */
    constexpr std::size_t ReportsWhileWorking = 10;

    void TestStatsSnapshot()
    {
        Section("counter readings while threads run",
                "4 threads make and delete blocks while main reads the counters 40000 times, "
                "3 times over. We check that no reading is impossible or half updated.");

        std::size_t impossibleCounts = 0;
        std::size_t impossibleBytes = 0;
        std::size_t halfUpdated = 0;

        for (std::size_t round = 0; round < ThreadTestRounds; ++round)
        {
            ThreadGate gate;
            std::atomic<bool> stop{false};

            auto work = [&gate, &stop]
            {
                gate.WorkerStart();

                while (!stop.load())
                {
                    char* block = new char[SnapshotBlockBytes];
                    delete[] block;
                }

                gate.WorkerEnd();
            };

            std::thread threads[ThreadCount];
            for (std::thread& t : threads) t = std::thread(work);

            gate.WaitForStart();
            /* Nothing is in flight yet, so this reading is exact */
            const MemoryStats before = MemoryMonitor::GetStats();
            gate.go = true;

            for (std::size_t poll = 0; poll < SnapshotPolls; ++poll)
            {
                const MemoryStats now = MemoryMonitor::GetStats();

                if (now.freeCount > now.allocationCount) ++impossibleCounts;
                if (now.bytesFreed > now.bytesAllocated) ++impossibleBytes;

                /* Every block made since the baseline is the same size, so the two deltas must agree */
                const std::int64_t blocks = static_cast<std::int64_t>(now.LiveBlocks())
                    - static_cast<std::int64_t>(before.LiveBlocks());
                const std::int64_t bytes = static_cast<std::int64_t>(now.LiveBytes())
                    - static_cast<std::int64_t>(before.LiveBytes());
                if (bytes != blocks * static_cast<std::int64_t>(SnapshotBlockBytes)) ++halfUpdated;
            }

            stop = true;
            gate.WaitForEnd();
            gate.finish = true;
            for (std::thread& t : threads) t.join();
        }

        Expect(impossibleCounts == 0, "no reading had more frees than allocations");
        Expect(impossibleBytes == 0, "no reading had more bytes freed than allocated");
        Expect(halfUpdated == 0, "live blocks and live bytes always agree");
    }

    void TestThreadedReportLeaks()
    {
        Section("leak report while threads work",
                "4 threads make and delete blocks while main asks for the leak report. "
                "We check that every report is whole and only lists blocks that are really live.");
        if (SkipUnlessChecked()) return;

        std::size_t damagedReports = 0;
        std::size_t tooManyBlocks = 0;
        std::size_t wrongBytes = 0;
        std::size_t leftOver = 0;

        for (std::size_t round = 0; round < ThreadTestRounds; ++round)
        {
            ThreadGate gate;
            std::atomic<bool> stop{false};

            auto work = [&gate, &stop]
            {
                gate.WorkerStart();

                while (!stop.load())
                {
                    char* block = new char[SnapshotBlockBytes];
                    delete[] block;
                }

                gate.WorkerEnd();
            };

            std::thread threads[ThreadCount];
            for (std::thread& t : threads) t = std::thread(work);

            gate.WaitForStart();
            const MemoryStats before = MemoryMonitor::GetStats();
            gate.go = true;

            for (std::size_t report = 0; report < ReportsWhileWorking; ++report)
            {
                const LeakSummary now = MemoryMonitor::ReportLeaks(before);

                if (now.damaged) ++damagedReports;
                /* Each thread holds at most one block at a time, so the list cannot be longer */
                if (now.listed > ThreadCount) ++tooManyBlocks;
                if (now.bytes != now.listed * SnapshotBlockBytes) ++wrongBytes;
            }

            stop = true;
            gate.WaitForEnd();

            /* The threads are parked now, so nothing of theirs is still live */
            if (MemoryMonitor::ReportLeaks(before).listed != 0) ++leftOver;

            gate.finish = true;
            for (std::thread& t : threads) t.join();
        }

        Expect(damagedReports == 0, "no report hit a damaged block");
        Expect(tooManyBlocks == 0, "no report listed more blocks than the threads can hold");
        Expect(wrongBytes == 0, "every report's bytes match the blocks it listed");
        Expect(leftOver == 0, "nothing is left live once the threads stop");
    }

    void TestThreadedDoubleFree()
    {
        Section("threads free the same block at once",
                "4 threads delete the very same block at the same time. We check that exactly one "
                "delete goes through and the other three are refused.");
        if (SkipUnlessChecked()) return;

        std::size_t wrongFreeCount = 0;
        std::size_t wrongRefusedCount = 0;
        std::size_t wrongBytesFreed = 0;

        for (std::size_t round = 0; round < ThreadTestRounds; ++round)
        {
            ThreadGate gate;

            /*
             * The losing threads read the header of memory that was just freed. Nothing else
             * allocates in this window, so the runtime does not hand that memory to anyone else.
             */
            int* shared = new int(7);

            auto work = [&gate, shared]
            {
                gate.WorkerStart();
                delete static_cast<int*>(Launder(shared));
                gate.WorkerEnd();
            };

            std::thread threads[ThreadCount];
            for (std::thread& t : threads) t = std::thread(work);

            gate.WaitForStart();
            const MemoryStats before = MemoryMonitor::GetStats();
            gate.go = true;

            gate.WaitForEnd();
            const MemoryStats after = MemoryMonitor::GetStats();

            gate.finish = true;
            for (std::thread& t : threads) t.join();

            if (after.freeCount - before.freeCount != 1) ++wrongFreeCount;
            if (after.badBlockCount - before.badBlockCount != ThreadCount - 1) ++wrongRefusedCount;
            if (after.bytesFreed - before.bytesFreed != sizeof(int)) ++wrongBytesFreed;
        }

        Expect(wrongFreeCount == 0, "exactly one delete freed the block, in every round");
        Expect(wrongRefusedCount == 0, "the other three were refused, in every round");
        Expect(wrongBytesFreed == 0, "the block's bytes were given back once");
    }


    /* ============================== File-tagged new ============================== */

    /**
     * Makes a T with MEMMON_NEW, whose constructor throws, and checks that its memory was given back.
     * In Debug this goes through the file-tagged delete. In Release MEMMON_NEW is a plain new,
     * so the normal delete does the job and the checks still hold.
     * @param what the type's name, printed above its checks
     */
    template <typename T>
    void CheckThrowFreesMemory(const char* what)
    {
        const MemoryStats before = MemoryMonitor::GetStats();
        bool caught = false;

        try
        {
            [[maybe_unused]] T* never = MEMMON_NEW T;
        }
        catch (int)
        {
            caught = true;
        }

        const MemoryStats after = MemoryMonitor::GetStats();

        std::printf("  %s%s%s\n", Dim, what, Reset);
        Expect(caught, "the exception reached us");
        Expect(after.allocationCount == before.allocationCount + 1, "one block was made");
        Expect(after.freeCount == before.freeCount + 1, "and given back");
        Expect(after.LiveBytes() == before.LiveBytes(), "nothing left live");
    }

    void TestConstructorThrows()
    {
        Section("constructor throws",
                "We make an object whose constructor fails, and check that its memory is still given back.");

        CheckThrowFreesMemory<Explodes>("plain object");
        CheckThrowFreesMemory<ExplodesAligned>("64 byte aligned object");
    }


    /* ============================== Deliberate mistakes ============================== */

    void TestDoubleFree()
    {
        Section("double free",
                "We delete the same memory twice on purpose, and check that the second delete is refused.");
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
        Section("small underrun, grid[-1]",
                "We write one step before a block on purpose, and check that delete notices the damage.");
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

        /* The refused block stays in the list with its broken stamp, so the leak walk stops on it */
        const LeakSummary leaks = MemoryMonitor::ReportLeaks(start);
        Expect(leaks.damaged, "the leak report stops at the damaged block");
        Expect(leaks.listed == 0, "nothing is listed past it");
    }

    void TestNonHeapPointer()
    {
        Section("delete of something that never came from new",
                "We delete memory that new never gave out, and check that the monitor refuses it.");
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
        Section("aligned block freed by the plain delete",
                "We free special aligned memory with the normal delete, and check it still goes back the right way.");

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
        Section("aligned block freed with the wrong alignment",
                "We free aligned memory while passing the wrong alignment, "
                "and check it still goes back the right way.");

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
    /* Before the mistakes: TestUnderrunHitsMagic leaves a damaged block in the list for good */
    TestLeakListing();
    TestLeakFileNames();
    TestTaggedMakeUnique();
    TestDamagedLeakList();
    TestThreadedLeakList();
    TestStatsSnapshot();
    TestThreadedReportLeaks();
    TestThreadedDoubleFree();
    TestConstructorThrows();
    TestDoubleFree();
    TestUnderrunHitsMagic();
    TestNonHeapPointer();
    TestWrongDeleteForm();
    TestWrongAlignmentValue();

    std::printf("\n%s%d of %d checks passed%s\n",
                failures ? Fail : Pass, checks - failures, checks, Reset);
    return failures == 0 ? 0 : 1;
}
