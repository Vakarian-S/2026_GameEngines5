#include "core/Window.h"

#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>

/*
 * SDL3 returns true from SDL_Init on success. SDL2 returned 0, which is the opposite, so this
 * is an easy line to get backwards when reading older examples.
 */
Window::Window(const std::string& title, const int width, const int height)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    window = SDL_CreateWindow(title.c_str(), width, height,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        /* Read the error before SDL_Quit, which is free to clear it */
        const std::string message = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        SDL_Quit();
        throw std::runtime_error(message);
    }
}

Window::~Window()
{
    if (window)
    {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

bool Window::PollEvents() noexcept
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        /*
         * QUIT arrives when the last window closes. CLOSE_REQUESTED arrives first, per window.
         * Both are handled, so this still ends when there is more than one window later.
         */
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            return false;
        }

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
        {
            return false;
        }
    }

    return true;
}
