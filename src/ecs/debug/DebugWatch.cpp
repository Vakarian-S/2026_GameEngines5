#include "ecs/debug/DebugWatch.h"

#include <string_view>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void DEBUG::BreakOnWatch()
{
    if (!watchBreakArmed || !IsDebuggerPresent()) return;

    /* A query run outside any system has no name, so the filter cannot match it. */
    if (watchSystem != nullptr &&
        (currentSystem == nullptr || std::string_view(watchSystem) != std::string_view(currentSystem)))
    {
        return;
    }
    __debugbreak();
}