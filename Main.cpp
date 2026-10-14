#include <exception>
#include <iostream>
#include <cmath>
#include <SDL3/SDL.h>

#include "MMath.h"
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
#include "vulkan/TemporaryCommandBuffers.h"
#include "vulkan/Pipeline.h"
#include "vulkan/Swapchain.h"
#include "vulkan/Buffer.h"
#include "vulkan/Descriptors.h"
#include "vulkan/Vertex.h"

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
        /*
         * The order of these declarations matters. C++ destroys them in reverse, so FrameLoop goes
         * first and waits for the GPU. The descriptors, buffers and the pipeline are destroyed only after that.
         */
        GPUContext gpuContext(window.GetSDLWindow());
        Descriptors descriptors(gpuContext);
        TemporaryCommandBuffers temporaryCommandBuffers(gpuContext);
        Swapchain swapchain(gpuContext, window.GetSDLWindow());
        Pipeline pipeline(gpuContext, swapchain.GetImageFormat(), swapchain.GetDepthFormat(),
                          "shaders/triangle.vert.spv",
                          "shaders/triangle.frag.spv", descriptors.GetLayout());

        /* The eight corners of a cube: a position and a colour each */
        constexpr Vertex cubeVertices[] = {
            {.position = MATH::Vec3(-0.5f, -0.5f, 0.5f), .color = MATH::Vec3(1.0, 0.37f, 0.38f)},
            {.position = MATH::Vec3(0.5f, -0.5f, 0.5f), .color = MATH::Vec3(1.0f, 0.6f, 0.4f)},
            {.position = MATH::Vec3(0.5f, 0.5f, 0.5f), .color = MATH::Vec3(0.5f, 0.0, 0.5)},
            {.position = MATH::Vec3(-0.5f, 0.5f, 0.5f), .color = MATH::Vec3(0.18f, 0.11f, 0.41f)},
            {.position = MATH::Vec3(-0.5f, -0.5f, -0.5f), .color = MATH::Vec3(1.0, 0.37f, 0.38f)},
            {.position = MATH::Vec3(0.5f, -0.5f, -0.5f), .color = MATH::Vec3(1.0f, 0.6f, 0.4f)},
            {.position = MATH::Vec3(0.5f, 0.5f, -0.5f), .color = MATH::Vec3(0.5f, 0.0, 0.5)},
            {.position = MATH::Vec3(-0.5f, 0.5f, -0.5f), .color = MATH::Vec3(0.18f, 0.11f, 0.41f)},
        };
        /* Each number is a position in cubeVertices. Every three numbers make one triangle */
        constexpr uint32_t cubeIndices[] = {
            /* near face */
            4, 5, 6, 6, 7, 4,
            /* far face */
            0, 2, 1, 0, 3, 2,
            /* right face */
            5, 1, 2, 2, 6, 5,
            /* left face */
            0, 4, 7, 7, 3, 0,
            /* top face */
            0, 1, 5, 5, 4, 0,
            /* bottom face */
            3, 7, 6, 6, 2, 3,
        };

        /* Both arrays are copied into GPU memory once, here. The frame loop only reads them */
        Buffer cubeVertexBuffer(gpuContext, temporaryCommandBuffers, cubeVertices, sizeof(cubeVertices),
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        Buffer indexBuffer(gpuContext, temporaryCommandBuffers, cubeIndices, sizeof(cubeIndices),
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        FrameLoop frameLoop(gpuContext, swapchain, descriptors);
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
                /* Width over height, read every frame, so a resize keeps the cubes' proportions */
                float aspect = static_cast<float>(swapchain.GetExtent().width) / static_cast<float>(swapchain.
                    GetExtent().
                    height);
                /** CameraStruct is temporary **/
                CameraStruct cameraStruct{
                    .view = MMath::lookAt(MATH::Vec3{0, 0, 3}, MATH::Vec3{0, 0, 0}, MATH::Vec3{0, 1, 0}),
                    .projection = MMath::PerspectiveVulkan(45.0f, aspect, 0.1f, 100.0f)
                };
                /* Element 5 is the Y scale. The math library assumes +Y is up, and Vulkan's +Y points down the screen */
                cameraStruct.projection[5] *= -1.0f;
                /* TEMPORARY: two cubes. The second orbits the first, so it passes behind it and tests depth */
                Matrix4 modelMatrices[] = {
                    MATH::MMath::rotate(fmod(elapsedSeconds * 35.0f, 360.f), MATH::Vec3(0.0f, 1.0f, 0.f)),
                    MATH::MMath::rotate(fmod(elapsedSeconds * 55.0f, 360.f), MATH::Vec3(0.0f, 1.0f, 0.f)) *
                    MMath::translate(0.0f, 0.0f, -2.0f)
                };
                std::span<const Matrix4> modelMatricesSpan = modelMatrices;
                frameLoop.DrawFrame({.float32 = {0.0f, pulse, 1.0f - pulse, 1.0f}}, pipeline, cubeVertexBuffer,
                                    indexBuffer, std::size(cubeIndices),
                                    modelMatricesSpan, cameraStruct);
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
