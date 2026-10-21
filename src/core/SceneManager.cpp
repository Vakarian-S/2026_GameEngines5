#include "core/SceneManager.h"

#include "ecs/FrameStats.h"
#include "ecs/World.h"

#include <iostream>
#include <stdexcept>
#include <string>


namespace
{
    /**
    * Print a report from the ECS profiler and the Arena Stats of a given world.
    * @param world World to take the stats from
    */
    void PrintEcsReport(const World& world)
    {
        const FrameStats& totals = world.Totals();
        const WorldStats snapshot = world.Snapshot();

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

        const auto [arena, resource] = world.MemoryStats();

        std::cout << "-- World Arena --\n"
            << "  world capacity          " << arena.capacity << " bytes\n"
            << "  world used          " << arena.used << " bytes\n"
            << "  world allocations   " << resource.fromArena
            << " (" << resource.fromHeap << " fell to the heap)\n";
    }
}

SceneManager::SceneManager(const std::span<const SceneEntry> scenes_, ResourceLoader& loader_) :
    scenes(scenes_),
    loader(loader_)
{
}

SceneManager::~SceneManager()
{
    CloseCurrent();
}

void SceneManager::Start(const std::size_t sceneIndex)
{
    /* Checked before closing, so a bad index leaves the running scene alone */
    if (sceneIndex >= scenes.size())
    {
        throw std::out_of_range("no scene at index " + std::to_string(sceneIndex));
    }

    /* The old scene goes first, so two Worlds never hold memory at the same time */
    CloseCurrent();

    currentScene = scenes[sceneIndex].build();
    currentSceneIndex = sceneIndex;
    if (!currentScene->OnCreate(loader))
    {
        throw std::runtime_error(std::string(scenes[sceneIndex].name) + "::OnCreate failed");
    }
}

void SceneManager::RequestSwitch(std::size_t sceneIndex)
{
    if (sceneIndex >= scenes.size()) return;
    pendingScene = sceneIndex;
}

void SceneManager::ApplyPendingSwitch()
{
    if (!pendingScene.has_value()) return;

    /* Cleared before Start, so a scene that throws in OnCreate is not retried every frame */
    const std::size_t next = *pendingScene;
    pendingScene.reset();
    Start(next);
}

void SceneManager::CloseCurrent()
{
    if (!currentScene) return;

    /* OnDestroy folds the last frame into the totals, so it runs before the report */
    currentScene->OnDestroy();
    std::cout << "-- Closing " << scenes[currentSceneIndex].name << " --\n";
    PrintEcsReport(currentScene->GetWorld());
    currentScene.reset();
}
