#include "app/Application.h"

#include <charconv>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include "core/PresentMode.h"
#include "scenes/PacingScene.h"
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
        {.name = "Pacing Scene", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<PacingScene>(); }}
    };

    /** The present mode the game starts in. Falls back to FIFO when the GPU lacks it */
    constexpr auto kStartPresentMode = PresentMode::Fifo;

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
    renderer(window.GetSDLWindow(), kStartPresentMode),
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
    MemoryMonitor::SetLogging(false);
    std::cout << "present modes:";
    for (const PresentMode mode : {
             PresentMode::Fifo, PresentMode::FifoRelaxed, PresentMode::Mailbox, PresentMode::Immediate
         })
    {
        if (renderer.SupportsPresentMode(mode)) std::cout << " " << PresentModeName(mode);
    }
    std::cout << ". Using " << PresentModeName(renderer.GetPresentMode()) << "\n";
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
            const SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
            /* SDL numbers its buttons from 1, so index 0 stays unused */
            for (std::size_t button = 1; button < inputState.mouseHeld.size(); ++button)
            {
                inputState.mouseHeld[button] = (mouseButtons & SDL_BUTTON_MASK(button)) != 0;
            }

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
        if (frameTimeStats.AddFrame(timer.GetDeltaNanoseconds()))
        {
            const auto& [fps, averageMs, shortestMs, longestMs] = frameTimeStats.GetReport();
            std::ostringstream line;
            line << PresentModeName(renderer.GetPresentMode()) << " | cap ";
            if (frameCap.GetFramesPerSecond() > 0.0) line << std::fixed << std::setprecision(0) << frameCap.
                GetFramesPerSecond();
            else line << "off";
            line << std::fixed << std::setprecision(1) << " | " << fps << " fps | "
                << std::setprecision(2) << averageMs << " ms (min " << shortestMs
                << ", max " << longestMs << ")";
            window.SetTitle(line.str());
            std::cout << line.str() << "\n";
        }
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

        {
            PROFILE_SCOPE("Wait");
            frameCap.Wait();
        }

        /* Warns once, the first time a frame's scratch memory does not fit, so an overflow is seen when it happens */
        if (!frameArenaWarned && frameMemory.Stats().fromHeap > 0)
        {
            frameArenaWarned = true;
            std::cerr << "[WARNING] frame arena full: a request fell to the heap. High water "
                << frameArena.Stats().highWater << " of " << frameArena.Stats().capacity
                << " bytes. Raise kFrameArenaBytes\n";
        }

        Profiler::EndFrame();

        /* Over budget, so dump the frame while its numbers are still in the table. With a cap, over budget means 1.5 cap frames, never under 30 ms */
        const double capFramesPerSecond = frameCap.GetFramesPerSecond();
        const double dumpThresholdMs = capFramesPerSecond > 0.0
                                           ? std::max(30.0, 1.5 * 1000.0 / capFramesPerSecond)
                                           : 30.0;
        if (Profiler::LastFrameMilliseconds() > dumpThresholdMs) Profiler::ReportFrame();
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
    /* Ctrl+P Changes Present Mode */
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_P && (event.key.mod
        & SDL_KMOD_CTRL) != 0)
    {
        const PresentMode next = NextPresentMode(renderer.GetPresentMode());
        renderer.SetPresentMode(next);
        std::cout << "present mode: " << PresentModeName(next) << "\n";
        return;
    }

    /* Ctrl+L cycles the frame cap */
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_L && (event.key.mod
        & SDL_KMOD_CTRL) != 0)
    {
        CycleFrameCap();
        return;
    }

    /* ALT + Enter toggles fullscreen */
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && (event.key.mod & SDL_KMOD_ALT) != 0 && (event.key.
        scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_KP_ENTER))
    {
        const bool on = !window.IsFullscreen();
        window.SetFullscreen(on);
        std::cout << "Fullscreen: " << (on ? "on" : "off") << "\n";
        return;
    }

    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) renderer.RequestSwapchainRebuild();
    sceneManager.GetCurrentScene().HandleEvents(event);
}

PresentMode Application::NextPresentMode(const PresentMode current) const
{
    constexpr int count = static_cast<int>(PresentMode::Count);
    const int start = static_cast<int>(current);
    for (int step = 1; step <= count; ++step)
    {
        const auto candidate = static_cast<PresentMode>((start + step) % count);
        if (renderer.SupportsPresentMode(candidate)) return candidate;
    }
    return current;
}

void Application::CycleFrameCap()
{
    constexpr int count = static_cast<int>(FrameCapStep::Count);
    frameCapStep = static_cast<FrameCapStep>((static_cast<int>(frameCapStep) + 1) % count);

    /* Read at each press, because the window can move between the 240 Hz and 165 Hz screens */
    const double refreshRate = window.GetRefreshRate();
    double framesPerSecond = 0.0;
    switch (frameCapStep)
    {
    case FrameCapStep::Fps30: framesPerSecond = 30.0;
        break;
    case FrameCapStep::Fps60: framesPerSecond = 60.0;
        break;
    case FrameCapStep::Fps100: framesPerSecond = 100.0;
        break;
    case FrameCapStep::HalfRefresh: framesPerSecond = refreshRate / 2.0;
        break;
    case FrameCapStep::RefreshMinus3: framesPerSecond = refreshRate - 3.0;
        break;
    case FrameCapStep::Off:
    case FrameCapStep::Count:
        break;
    }
    frameCap.SetFramesPerSecond(framesPerSecond);

    if (framesPerSecond > 0.0) std::cout << "frame cap: " << std::fixed << std::setprecision(2) << framesPerSecond <<
        "\n";
    else std::cout << "frame cap: off\n";
}
