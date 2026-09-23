#pragma once

#include <string>

/**
 * Finding files that ship next to the program.
 *
 * The working directory is not the folder holding the .exe. Rider runs the program from the
 * build tree, a terminal runs it from wherever you happen to be, and a double click runs it
 * from somewhere else again. So fopen("shaders/triangle.vert.spv") works in one of those and
 * not the others, and the failure reads like a Vulkan problem when it is a path problem.
 *
 * These ask SDL where the executable is and build the path from there, so the answer is the
 * same however the program was started.
 *
 * Both return by value and neither keeps a copy. A cached std::string here would still be
 * alive when MemoryMonitor::ReportLeaks runs, and would be listed as a leak it cannot name.
 * SDL does its own caching inside SDL_GetBasePath, out of the monitor's sight, so there is
 * nothing to gain by holding one here.
 */
namespace Paths
{
    /**
     * Makes a path that sits next to the executable.
     *
     * @param relative a path under the executable's folder, such as "shaders/triangle.vert.spv"
     * @return the absolute path, or @p relative unchanged if SDL could not answer
     */
    std::string FromExecutable(const std::string& relative);

    /**
     * @return the folder holding the executable, ending in a separator, or an empty string if
     *         SDL could not answer
     */
    std::string ExecutableDirectory();
}
