#include <exception>
#include <iostream>

#include "app/Application.h"
#include "utils/MemoryMonitor.h"
#include "utils/Profiler.h"

#include <crtdbg.h>
#include <cstdlib>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

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

namespace
{
    /**
     * When GE5_NO_DIALOGS is set, a failed assert, an abort or a crash prints to stderr and ends
     * the process, instead of opening a window that waits for a click. scripts\run-game.ps1 sets it;
     * a run from the IDE does not, so the IDE keeps the window and its Retry into the debugger.
     */
    void TurnOffErrorWindowsWhenAsked()
    {
        char value[2]{};
        if (GetEnvironmentVariableA("GE5_NO_DIALOGS", value, sizeof(value)) == 0) return;

        /* abort() opens no window and sends no crash report. */
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        /* assert() prints its line to stderr instead of opening a window. */
        _set_error_mode(_OUT_TO_STDERR);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        /* A real crash, such as an access violation, ends the process without the Windows error window. */
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    }
}


int main(const int argc, char* argv[])
{
    TurnOffErrorWindowsWhenAsked();

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
