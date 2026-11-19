#pragma once
#include <cassert>
#include <cstdint>
#include <memory_resource>

using ResourceId = std::uint32_t;
using ResourceMask = std::uint64_t;

/* One bit per resource type in a ResourceMask, so this can never exceed the mask's width. */
constexpr ResourceId kMaxResources = 64;

namespace RESOURCES
{
    /**
     * Hands out the next unused id. The counter is a function-local static, so it is created
     * once and keeps its value between calls.
     */
    inline ResourceId NextResourceId()
    {
        static ResourceId next = 0;
        return next++;
    }

    /**
     * The id of resource type T, the same number for the whole run.
     *
     * A function-local static inside a template gets one copy per T, set on the first call
     * and never again. That is the whole bridge from a type to a number.
     *
     * Ids are handed out in first-call order, so they are not stable between builds. That is
     * fine until save games: a mask cannot be written to disk and read back by another build.
     *
     * @tparam T the resource type
     */
    template <typename T>
    ResourceId IdOf()
    {
        static const ResourceId id = NextResourceId();
        assert(id < kMaxResources && "Out of resource ids. Widen ResourceMask and kMaxResources.");
        return id;
    }
}