#pragma once
#include <vector>

class World;
class CommandBuffer;
struct DrawItem;
struct LineVertex;
struct InputState;

/**
 * Struct that contains the info that is passed to all system on run, It is meant to give them context of the scene
 * that they are running on, including the world instance and the command buffer to use
 */
struct SystemContext
{
    /* Warning Disabled because SystemContext is not meant to be reassigned */
    /** Reference to the world where this system is being used */
    World& world; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    /** Reference to the command buffer to use in this system */
    CommandBuffer& commands; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    /** 1/60 s in Fixed, the frame's time in Update and Extract */
    float deltaTime;
    /** Filled in Extract. Null in Fixed and Update */
    std::pmr::vector<DrawItem>* drawList;
    /** Filled in Extract. Null in Fixed and Update */
    std::pmr::vector<LineVertex>* debugLines;
    /** Current state of user input, Set by Application every   frame. Null only in tests that run phases without the app */
    const InputState* inputState;
    
};
