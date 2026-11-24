#include "app/ServerApplication.h"

#include <iostream>

#include "app/SceneIndex.h"
#include "core/time/FixedStep.h"
#include "ecs/Phase.h"
#include "ecs/SystemRegistry.h"
#include "utils/Profiler.h"
#include "core/Components.h"
#include "ecs/Query.h"

#include <optional>
#include <span>
#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "core/time/FixedTickCount.h"
#include "ecs/World.h"
#include "game/Components.h"

namespace
{
    /** Ticks the server runs each second. One tick is one fixed step */
    constexpr double kTicksPerSecond = 60.0;

    /** True when Ctrl+Q is waiting in the console's input. Reads and drops every other record */
    bool IsCloseCommandTriggered()
    {
        const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
        DWORD count = 0;
        /* Fails when stdin is not a console, such as a pipe from a script, and then returns false */
        while (GetNumberOfConsoleInputEvents(handle, &count) && count > 0)
        {
            INPUT_RECORD record{};
            DWORD read = 0;
            if (!ReadConsoleInput(handle, &record, 1, &read) || read == 0) return false;

            if (record.EventType != KEY_EVENT) continue;
            const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
            const bool ctrlHeld = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
            if (key.bKeyDown && key.wVirtualKeyCode == 'Q' && ctrlHeld) return true;
        }
        return false;
    }

    /**
     * Reads --ticks N from the command line.
     *  @return N, or no value when the flag is missing. Throws when N is missing, not a number, or 0
     */
    std::optional<std::uint64_t> GetTickLifespan(const int argc, char* argv[])
    {
        /* Wrap argv in a span and skip argv[0] */
        const auto args = std::span(argv, argc).subspan(1);
        const auto flag = std::ranges::find(args, std::string_view("--ticks"));
        if (flag == args.end()) return std::nullopt;

        const auto value = std::next(flag);
        if (value == args.end()) throw std::invalid_argument("--ticks needs a number, such as --ticks 600");

        const std::string_view text = *value;
        std::uint64_t ticks = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), ticks);
        /* Bad when it is not a number, has extra characters after the number, or is 0 */
        if (error != std::errc() || end != text.data() + text.size() || ticks == 0)
            throw std::invalid_argument("--ticks needs a number above 0, not \"" + std::string(text) + "\"");
        return ticks;
    }
}

ServerApplication::ServerApplication(int argc, char* argv[]) :
    sceneManager(Scenes(), assetLoader)
{
    tickLifespan = GetTickLifespan(argc, argv);
    frameCap.SetFramesPerSecond(kTicksPerSecond);
    const std::size_t sceneIndex = PickScene(argc, argv);
    sceneManager.Start(sceneIndex);
}

void ServerApplication::Run()
{
    auto query = Query(sceneManager.GetCurrentScene().GetWorld(), COMPONENTS::MaskOf<TransformComponent, FixedStepRowTag>());
    
    while (
        (!tickLifespan || tickCount < *tickLifespan) &&
        !IsCloseCommandTriggered()
    )
    {
        SystemRegistry& systems = sceneManager.GetCurrentScene().GetSystems();
        {
            PROFILE_SCOPE("[Server]: Fixed Update");
            tickCount++;
            sceneManager.GetCurrentScene().GetWorld().GetResources().Set<FixedTickCount>({.ticks = tickCount});
            systems.RunPhase(Phase::Fixed, FixedStep::kFixedTimeStep);
            
            if (tickCount % static_cast<int>(kTicksPerSecond) == 0)
            {
                /* The query matches only the bottom row, so one Mario from the first chunk is enough */
                bool printed = false;
                query.ForEachChunk([this, &printed](ChunkView chunk)
                {
                    if (printed || chunk.Count() == 0) return;
                    printed = true;
                    std::cout << "tick " << tickCount << ", mario x " <<
                        chunk.Column<TransformComponent>()[0].position.x << "\n";
                });
            }
        }
        {
            PROFILE_SCOPE("[Server]: Wait");
            frameCap.Wait();
        }
    }
    std::cout << "Ran for " << tickCount << " ticks" << std::endl;
}
