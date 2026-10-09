#include <exception>
#include <iostream>
#include <cmath>
#include <SDL3/SDL.h>

#include "core/Timer.h"
#include "core/Window.h"
#include "core/Scene.h"
#include "scenes/Scene0.h"
#include "utils/Arena.h"
#include "utils/ArenaResource.h"
#include "utils/MemoryMonitor.h"
#include "utils/Profiler.h"
#include "vulkan/FrameLoop.h"
#include "vulkan/GPUContext.h"
#include "vulkan/Swapchain.h"

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
    /** Bytes of scratch memory one frame may use. A draw list of 1000 items takes 12,000. */
    constexpr std::size_t kFrameArenaBytes = 64 * 1024;
}

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
        GPUContext gpuContext(window.GetSDLWindow());
        Swapchain swapchain(gpuContext, window.GetSDLWindow());
        FrameLoop frameLoop(gpuContext, swapchain);
        /*
         * What a scene costs to start. The World takes one slab and every archetype, column
         * and slot lives inside it, so the lines printed here are the slab and whatever the
         * scene itself allocates outside the World.
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

        /* Scratch memory that lives for one frame. Taken from the OS once, here. */
        Arena frameArena(kFrameArenaBytes);
        ArenaResource frameMemory(frameArena);

        SDL_Event event;
        float elapsedSeconds = 0.0f;
        while (true)
        {
            Profiler::BeginFrame();

            /* Everything the last frame put in the frame arena is gone from here on. */
            frameArena.Reset();
            DrawList drawList(&frameMemory);
            {
                PROFILE_SCOPE("Events");
                while (window.NextEvent(event))
                {
                    scene.HandleEvents(event);
                }
            }

            if (window.ShouldClose()) break;

            timer.UpdateFrameTicks();

            {
                PROFILE_SCOPE("Scene0::Update");
                scene.Update(timer.GetDeltaTime());
            }

            {
                PROFILE_SCOPE("MakeDrawList");
                scene.MakeDrawList(drawList);
            }

            {
                PROFILE_SCOPE("FrameLoop");
                /* Fades between green and blue, so a frame that stops updating is easy to see */
                elapsedSeconds += timer.GetDeltaTime();
                const float pulse = 0.5f + 0.5f * std::sin(elapsedSeconds);
                frameLoop.DrawFrame({.float32 = {0.0f, pulse, 1.0f - pulse, 1.0f}});
            }

            {
                PROFILE_SCOPE("Wait");
                SDL_Delay(timer.GetSleepTime(60));
            }
            Profiler::EndFrame();

            /* Over budget, so dump the frame while its numbers are still in the table. */
            if (Profiler::LastFrameMilliseconds() > 30.0) Profiler::ReportFrame();
        }
        scene.OnDestroy();

        const FrameStats& totals = scene.GetWorld().Totals();
        const WorldStats snapshot = scene.GetWorld().Snapshot();

        std::cout << "-- ECS Report --\n"
            << "  entities created    " << totals.entitiesCreated << "\n"
            << "  components added    " << totals.componentsAdded << "\n"
            << "  components removed  " << totals.componentsRemoved << "\n"
            << "  migrations          " << totals.migrations << "\n"
            << "  bytes copied        " << totals.bytesCopied << "\n"
            << "  archetypes created  " << totals.archetypesCreated << "\n"
            << "  live entities       " << snapshot.liveEntities << "\n"
            << "  archetypes now      " << snapshot.archetypeCount
            << " (" << snapshot.emptyArchetypes << " empty)\n";

        const WorldMemoryStats worldMemory = scene.GetWorld().MemoryStats();
        const ArenaStats frameStats = frameArena.Stats();

        std::cout << "-- Arena Report --\n"
            << "  world slab          " << worldMemory.arena.capacity << " bytes\n"
            << "  world used          " << worldMemory.arena.used << " bytes\n"
            << "  world allocations   " << worldMemory.resource.fromArena
            << " (" << worldMemory.resource.fromHeap << " fell to the heap)\n"
            << "  frame slab          " << frameStats.capacity << " bytes\n"
            << "  frame high water    " << frameStats.highWater << " bytes\n"
            << "  frame resets        " << frameStats.resets
            << " (" << frameMemory.Stats().fromHeap << " fell to the heap)\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    Profiler::Report();
    MemoryMonitor::Report(baseline);
    MemoryMonitor::ReportLeaks(baseline);

    return 0;
}
