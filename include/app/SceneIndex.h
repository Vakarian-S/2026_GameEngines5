#pragma once
#include <cstddef>
#include <span>


struct SceneEntry;

/**
 * Reads the starting scene from the command line.
 * @return the row in argv[1], or the last row when no argument is given. Throws with the scene list on a bad argument
*/
std::size_t PickScene(const int argc, char* argv[]);

/** A span of all the scenes in game */
std::span<const SceneEntry> Scenes();
