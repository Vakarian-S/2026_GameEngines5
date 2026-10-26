#pragma once
#include "core/FixedStep.h"
#include "core/SceneManager.h"
#include "core/Timer.h"
#include "core/Window.h"
#include "utils/Arena.h"
#include "utils/ArenaResource.h"
#include "vulkan/VulkanRenderer.h"

union SDL_Event;

class Application
{
public:
    /**
    * Opens the window, starts Vulkan, then starts the scene picked on the command line.
    * @param argc the argument count from main
    * @param argv the arguments from main. argv[1], when given, is a row in the scene table
    */
    Application(int argc, char* argv[]);

    /* Owns the window, the renderer and the running scene, so no copies or moves */
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /**
     * Runs the whole application
     */
    void Run();

private:
    /** Window class that holds the surface */
    Window window;
    /** Vulkan renderer used to present the graphics */
    VulkanRenderer renderer;
    /** Class that manages all scenes and scene switching. */
    SceneManager sceneManager;
    /** Measures each frame's time step, and how long to sleep to hold 60 frames per second */
    Timer timer;
    /** Manages the time steps between frames */
    FixedStep fixedStep;
    /** Scratch memory for one frame. Run resets it at the top of every frame */
    Arena frameArena;
    /** Whether the frame arena overflow warning has printed. It prints once per run */
    bool frameArenaWarned = false;
    /** Whether grid and axes are drawn on the current scene */
    bool showWorldReference = true;
    /** How many line vertices the last frame held. The next frame reserves that many. Starts at 512, so the first frame reserves too */
    std::size_t lastDebugLineCount = 512;
    /** Hands frameArena to the draw list as a std::pmr memory resource */
    ArenaResource frameMemory;

    /**
    * Sends one event where it belongs. F1 to F4 request a scene switch, and every other event goes to the scene.
    * @param event the event SDL returned
    */
    void HandleEvent(const SDL_Event& event);
};
