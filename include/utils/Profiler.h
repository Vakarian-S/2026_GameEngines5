#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

/* ============================== Switch ============================== */

/* On in Debug and Release. Set to 0 to remove the macros at the bottom of this file. */
#ifndef GE5_PROFILING
#define GE5_PROFILING 1
#endif

/* ============================== Console colors ============================== */

/** The profiler prints its own table, so it carries its own colors. */
namespace PROF_COLOR
{
    /** Back to the default color */
    inline constexpr std::string_view kReset = "\x1b[0m";
    /** Dim gray */
    inline constexpr std::string_view kSection = "\x1b[90m";
    /** Bold white */
    inline constexpr std::string_view kValue = "\x1b[1;97m";
    /** Cyan */
    inline constexpr std::string_view kName = "\x1b[36m";
}

/* ============================== Public API ============================== */

/** One named span. Every number is nanoseconds, milliseconds appear only when printing. */
struct ProfileSpan
{
    /** Compared by pointer, so use a string literal or __FUNCTION__. */
    const char* name = nullptr;

    /* This frame. BeginFrame clears these. */

    /** Nanoseconds inside this span so far this frame */
    std::uint64_t frameNanoseconds = 0;

    /** Times the span was entered this frame */
    std::uint32_t frameCalls = 0;

    /* Every frame so far. EndFrame updates these. */

    /** Frames the span ran in. A span timed outside a frame counts each call as one. */
    std::uint64_t framesSeen = 0;
    /** Nanoseconds across every frame */
    std::uint64_t totalNanoseconds = 0;
    /** The fastest frame */
    std::uint64_t minNanoseconds = 0;
    /** The slowest frame */
    std::uint64_t maxNanoseconds = 0;
    /** Times the span was entered, all frames */
    std::uint64_t totalCalls = 0;
};


/** Named timing spans, collected per frame. Never calls new, and is not thread safe. */
class Profiler
{
public:
    /** Spans one program can name. Extra ones are dropped and the report says so. */
    static constexpr std::size_t kMaxSpans = 64;

    /** Clears this frame's totals and starts the frame clock. First line of the loop. */
    static void BeginFrame();

    /** Adds every span's total to its history and times the frame. Last line of the loop. */
    static void EndFrame();

    /**
     * Adds time to a span.
     * @param name a string literal or __FUNCTION__, kept by pointer
     * @param nanoseconds time to add
     */
    static void Record(const char* name, std::uint64_t nanoseconds);

    /**
     * Turns the per frame log line on or off. Recording never stops, only the printing.
     * @param enabled
     */
    static void SetLogging(bool enabled);

    /** @return true if the per frame log line is on */
    static bool IsLogging();

    /** Prints the frame summary and the span table, biggest total first. */
    static void Report();

    /** Forgets every span and every frame. */
    static void Reset();

    /** @return frames closed by EndFrame since the last Reset */
    static std::uint64_t FrameCount();

    /** @return spans named so far */
    static std::size_t SpanCount();

    /** @return pointer to the first of SpanCount() spans */
    static const ProfileSpan* Spans();
};


/** Times the block it sits in. Two clock reads, so use it on a system, not on one entity. */
class ScopedTimer
{
public:
    explicit ScopedTimer(const char* name_) : name(name_), start(std::chrono::steady_clock::now())
    {
    }

    ~ScopedTimer()
    {
        const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        Profiler::Record(
            name, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).
                count()));
    }

    /* One scope, one timer. A copy would record the same scope twice. */
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    const char* name;
    std::chrono::steady_clock::time_point start;
};


/* ============================== Macros ============================== */

/* Two steps, so __LINE__ becomes a number before it is pasted on. */
#define GE5_PROFILE_JOIN_INNER(a, b) a##b
#define GE5_PROFILE_JOIN(a, b) GE5_PROFILE_JOIN_INNER(a, b)

#if GE5_PROFILING

/** Times the enclosing block under a name of your own */
#define PROFILE_SCOPE(name) const ScopedTimer GE5_PROFILE_JOIN(profileScope, __LINE__)(name)

/** Times the enclosing function, named after it */
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

#else

/** Profiling off - does nothing */
#define PROFILE_SCOPE(name) ((void)0)
/** Profiling off - does nothing */
#define PROFILE_FUNCTION() ((void)0)

#endif