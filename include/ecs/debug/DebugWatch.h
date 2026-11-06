#pragma once

namespace DEBUG
{
    /** Tags an entity so that every system reaching it stops in the debugger. */
    struct DebugWatch
    {
    };

    /** Whether a watched entity stops the debugger. Set it in Rider's watch window while the game runs. */
    inline bool watchBreakArmed = false;

    /** Name of the system SystemRegistry is running right now, or nullptr between systems. */
    inline const char* currentSystem = nullptr;

    /** Name of the only system a watched entity stops in. nullptr stops in every system. */
    inline const char* watchSystem = nullptr;

    /**
     * Stops in the debugger when the break is armed, a debugger is attached, and the running system passes
     * the watchSystem filter. Otherwise it returns at once.
     */
    void BreakOnWatch();
}