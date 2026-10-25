#include "ecs/SystemRegistry.h"

#include <cassert>
#include <iterator>
#include <iostream>

#include "ecs/SystemContext.h"
#include "ecs/World.h"
#include "utils/Profiler.h"

namespace
{
    /**
     * Array containing the names given to the playback per phases to use on the profiler
     */
    constexpr const char* kPlaybackNames[] = {
        "Playback Fixed",
        "Playback Update",
        "Playback Extract",
    };
    
    /** Phase names for Print, in the same order as Phase */
    constexpr const char* kPhaseNames[] = {"Fixed", "Update", "Extract"};
    static_assert(std::size(kPhaseNames) == kPhaseCount, "every phase needs a name");

    /**
     * Prints the name of every component in a mask, or a dash when the mask is empty.
     * @param mask one bit per component to print
     */
    void PrintMask(const ComponentMask mask)
    {
        if (mask == 0)
        {
            std::cout << "-";
            return;
        }
        for (ComponentId id = 0; id < kMaxComponents; ++id)
        {
            if ((mask & (ComponentMask{1} << id)) != 0)
            {
                std::cout << COMPONENTS::InfoOf(id).name << " ";
            }
        }
    }
}

static_assert(std::size(kPlaybackNames) == kPhaseCount);

SystemRegistry::SystemRegistry(World& world_) : world(world_)
{
}

void SystemRegistry::Add(Phase phase, std::unique_ptr<System> system)
{
    assert(phase != Phase::Count);
    systemsByPhase[static_cast<size_t>(phase)].push_back(std::move(system));
}

void SystemRegistry::RunPhase(Phase phase, const float deltaTime, std::pmr::vector<DrawItem>* drawItems,
                              std::pmr::vector<LineVertex>* debugLines)
{
    const SystemContext systemContext{
        .world = world,
        .commands = commands,
        .deltaTime = deltaTime,
        .drawList = drawItems,
        .debugLines = debugLines
    };
    for (const std::unique_ptr<System>& system : systemsByPhase[static_cast<size_t>(phase)])
    {
        PROFILE_SCOPE(system->GetName());
        system->Run(systemContext);
    }
    if (commands.Count() > 0)
    {
        PROFILE_SCOPE_ECS(world, kPlaybackNames[static_cast<size_t>(phase)]);
        commands.Playback(world);
    }
}

void SystemRegistry::Print() const
{
    std::cout << "Systems\n";
    for (std::size_t phase = 0; phase < kPhaseCount; ++phase)
    {
        std::cout << "  " << kPhaseNames[phase] << "\n";
        for (const std::unique_ptr<System>& system : systemsByPhase[phase])
        {
            std::cout << "    " << system->GetName() << "  reads ";
            PrintMask(system->GetReads());
            std::cout << "  writes ";
            PrintMask(system->GetWrites());
            std::cout << "\n";
        }
    }

    /* Two systems in one phase that write the same component: their add order changes the result */
    for (std::size_t phase = 0; phase < kPhaseCount; ++phase)
    {
        const std::vector<std::unique_ptr<System>>& systems = systemsByPhase[phase];
        for (std::size_t i = 0; i < systems.size(); ++i)
        {
            for (std::size_t j = i + 1; j < systems.size(); ++j)
            {
                const ComponentMask common = systems[i]->GetWrites() & systems[j]->GetWrites();
                if (common == 0) continue;

                std::cout << "  Note: " << kPhaseNames[phase] << ": " << systems[i]->GetName()
                    << " and " << systems[j]->GetName() << " both write ";
                PrintMask(common);
                std::cout << ". They run in add order.\n";
            }
        }
    }
}
