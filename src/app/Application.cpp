#include "app/Application.h"

#include <charconv>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <iostream>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include "scenes/Scene0.h"
#include "scenes/Scene1.h"
#include "utils/MemoryMonitor.h"
#include "utils/Profiler.h"

namespace
{
    /** Bytes of scratch memory one frame may use. 1 MB holds about 14,000 draw items at 72 bytes each; Scene0's 1,000 take 72,000 */
    constexpr std::size_t kFrameArenaBytes = 1024 * 1024;

    /** Every scene the program can run. F1 picks row 0, F2 row 1. At file scope, so it outlives the manager's span */
    constexpr SceneEntry kScenes[] = {
        {.name = "Scene0", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<Scene0>(); }},
        {.name = "Scene1", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<Scene1>(); }},
    };

    /**
     * Reads the starting scene from the command line.
     * @return the row in argv[1], or the last row when no argument is given. Throws with the scene list on a bad argument
     */
    std::size_t PickScene(const int argc, char* argv[])
    {
        const std::size_t sceneCount = std::size(kScenes);
        if (argc < 2) return sceneCount - 1;

        const std::string_view text = argv[1];
        std::size_t index = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), index);

        /* Bad when it is not a number, has extra characters after the number, or is past the table */
        if (error != std::errc() || end != text.data() + text.size() || index >= sceneCount)
        {
            std::string message = "no scene \"" + std::string(text) + "\". Pick one of:\n";
            for (std::size_t i = 0; i < sceneCount; ++i)
            {
                message += "  " + std::to_string(i) + ": " + kScenes[i].name + "\n";
            }
            throw std::invalid_argument(message);
        }
        return index;
    }
}

Application::Application(const int argc, char* argv[]) :
    window("GameEngines5", 1280, 720),
    renderer(window.GetSDLWindow()),
    sceneManager(kScenes, renderer.GetResourceStore()),
    frameArena(kFrameArenaBytes),
    frameMemory(frameArena)
{
    const std::size_t sceneIndex = PickScene(argc, argv);
    /*
     * What a scene costs to start. The World takes one slab and every archetype, column
     * and slot lives inside it, so the lines printed here are the slab and whatever the
     * scene itself allocates outside the World.
     */
    MemoryMonitor::SetLogging(true);
    sceneManager.Start(sceneIndex);
    MemoryMonitor::SetLogging(false);
    timer.Start();
}

