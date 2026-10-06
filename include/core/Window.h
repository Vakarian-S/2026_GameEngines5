#pragma once

struct SDL_Window;
union SDL_Event;

#include <string>

/**
 * A window, and the SDL video subsystem behind it.
 *
 * One of these at a time. The constructor starts SDL and the destructor stops it, so the
 * engine has one thing to own rather than two. Both constructors throw std::runtime_error
 * with SDL's own message when SDL refuses, which is more use than a null pointer.
 *
 * The window is created with the Vulkan flag already set, so the renderer can ask it for a
 * surface later without the window being destroyed and made again.
 */
class Window
{
public:
    /**
     * Starts SDL and opens the window.
     * @param title what the title bar says
     * @param width client area width in pixels
     * @param height client area height in pixels
     */
    Window(const std::string& title, int width, int height);
    ~Window();

    /* SDL owns one window handle and it is freed in the destructor, so no copies or moves */
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /**
    * Takes the next event waiting in the queue.
    *
    * Close requests are noticed on the way past, so a caller that ignores every event still
    * ends up with ShouldClose answering true.
    *
    * @param out filled with the event when one was waiting
    * @return false once the queue is empty for this frame
    */
    bool NextEvent(SDL_Event& out) noexcept;

    /** @return true once the user has asked to close the window */
    bool ShouldClose() const noexcept { return shouldClose; }

    /** @return SDL's Window, which the Vulkan surface will need */
    SDL_Window* GetSDLWindow() const noexcept { return window; }

private:
    SDL_Window* window = nullptr;
    bool shouldClose = false;
};
