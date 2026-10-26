#include <exception>
#include <iostream>

#include "app/Application.h"
#include "utils/MemoryMonitor.h"
#include "utils/Profiler.h"

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
 *   ArenaTests          tests\ArenaTests.cpp, the arena's checks
 *   ArenaDemo           demo\ArenaDemo.cpp, the heap and the arena side by side
 *
 * Both test programs are built and run by scripts\run-tests.bat.
 */


int main(const int argc, char* argv[])
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
    try
    {
        Application app(argc, argv);
        app.Run();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    catch (const std::string& error)
    {
        std::cerr << "[ERROR] math library: " << error << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "[ERROR] unknown exception" << std::endl;
        return 1;
    }

    Profiler::Report();
    MemoryMonitor::Report(baseline);
    MemoryMonitor::ReportLeaks(baseline);

    return 0;
}
