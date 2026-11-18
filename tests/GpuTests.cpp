/*
 * Tests for the Vulkan classes that need a real GPU: today, CubeMap.
 *
 * A hidden SDL window gives GPUContext its surface, so the test needs a GPU and a desktop
 * session but shows nothing. x64 only, like the engine itself.
 *
 * In Debug the validation layers run, and every message they print during a check goes to a
 * captured stream. A check fails if that stream holds a [WARNING] or an [ERROR]. Release has no
 * validation layers, so there the same checks only test what the code returns and throws.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>

#include "vulkan/CubeMap.h"
#include "vulkan/GPUContext.h"
#include "vulkan/TemporaryCommandBuffers.h"

namespace
{
    int checks = 0;
    int failures = 0;

    constexpr const char* Pass = "\x1b[1;32m";
    constexpr const char* Fail = "\x1b[1;91m";
    constexpr const char* Reset = "\x1b[0m";

    /** The folder that holds the test cube maps, next to the executable. */
    const std::string kCubeMapFolder = "tests/assets/cubemaps/";

    /** Prints one named expectation, and remembers whether it held. */
    void Check(const char* what, const bool held)
    {
        ++checks;
        if (!held) ++failures;
        std::cout << (held ? "  pass  " : "  FAIL  ") << what << "\n";
    }

    /**
     * Sends std::cerr into a string for as long as it lives, so a check can read what the
     * validation layers printed. The destructor puts std::cerr back.
     */
    class CapturedErrors
    {
    public:
        CapturedErrors() : previous(std::cerr.rdbuf(captured.rdbuf())) {}
        ~CapturedErrors() { std::cerr.rdbuf(previous); }

        CapturedErrors(const CapturedErrors&) = delete;
        CapturedErrors& operator=(const CapturedErrors&) = delete;

        /** @return true if nothing captured so far is a validation warning or error */
        bool Clean() const
        {
            const std::string text = captured.str();
            return text.find("[WARNING]") == std::string::npos && text.find("[ERROR]") == std::string::npos;
        }

        /** @return everything captured so far */
        std::string Text() const { return captured.str(); }

    private:
        std::ostringstream captured;
        std::streambuf* previous;
    };

    /**
     * Tries to load one cube map folder that should be refused.
     * @return the exception's message, or an empty string if the constructor did not throw
     */
    std::string LoadError(const GPUContext& gpuContext, const TemporaryCommandBuffers& commandBuffers,
                          const std::string& folder)
    {
        try
        {
            CubeMap cubeMap(gpuContext, commandBuffers, kCubeMapFolder + folder);
        }
        catch (const std::runtime_error& error)
        {
            return error.what();
        }
        return {};
    }

    /** @return true if text contains part */
    bool Contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }
}

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cout << Fail << "SDL_Init failed: " << SDL_GetError() << Reset << "\n";
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("GpuTests", 64, 64, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    if (window == nullptr)
    {
        std::cout << Fail << "SDL_CreateWindow failed: " << SDL_GetError() << Reset << "\n";
        SDL_Quit();
        return 1;
    }

    {
        const GPUContext gpuContext(window);
        const TemporaryCommandBuffers commandBuffers(gpuContext);

        std::cout << "\nTEST CASE: a folder of six 4x4 faces loads into a cube map\n";
        std::cout << "The image, its six layers, the cube view and the sampler build with no validation messages.\n";
        {
            CapturedErrors errors;
            bool loaded = false;
            bool handlesSet = false;
            try
            {
                const CubeMap cubeMap(gpuContext, commandBuffers, kCubeMapFolder + "good");
                loaded = true;
                handlesSet = cubeMap.GetImageView() != VK_NULL_HANDLE && cubeMap.GetSampler() != VK_NULL_HANDLE;
            }
            catch (const std::exception& error)
            {
                std::cout << "  threw: " << error.what() << "\n";
            }
            Check("the constructor returns", loaded);
            Check("the view and the sampler are set", handlesSet);
            Check("the validation layers print no warning or error", errors.Clean());
            if (!errors.Clean()) std::cout << errors.Text();
        }

        std::cout << "\nTEST CASE: a face of the wrong size is refused\n";
        std::cout << "nz.png is 8x8 and the other five are 4x4. The constructor throws and names the file.\n";
        {
            const std::string message = LoadError(gpuContext, commandBuffers, "wrong_size");
            Check("the constructor throws", !message.empty());
            Check("the message names nz.png", Contains(message, "nz.png"));
        }

        std::cout << "\nTEST CASE: a face that is not square is refused\n";
        std::cout << "py.png is 4x8. The constructor throws and names the file.\n";
        {
            const std::string message = LoadError(gpuContext, commandBuffers, "not_square");
            Check("the constructor throws", !message.empty());
            Check("the message names py.png", Contains(message, "py.png"));
        }

        std::cout << "\nTEST CASE: a missing face is refused\n";
        std::cout << "The folder has no pz.png. The constructor throws and names the file.\n";
        {
            const std::string message = LoadError(gpuContext, commandBuffers, "missing_face");
            Check("the constructor throws", !message.empty());
            Check("the message names pz.png", Contains(message, "pz.png"));
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "\n" << (failures ? Fail : Pass)
        << checks - failures << " of " << checks << " checks passed" << Reset << "\n";

    return failures == 0 ? 0 : 1;
}
