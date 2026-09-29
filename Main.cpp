#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include "core/Window.h"
#include "ecs/World.h"
#include "utils/MemoryMonitor.h"

/*
 * Where the engine starts.
 *
 * The ECS checks used to live here. They moved to tests\EcsTests.cpp once this file had to
 * become a program rather than a test runner.
 *
 * Programs in this repository:
 *   GameEngines5        this file, the engine
 *   MemoryMonitorDemo   demo\MemoryMonitorDemo.cpp, every path the monitor watches
 *   MemoryMonitorTests  tests\MemoryMonitorTests.cpp, the monitor's checks
 *   EcsTests            tests\EcsTests.cpp, the ECS checks
 *
 * Both test programs are built and run by scripts\run-tests.bat.
 */

int main()
{
    /*
     * The runtime allocates before main, so measure against this point.
     *
     * This call used to do a second job. While the monitor was a static library, it was this
     * line that pulled its operator new and operator delete into the program, because a linker
     * only takes the parts of a library that something asks for. src/utils/MemoryMonitor.cpp is
     * compiled straight into this program now, so the replacements are always here and the
     * line is only what it looks like: a starting point to measure from.
     */
    const MemoryStats baseline = MemoryMonitor::GetStats();

    std::cout << "GameEngines5\n";

    /*
     * What an empty world costs to start: two reserves in the constructor, then the empty
     * archetype and its entities vector on the first CreateEntity. This is the number the
     * arena allocator is meant to bring down, so it is worth seeing on every run.
     */
    MemoryMonitor::SetLogging(true);

    World world;
    world.CreateEntity();

    MemoryMonitor::SetLogging(false);

    try
    {
        Window window("GameEngines5", 1280, 720);

        while (window.PollEvents())
        {
            /*
             * Nothing is drawn yet, so give the core back rather than spinning on an empty
             * event queue. The renderer replaces this with the frame it presents.
             */
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    MemoryMonitor::Report(baseline);
    MemoryMonitor::ReportLeaks(baseline);

    return 0;
}
