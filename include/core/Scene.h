#pragma once

union SDL_Event;

/* Placeholder */
struct DrawList
{
    float x;
};

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
