#pragma once
#include <array>
#include <memory>
#include <vector>

#include "CommandBuffer.h"
#include "Phase.h"
#include "System.h"

struct Camera;
struct LineVertex;
struct DrawItem;
struct InputState;
class World;

/**
 * Registry that owns and manages every system available on a given world and runs them by phase.
 */
class SystemRegistry
{
public:
    explicit SystemRegistry(World& world_);

    /* The registry owns unique pointers and a reference so we delete copy and move*/
    SystemRegistry(const SystemRegistry&) = delete;
    SystemRegistry& operator=(const SystemRegistry&) = delete;
    SystemRegistry(SystemRegistry&&) = delete;
    SystemRegistry& operator=(SystemRegistry&&) = delete;

    /**
    * Adds a system to the end of a phase. Systems in a phase run in the order they were added, so moving an Add line can change the result.
    * @param phase when the system runs
    * @param system the system to own
    * @returns A pointer to the instance of the recently created system
    */
    System* Add(Phase phase, std::unique_ptr<System> system);

    /**
     * Runs the logic for a whole phase, with all the systems it includes
     * @param phase Phase to run
     * @param deltaTime time since the last frame, for Fixed Phases, its the fixed step time (i.e. 1/60)
     * @param inputState Current state of the users' input
     * @param camera Gameplay camera used for systems that require the view. Null only in tests
     * @param drawItems Draw items if applicable (Only for extract phase)
     * @param debugLines Debug lines to draw if applicable (Only for Extract phase)
     */
    void RunPhase(
        Phase phase,
        float deltaTime,
        const InputState* inputState,
        const Camera* camera = nullptr,
        std::pmr::vector<DrawItem>* drawItems = nullptr,
        std::pmr::vector<LineVertex>* debugLines = nullptr
    );

    /**
     * Prints a small overview of all the systems in the registry, with a note indicating which system we should be
     * wary of as they write onto the same components.
     */
    void Print() const;

    /**
     * Sets the enabled state for a particular system, enabling or preventing its execution
     * @param system Instance of the system to switch
     * @param enabled desired state of the system
     */
    void SetEnabled(System* system, const bool enabled) { system->enabled = enabled; }

    /**
     * Checks whether a given system is enabled
     * @param system instance of the system to check
     * @return boolean indicating whether the system is enabled or not
     */
    bool IsEnabled(const System* system) const { return system->enabled; }

private:
    /** Reference to the world on which the systems will live */
    World& world;
    /** List of every system that is available, grouped by phase to optimize lookup */
    std::array<std::vector<std::unique_ptr<System>>, kPhaseCount> systemsByPhase;
    /** Where every system queues structural changes. Played back after each phase */
    CommandBuffer commands;
};