void Application::Run()
{
    while (true)
    {
        Profiler::BeginFrame();
        /* Everything the last frame put in the frame arena is gone from here on. */
        frameArena.Reset();
        DrawList drawList(&frameMemory);
        DebugLines debugLines(&frameMemory);
        /*
         * The arena never frees the blocks a vector leaves behind when it grows, so a list grown one push at a time
         * keeps every old size. Reserving last frame's count makes one block instead
         */
        debugLines.reserve(lastDebugLineCount);
        {
            PROFILE_SCOPE("Events");
            SDL_Event event;
            while (window.NextEvent(event))
            {
                HandleEvent(event);
            }
        }

        /* Between frames, so nothing below still uses the old scene */
        if (sceneManager.ApplyPendingSwitch())
        {
            fixedStep.Reset();
            timer.Start();
        }

        SystemRegistry& systems = sceneManager.GetCurrentScene().GetSystems();

        if (window.ShouldClose()) break;

        timer.UpdateFrameTicks();
        {
            PROFILE_SCOPE("Scene::Update");
            sceneManager.GetCurrentScene().Update(timer.GetDeltaTime());
            fixedStep.Add(timer.GetDeltaTime());
            while (fixedStep.TakeStep())
                systems.RunPhase(
                    Phase::Fixed, FixedStep::kFixedTimeStep);
            systems.RunPhase(Phase::Update, timer.GetDeltaTime());
        }

        {
            PROFILE_SCOPE("MakeDrawList");
            systems.RunPhase(Phase::Extract, timer.GetDeltaTime(), &drawList,
                             &debugLines);
            /* The grid first, then the axes: lines do not write depth, so the line drawn last wins where they overlap */
            if (showWorldReference)
            {
                AddGrid(debugLines, 40.0f, 1.0f, MATH::Vec3(0.3f, 0.3f, 0.3f));
                AddAxes(debugLines, 25.0f);
                /* TEMPORARY wire demo. Row 1 unturned, row 2 turned 45 degrees about Z, row 3 turned 30 degrees about X at
                 * scale 2. The grey box uses the same matrix and is known good, so each shape must stay inside it */
                {
                    const MATH::Matrix4 turns[] = {
                        MATH::Matrix4(),
                        MMath::rotate(45.0f, 0.0f, 0.0f, 1.0f),
                        MMath::rotate(30.0f, 1.0f, 0.0f, 0.0f) * MMath::scale(2.0f, 2.0f, 2.0f)
                    };
                    const MATH::Vec3 grey(0.6f, 0.6f, 0.6f);
                    for (int row = 0; row < 3; ++row)
                    {
                        const float z = -8.0f * static_cast<float>(row);

                        const MATH::Matrix4 sphere = MMath::translate(-9.0f, 3.0f, z) * turns[row];
                        AddWireSphere(debugLines, sphere, 1.0f, MATH::Vec3(1.f, 1.f, 0.f));
                        AddWireBox(debugLines, sphere, MATH::Vec3(1.0f, 1.0f, 1.0f), grey);

                        const MATH::Matrix4 cylinder = MMath::translate(-3.0f, 3.0f, z) * turns[row];
                        AddWireCylinder(debugLines, cylinder, 1.0f, 0.5f, MATH::Vec3(0.f, 1.f, 0.f));
                        AddWireBox(debugLines, cylinder, MATH::Vec3(1.0f, 0.5f, 1.0f), grey);

                        const MATH::Matrix4 capsule = MMath::translate(3.0f, 3.0f, z) * turns[row];
                        AddWireCapsule(debugLines, capsule, 0.5f, 0.75f, MATH::Vec3(1.f, 0.f, 1.f));
                        AddWireBox(debugLines, capsule, MATH::Vec3(0.5f, 1.25f, 0.5f), grey);

                        const MATH::Matrix4 box = MMath::translate(9.0f, 3.0f, z) * turns[row];
                        AddWireBox(debugLines, box, MATH::Vec3(1.0f, 1.0f, 1.0f), MATH::Vec3(1.f, 0.f, 0.f));
                    }
                }
            }
        }
        /* The next frame reserves this many. Set after the list is full, so it counts the scene's lines and the grid */
        lastDebugLineCount = debugLines.size();
        {
            PROFILE_SCOPE("Render");
            renderer.DrawFrame(sceneManager.GetCurrentScene().GetCamera(),
                               sceneManager.GetCurrentScene().GetLighting(), drawList, debugLines);
        }

        /* Warns once, the first time a frame's scratch memory does not fit, so an overflow is seen when it happens */
        if (!frameArenaWarned && frameMemory.Stats().fromHeap > 0)
        {
            frameArenaWarned = true;
            std::cerr << "[WARNING] frame arena full: a request fell to the heap. High water "
                << frameArena.Stats().highWater << " of " << frameArena.Stats().capacity
                << " bytes. Raise kFrameArenaBytes\n";
        }

        {
            PROFILE_SCOPE("Wait");
            SDL_Delay(timer.GetSleepTime(60));
        }
        Profiler::EndFrame();

        /* Over budget, so dump the frame while its numbers are still in the table. */
        if (Profiler::LastFrameMilliseconds() > 30.0) Profiler::ReportFrame();
    }

    const ArenaStats frameStats = frameArena.Stats();

    std::cout << "-- Frame Arena Report --\n"
        << "  frame capacity          " << frameStats.capacity << " bytes\n"
        << "  frame high water    " << frameStats.highWater << " bytes\n"
        << "  frame resets        " << frameStats.resets
        << " (" << frameMemory.Stats().fromHeap << " fell to the heap)\n";
}

void Application::HandleEvent(const SDL_Event& event)
{
    /* The application keeps F1 to F4 and G. The scene never sees these keys */
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
        event.key.scancode >= SDL_SCANCODE_F1 && event.key.scancode <= SDL_SCANCODE_F4)
    {
        /* F1 to F4 pick a row in the scene table */
        sceneManager.RequestSwitch(event.key.scancode - SDL_SCANCODE_F1);
        return;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_G)
    {
        /* G shows and hides the grid and the axes in every scene. A held key repeats, so only the first press counts */
        showWorldReference = !showWorldReference;
        return;
    }
    sceneManager.GetCurrentScene().HandleEvents(event);
}
