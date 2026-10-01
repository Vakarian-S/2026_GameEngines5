#pragma once

#include <memory_resource>
#include <vector>

union SDL_Event;

/** One thing to draw. A placeholder holding a position, until a renderer asks for more. */
struct DrawItem
{
    float x, y, z;
};

/** Everything to draw this frame. The caller picks the memory, so a frame arena can stand behind it. */
using DrawList = std::pmr::vector<DrawItem>;

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
    virtual bool OnCreate() = 0;

    /**
     * Function that runs when the scene is destroyed
     */
    virtual void OnDestroy() = 0;

    /**
     * Function that qmakes the list of all components to be rendered to be passed to the renderer
     * @param drawList reference to the list where the elements to be rendered will be stored
     */
    virtual void MakeDrawList(DrawList& drawList) = 0;

    /**
     * Handles user inputs using SDL
     * @param sdlEvent Event recognized by SDL
     */
    virtual void HandleEvents(const SDL_Event& sdlEvent) = 0;
};
