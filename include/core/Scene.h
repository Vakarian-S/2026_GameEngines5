#pragma once

#include <cstdint>
#include <memory_resource>
#include <vector>

#include "DrawList.h"
#include "Light.h"
#include "core/debug/DebugLines.h"


struct Camera;

union SDL_Event;
class World;
class ResourceLoader;

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
     * Function that runs on every frame of the scene
     * @param deltaTime 
     */
    virtual void Update(float deltaTime) = 0;

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
     * Function that makes the list of all components to be rendered to be passed to the renderer
     * @param drawList reference to the list where the elements to be rendered will be stored
     */
    virtual void MakeDrawList(DrawList& drawList) = 0;

    /**
     * DEBUG ONLY
     * The scene adds its own debug lines, such as collider wires, and most scenes add none.
     */
    virtual void MakeDebugLines(DebugLines&) {}

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
};
