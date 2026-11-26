#pragma once
#include "core/time/FrameCap.h"
#include "core/NullAssetLoader.h"
#include "core/SceneManager.h"
#include "net/NetworkLibrary.h"

#include <optional>


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
    /** Starts GNS before every other member and stops it after them, so no World outlives it */
    NET::NetworkLibrary networkLibrary;
    /** The command line's network part. Built before sceneManager, which takes a copy */
    NetworkSettings networkSettings;
    /** Null override of the Asset loader, as the server application doesn't render assets */
    NullAssetLoader assetLoader;
    /** Manager of the scenes in the app */
    SceneManager sceneManager;
    /** Counts tick in the server ran so far */
    std::uint64_t tickCount = 0;
    /** If set, the amount of ticks the server will run for before closing */
    std::optional<std::uint64_t> tickLifespan = std::nullopt;
    /** Limits the frames per second when set */
    FrameCap frameCap;
    
    /**
     * Closes every client with a reason, then polls GNS for a short time so it can send the closes
     * before the program ends. Without it, the listen socket closes them "ungracefully", and each
     * client only finds out when its connection times out
     */
    void CloseClients();
};  
