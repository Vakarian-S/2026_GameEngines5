#pragma once
#include <cstdint>

    enum class PresentMode : uint8_t
{
    Fifo,
    FifoRelaxed,
    Mailbox,
    Immediate,
    Count
};

/**
 * Returns a char* name for the given Present mode enum
 * @param presentMode Present mode to name
 * @return a char* with the name of the present mode
 */
inline const char* PresentModeName(const PresentMode presentMode)
{
    switch (presentMode)
    {
    case PresentMode::Fifo: return "FIFO";
    case PresentMode::FifoRelaxed: return "FIFO_RELAXED";
    case PresentMode::Mailbox: return "MAILBOX";
    case PresentMode::Immediate: return "IMMEDIATE";
    default: return "Unknown";
    }
}
