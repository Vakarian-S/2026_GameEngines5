#pragma once

#include <cstdint>
#include <memory_resource>
#include <vector>

#include "Handles.h"
#include "Light.h"
#include "Matrix.h"

struct Camera;
union SDL_Event;

/** One thing to draw: which mesh, which material on it, and where it sits in the world. */
struct DrawItem
{
    MeshHandle mesh;
    MaterialHandle material;
    MATH::Matrix4 transform;
};

/* Two 4-byte handles and 16 floats of matrix. The frame arena in Main.cpp is sized from this */
static_assert(sizeof(DrawItem) == 72);

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
     * Function that makes the list of all components to be rendered to be passed to the renderer
     * @param drawList reference to the list where the elements to be rendered will be stored
     */
    virtual void MakeDrawList(DrawList& drawList) = 0;

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
};
