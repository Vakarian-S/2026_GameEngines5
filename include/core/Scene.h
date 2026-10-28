#pragma once

#include <cstdint>

#include "Light.h"

struct Camera;
struct InputState;

union SDL_Event;
class World;
class ResourceLoader;
class SystemRegistry;

class Scene
{
public:
    explicit Scene()
    {
    }

    virtual ~Scene()
    {
    }

    /**
     * Work per frame that lives outside the ECS System: Camera, world counters
     * @param deltaTime time since the last frame
     * @param input current state of the user input
     */
    virtual void Update(float deltaTime, const InputState& input) = 0;

    /**
     * Function that runs on scene Creation
     * @return boolean indicating whether the scene creation was successful or not
     */
    virtual bool OnCreate(ResourceLoader& loader) = 0;

    /**
     * Function that runs when the scene is destroyed
     */
    virtual void OnDestroy() = 0;

    /**
     * Handles user inputs using SDL
     * @param sdlEvent Event recognized by SDL
     */
    virtual void HandleEvents(const SDL_Event& sdlEvent) = 0;

    /**
    * @return The game camera corresponding to this scene
    */
    virtual const Camera& GetCamera() const = 0;

    /**
     * @return Struct containing the light on the scene, which is ambient and a collection of lights
     */
    virtual SceneLighting GetLighting() const = 0;

    /**
     * @return Instance of the scene's own World
     */
    virtual const World& GetWorld() const = 0;

    /**
     * @return Instance of the System Registry used in this scene
     */
    virtual SystemRegistry& GetSystems() = 0;
};
