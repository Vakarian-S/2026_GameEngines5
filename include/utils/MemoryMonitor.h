#pragma once
#include <cstdio>
#include <cstdlib>
#include <format>
#include <new>
#include <optional>
#include <string_view>


/**
 * Color Codes for making the terminal easier to read, each Function and Action has its own color
 */
namespace MemColor
{
    inline constexpr std::string_view Reset = "\x1b[0m";
    inline constexpr std::string_view Section = "\x1b[90m"; // dim gray
    inline constexpr std::string_view Value = "\x1b[1;97m"; // bold white

    inline constexpr std::string_view NewPrefix = "\x1b[1;32m"; // bold green
    inline constexpr std::string_view DeletePrefix = "\x1b[1;31m"; // bold red

    // Scalar overloads use the normal color, array overloads use the bright one.
    inline constexpr std::string_view Plain = "\x1b[36m"; // cyan
    inline constexpr std::string_view PlainArray = "\x1b[96m"; // bright cyan
    inline constexpr std::string_view Sized = "\x1b[33m"; // yellow
    inline constexpr std::string_view SizedArray = "\x1b[93m"; // bright yellow
    inline constexpr std::string_view Aligned = "\x1b[35m"; // magenta
    inline constexpr std::string_view AlignedArray = "\x1b[95m"; // bright magenta
    inline constexpr std::string_view SizedAligned = "\x1b[34m"; // blue
    inline constexpr std::string_view SizedAlignedArray = "\x1b[94m"; // bright blue
}

/**
 * Enables Console Colors on terminal
 */
void EnableConsoleColors();


/**
 * Logs allocation and deletion using console colors
 * @param prefixColor
 * @param operation
 * @param functionColor
 * @param function
 * @param size
 * @param alignment
 */
inline void LogOperation(const std::string_view prefixColor, const std::string_view operation,
                         const std::string_view functionColor, const std::string_view function,
                         const std::optional<std::size_t> size = std::nullopt,
                         const std::optional<std::size_t> alignment = std::nullopt)
{
    char buffer[256];
    const char* const end = buffer + sizeof(buffer) - 1; // leave room for '\0'
    char* out = buffer;

    out = std::format_to_n(out, end - out, "{}Operation: {:<7}{}| {}{:<37}{}",
                           prefixColor, operation, MemColor::Reset,
                           functionColor, function, MemColor::Reset).out;
    if (size)
    {
        out = std::format_to_n(out, end - out, "| size = {}{} bytes{}",
                               MemColor::Value, *size, MemColor::Reset).out;
    }
    if (alignment)
    {
        out = std::format_to_n(out, end - out, " | alignment = {}{}{}",
                               MemColor::Value, *alignment, MemColor::Reset).out;
    }
    *out = '\0';

    std::fputs(buffer, stdout);
    std::fputc('\n', stdout);
}


class MemoryMonitor
{
public:
};
