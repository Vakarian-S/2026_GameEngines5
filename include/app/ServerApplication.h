#pragma once
#include "core/FrameCap.h"
#include "core/NullAssetLoader.h"
#include "core/SceneManager.h"

/**
 * A headless application that runs the Fixed phase only
 */
class ServerApplication
{
public:
    /**
   * Starts in headless mode the scene picked on the command line.
   * @param argc the argument count from main
   * @param argv the arguments from main. 
   */
    ServerApplication(int argc, char* argv[]);

    /* Owns the running scene. so no copies or moves */
    ServerApplication(const ServerApplication&) = delete;
    ServerApplication& operator=(const ServerApplication&) = delete;
    ServerApplication(ServerApplication&&) = delete;
    ServerApplication& operator=(ServerApplication&&) = delete;

    /** Runs the headless application */
    void Run();
private:
    /** Null override of the Asset loader, as the server application doesn't render assets */
    NullAssetLoader assetLoader;
    /** Manager of the scenes in the app */
    SceneManager sceneManager;
    /** Counts tick in the server ran so far */
    std::uint64_t tickCounter = 0;
    /** If set, the amount of ticks the server will run for before closing */
    std::optional<std::uint64_t> tickLifespan = std::nullopt;
    /** Limits the frames per second when set */
    FrameCap frameCap;
};
