#include "core/Paths.h"

#include <string>

#include <SDL3/SDL.h>

namespace PATHS
{
    /*
     * SDL owns the string SDL_GetBasePath returns, so it is not freed here, and it already ends
     * in a separator. It is null when SDL cannot work the folder out, which leaves this empty
     * and makes FromExecutable hand back the path it was given.
     */
    std::string ExecutableDirectory()
    {
        const char* base = SDL_GetBasePath();
        return base ? std::string(base) : std::string();
    }

    std::string FromExecutable(const std::string& relative)
    {
        const std::string directory = ExecutableDirectory();
        if (directory.empty())
        {
            return relative;
        }

        return directory + relative;
    }
}
