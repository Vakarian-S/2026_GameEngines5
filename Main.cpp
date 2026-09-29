#include <exception>
#include <iostream>
#include <SDL3/SDL.h>


#include "core/Timer.h"
#include "core/Window.h"
#include "core/Scene.h"
#include "scenes/Scene0.h"
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

    try
    {
        Window window("GameEngines5", 1280, 720);
        /*
         * What a scene costs to start: the World's two reserves, the empty archetype on its
         * first entity, and whatever OnCreate spawns. This is the number the arena allocator
         * is meant to bring down, so it is worth seeing on every run.
         */
        MemoryMonitor::SetLogging(true);

        Scene0 scene;
        if (!scene.OnCreate())
        {
            std::cerr << "Scene0::OnCreate failed" << std::endl;
            return 1;
        }

        MemoryMonitor::SetLogging(false);
        Timer timer;
        timer.Start();

        DrawList drawList;
        SDL_Event event;
        while (true)
        {
            while (window.NextEvent(event))
            {
                scene.HandleEvents(event);
            }
            if (window.ShouldClose()) break;

            timer.UpdateFrameTicks();

            scene.Update(timer.GetDeltaTime());
            scene.MakeDrawList(drawList);

            /** Placeholder for Renderer consumption **/

            SDL_Delay(timer.GetSleepTime(60));
        }
        scene.OnDestroy();
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
