#include "utils/Profiler.h"

#include <cstdio>
#include <format>
#include <utility>

namespace
{
    /* The whole table. Fixed, because the profiler is not allowed to allocate. */
    ProfileSpan spans[Profiler::kMaxSpans];
    std::size_t spanCount = 0;

    /* Spans refused because the table was full. Reported, so a full table is never silent. */
    std::uint64_t droppedSpans = 0;

    std::chrono::steady_clock::time_point frameStart;
    bool frameOpen = false;

    std::uint64_t frameCount = 0;
    std::uint64_t frameTotalNanoseconds = 0;
    std::uint64_t frameMinNanoseconds = 0;
    std::uint64_t frameMaxNanoseconds = 0;

    bool logging = false;

    /**
     * Finds a span by a name pointer or takes the next free slot
     * @param name name to search with
     * @return the span, or nullptr when the table is full
     */
    ProfileSpan* FindOrAdd(const char* name)
    {
        for (std::size_t i = 0; i < spanCount; ++i)
        {
            if (spans[i].name == name) return &spans[i];
        }
        if (spanCount == Profiler::kMaxSpans) return nullptr;
        ProfileSpan& span = spans[spanCount++];
        span = ProfileSpan{};
        span.name = name;
        return &span;
    }

    constexpr double ToMilliseconds(const std::uint64_t nanoseconds)
    {
        return static_cast<double>(nanoseconds) / 1'000'000.0;
    }

    /**
    * Adds one measurement to a span's total, fastest and slowest.
    * @param span span to update
    * @param nanoseconds the measurement
    */
    void AddMeasurement(ProfileSpan& span, const std::uint64_t nanoseconds)
    {
        if (span.framesSeen == 0 || nanoseconds < span.minNanoseconds)
        {
            span.minNanoseconds = nanoseconds;
        }

        if (nanoseconds > span.maxNanoseconds)
        {
            span.maxNanoseconds = nanoseconds;
        }

        span.totalNanoseconds += nanoseconds;
        ++span.framesSeen;
    }

    template <typename... Args>
    void Print(const std::format_string<Args...> format, Args&&... arguments)
    {
        char line[192];
        *std::format_to_n(line, sizeof(line) - 1, format, std::forward<Args>(arguments)...).out = '\0';
        std::fputs(line, stdout);
    }
}

void Profiler::BeginFrame()
{
    for (std::size_t i = 0; i < spanCount; ++i)
    {
        spans[i].frameNanoseconds = 0;
        spans[i].frameCalls = 0;
    }

    frameStart = std::chrono::steady_clock::now();
    frameOpen = true;
}

void Profiler::EndFrame()
{
    if (!frameOpen) return;

    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    const std::uint64_t elapsed = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - frameStart).count());
    frameOpen = false;

    for (std::size_t i = 0; i < spanCount; ++i)
    {
        ProfileSpan& span = spans[i];

        /* A span that did not run this frame keeps the numbers it already had */
        if (span.frameCalls == 0) continue;
        AddMeasurement(span, span.frameNanoseconds);
    }

    if (frameCount == 0 || elapsed < frameMinNanoseconds) frameMinNanoseconds = elapsed;
    if (elapsed > frameMaxNanoseconds) frameMaxNanoseconds = elapsed;
    frameTotalNanoseconds += elapsed;
    ++frameCount;

    if (logging)
    {
        Print("{}frame{} {:>7}  {:>9.3f} ms\n",
              PROF_COLOR::kSection, PROF_COLOR::kReset, frameCount, ToMilliseconds(elapsed));
    }
}

void Profiler::Record(const char* name, const std::uint64_t nanoseconds)
{
    ProfileSpan* span = FindOrAdd(name);
    if (span == nullptr)
    {
        ++droppedSpans;
        return;
    }
    ++span->totalCalls;

    /* Outside the game loop, so EndFrame will never collect this. Count it as its own frame. */
    if (!frameOpen)
    {
        AddMeasurement(*span, nanoseconds);
        return;
    }

    span->frameNanoseconds += nanoseconds;
    ++span->frameCalls;
}

void Profiler::SetLogging(bool enabled)
{
    logging = enabled;
}

bool Profiler::IsLogging()
{
    return logging;
}

void Profiler::Report()
{
    Print("{}-- Profile Report --{}\n", PROF_COLOR::kSection, PROF_COLOR::kReset);
    if (frameCount == 0)
    {
        Print("  no frames recorded\n");
        return;
    }

    const double averageFrame = ToMilliseconds(frameTotalNanoseconds / frameCount);

    Print("  frames {}{}{}\n", PROF_COLOR::kValue, frameCount, PROF_COLOR::kReset);
    Print("  frame ms   min {}{:.3f}{}   avg {}{:.3f}{}   max {}{:.3f}{}   ({:.1f} fps)\n",
          PROF_COLOR::kValue, ToMilliseconds(frameMinNanoseconds), PROF_COLOR::kReset,
          PROF_COLOR::kValue, averageFrame, PROF_COLOR::kReset,
          PROF_COLOR::kValue, ToMilliseconds(frameMaxNanoseconds), PROF_COLOR::kReset,
          averageFrame > 0.0 ? 1000.0 / averageFrame : 0.0);

    if (spanCount == 0) return;


    /* Biggest total first. Sorts an index list, so the table itself never moves. */
    std::size_t order[Profiler::kMaxSpans];
    for (std::size_t i = 0; i < spanCount; i++) order[i] = i;
    for (std::size_t i = 1; i < spanCount; i++)
    {
        const std::size_t current = order[i];
        const std::uint64_t currentTotal = spans[current].totalNanoseconds;

        std::size_t position = i;
        while (position > 0 && spans[order[position - 1]].totalNanoseconds < currentTotal)
        {
            order[position] = order[position - 1];
            --position;
        }
        order[position] = current;
    }

    Print("{}  {:<28}{:>8}{:>8}{:>11}{:>11}{:>11}{}\n",
          PROF_COLOR::kSection, "span", "frames", "calls", "min ms", "avg ms", "max ms",
          PROF_COLOR::kReset);

    for (std::size_t i = 0; i < spanCount; ++i)
    {
        const ProfileSpan& span = spans[order[i]];
        const double average = ToMilliseconds(span.totalNanoseconds / span.framesSeen);

        Print("  {}{:<28}{}{:>8}{:>8}{:>11.3f}{:>11.3f}{:>11.3f}\n",
              PROF_COLOR::kName, span.name, PROF_COLOR::kReset,
              span.framesSeen,
              span.totalCalls,
              ToMilliseconds(span.minNanoseconds),
              average,
              ToMilliseconds(span.maxNanoseconds));
    }

    /* When a table runs out of size, print the dropped fields info. */
    if (droppedSpans)
    {
        Print("  {}dropped {} records, the table holds {} spans{}\n",
              PROF_COLOR::kSection, droppedSpans, Profiler::kMaxSpans, PROF_COLOR::kReset);
    }
}

void Profiler::Reset()
{
    for (std::size_t i = 0; i < spanCount; ++i) spans[i] = ProfileSpan{};

    spanCount = 0;
    droppedSpans = 0;
    frameOpen = false;
    frameCount = 0;
    frameTotalNanoseconds = 0;
    frameMinNanoseconds = 0;
    frameMaxNanoseconds = 0;
}

std::uint64_t Profiler::FrameCount()
{
    return frameCount;
}

std::size_t Profiler::SpanCount()
{
    return spanCount;
}

const ProfileSpan* Profiler::Spans()
{
    return spans;
}
