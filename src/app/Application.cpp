#include "app/Application.h"

#include <charconv>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <iostream>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include "scenes/PhysicsScene.h"
#include "scenes/Scene0.h"
#include "scenes/Scene1.h"
#include "utils/MemoryMonitor.h"
#include "utils/Profiler.h"

namespace
{
    /** Bytes of scratch memory one frame may use. 1 MB holds about 14,000 draw items at 72 bytes each; Scene0's 1,000 take 72,000 */
    constexpr std::size_t kFrameArenaBytes = 1024 * 1024;

    /** Every scene the program can run. F1 picks row 0, F2 row 1 and so on. At file scope, so it outlives the manager's span */
    constexpr SceneEntry kScenes[] = {
        {.name = "Scene0", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<Scene0>(); }},
        {.name = "Scene1", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<Scene1>(); }},
        {.name = "Physics Scene", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<PhysicsScene>(); }},
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
            inputState.pressed.fill(false);
            inputState.mousePressed.fill(false);

            while (window.NextEvent(event))
            {
                HandleEvent(event);
            }

            /* Held keys and the mouse are read once, after every event of this frame is in */
            int keyCount = 0;
            const bool* keyboard = SDL_GetKeyboardState(&keyCount);
            std::copy_n(keyboard, std::min(keyCount, static_cast<int>(SDL_SCANCODE_COUNT)), inputState.held.begin());
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            SDL_GetMouseState(&mouseX, &mouseY);
            inputState.mousePosition = MATH::Vec2(mouseX, mouseY);
            int windowWidth = 0;
            int windowHeight = 0;
            SDL_GetWindowSize(window.GetSDLWindow(), &windowWidth, &windowHeight);
            inputState.windowSize = MATH::Vec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        }

        /* Between frames, so nothing below still uses the old scene */
        if (sceneManager.ApplyPendingSwitch())
        {
            fixedStep.Reset();
            timer.Start();
        }
        
        SystemRegistry& systems = sceneManager.GetCurrentScene().GetSystems();
        const Camera* camera = &sceneManager.GetCurrentScene().GetCamera();

        if (window.ShouldClose()) break;

        timer.UpdateFrameTicks();
        {
            PROFILE_SCOPE("Scene::Update");
            sceneManager.GetCurrentScene().Update(timer.GetDeltaTime(), inputState);
            fixedStep.Add(timer.GetDeltaTime());
            while (fixedStep.TakeStep())
                systems.RunPhase(
                    Phase::Fixed,
                    FixedStep::kFixedTimeStep,
                    &inputState,
                    camera
                );
            systems.RunPhase(
                Phase::Update,
                timer.GetDeltaTime(),
                &inputState,
                camera
            );
        }

        {
            PROFILE_SCOPE("MakeDrawList");
            systems.RunPhase(
                Phase::Extract,
                timer.GetDeltaTime(),
                &inputState,
                camera,
                &drawList,
                &debugLines);
            /* The grid first, then the axes: lines do not write depth, so the line drawn last wins where they overlap */
            if (showWorldReference)
            {
                AddGrid(debugLines, 40.0f, 1.0f, MATH::Vec3(0.3f, 0.3f, 0.3f));
                AddAxes(debugLines, 25.0f);
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
    /* Recording Keys on Input State */
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
    {
        inputState.pressed[event.key.scancode] = true;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button < inputState.mousePressed.size())
    {
        inputState.mousePressed[event.button.button] = true;
    }

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
