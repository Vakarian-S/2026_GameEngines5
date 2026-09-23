#include <iostream>

#include "utils/MemoryMonitor.h"

/*
 * Where the engine starts.
 *
 * The MemoryMonitor walkthrough used to live in this file. It moved to
 * demo\MemoryMonitorDemo.cpp and builds as its own program, so this one is free for engine code.
 *
 * Programs in this solution:
 *   GameEngines5        this file, the engine
 *   MemoryMonitorDemo   demo\MemoryMonitorDemo.cpp, every path the monitor watches
 *   MemoryMonitorTests  tests\MemoryMonitorTests.cpp, the checks, run by scripts\run-tests.bat
 */

int main()
{
    /*
     * The runtime allocates before main, so measure against this point.
     *
     * This call does a second job. MemoryMonitor is a static library, and a linker only takes
     * the parts of one that something asks for. Touching it here is what pulls in its
     * operator new and operator delete, which is what puts the monitor in front of the
     * whole program.
     */
    const MemoryStats baseline = MemoryMonitor::GetStats();

    std::cout << "GameEngines5" << std::endl;

    MemoryMonitor::Report(baseline);
    MemoryMonitor::ReportLeaks(baseline);

    return 0;
}
