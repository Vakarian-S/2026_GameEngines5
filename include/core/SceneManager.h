#pragma once

#include <span>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>

#include "Scene.h"

class ResourceLoader;

struct SceneEntry
{
    /** The name printed in the scene list */
    const char* name;
    /** Builds a new scene each time it is called */
    std::unique_ptr<Scene> (*build)();
};

/**
 * Owns the one scene that runs, and builds the others from a table only when asked.
 * A switch waits until ApplyPendingSwitch, so a scene is never destroyed in the middle of its own frame.
 */
class SceneManager
{
public:
    /**
     * @param scenes_ the table of scenes. It must outlive this object
     * @param loader_ hands each new scene its assets. It must outlive this object
     */
    SceneManager(std::span<const SceneEntry> scenes_, ResourceLoader& loader_);
    
    /** Closes the running scene, so its OnDestroy and its report always run */
    ~SceneManager();

    /* Holds a reference and owns a scene, so no copies or moves */
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;

    /**
     * Closes the running scene, if any, then builds and creates the scene at this row.
     * @param sceneIndex A row in the table. Throws if it is past the end
     */
    void Start(std::size_t sceneIndex);

    /**
     * Remembers a switch for the next ApplyPendingSwitch. An index past the end of the table is ignored.
     * @param sceneIndex a row in the table
     */
    void RequestSwitch(std::size_t sceneIndex);

    /** Runs the remembered switch, if there is one. Call it at the top of a frame. */
    void ApplyPendingSwitch();

    /** @return the running scene. Do not keep the reference across a switch */
    Scene& GetCurrentScene() const { return *currentScene; }

private:
    /** Runs OnDestroy, prints the ECS report, then destroys the running scene. Does nothing when none runs */
    void CloseCurrent();

    /** How to build each scene. Only one is built at a time */
    std::span<const SceneEntry> scenes;
    /** Lends each new scene the asset functions in OnCreate */
    ResourceLoader& loader;
    /** The switch to run at the next ApplyPendingSwitch. Empty when none is asked for */
    std::optional<std::size_t> pendingScene;
    /** The running scene */
    std::unique_ptr<Scene> currentScene;
    /** The row the running scene came from, for its name in the report */
    std::size_t currentSceneIndex = 0;
};
