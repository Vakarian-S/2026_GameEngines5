#pragma once
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <typeinfo>

using ComponentId = uint32_t;
using ComponentMask = uint64_t;

/* One bit per component type in a ComponentMask, so this can never exceed the mask's width. */
constexpr ComponentId kMaxComponents = 64;

/**
 * Everything an archetype needs to know about a component type without knowing the type.
 * It allocates, indexes and copies using nothing but these three numbers.
 */
struct ComponentInfo
{
    /** Bytes one element takes. Zero for a tag, which stores nothing. */
    uint32_t size;
    uint32_t alignment;
    /** For the debug tooling later. On MSVC this reads "struct Position". */
    const char* name;
};

namespace COMPONENTS
{
    /**
     * The one table for the whole process, indexed by ComponentId.
     *
     * A function-local static rather than a global, so it is built before the first caller
     * can reach it no matter which translation unit asks first.
     */
    inline ComponentInfo* InfoTable()
    {
        static ComponentInfo table[kMaxComponents]{};
        return table;
    }

    /**
     * Hands out the next unused id. The counter is a function-local static, so it is created
     * once and keeps its value between calls.
     */
    inline ComponentId NextComponentId()
    {
        static ComponentId next = 0;
        return next++;
    }

    /**
     * Claims an id for T and records its size, alignment, and name. Runs once per type.
     *
     * @tparam T the component type being registered
     */
    template <typename T>
    ComponentId Register()
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "A component must be plain data. No destructor, no std::string, no owning pointers: "
                      "moving an entity between archetypes is a memcpy and nothing else.");

        const ComponentId id = NextComponentId();
        assert(id < kMaxComponents && "Out of component ids. Widen ComponentMask and kMaxComponents.");

        /*
         * sizeof an empty struct is 1, not 0, because C++ gives distinct objects distinct
         * addresses. A tag has to cost nothing, so the size is forced to zero here and the
         * archetype gives it no column at all.
         */
        InfoTable()[id] = ComponentInfo{
            std::is_empty_v<T> ? 0u : static_cast<uint32_t>(sizeof(T)),
            static_cast<uint32_t>(alignof(T)),
            typeid(T).name()
        };

        return id;
    }

    /**
     * The id of component type T, the same number for the whole run.
     *
     * A function-local static inside a template gets one copy per T, set on the first call
     * and never again. That is the whole bridge from a type to a number.
     *
     * Ids are handed out in first-call order, so they are not stable between builds. That is
     * fine until save games: a mask cannot be written to disk and read back by another build.
     *
     * @tparam T the component type
     */
    template <typename T>
    ComponentId IdOf()
    {
        static const ComponentId id = Register<T>();
        return id;
    }

    /**
     * Size, alignment, and name of a registered component.
     *
     * Const because nothing may rewrite a type's size once it is registered.
     */
    inline const ComponentInfo& InfoOf(const ComponentId id)
    {
        assert(id < kMaxComponents);
        return InfoTable()[id];
    }

    /**
     * A mask with one bit set per listed type. This is what a query matches archetypes with.
     *
     * The fold is a loop the compiler writes: start at zero, then OR in one shifted bit per
     * type. MaskOf<Position, Velocity>() with ids 0 and 1 gives 0b11.
     *
     * ComponentMask{1} rather than a plain 1, because shifting an int more than 31 places is
     * undefined and every component past the 32nd would get a wrong bit.
     *
     * The leading ComponentMask{0} makes this a binary fold, so MaskOf<>() compiles and
     * returns zero. That is the empty archetype's mask, and what a query passes for "none"
     * when it excludes nothing.
     *
     * @tparam Args the component types whose bits to set
     */
    template <typename... Args>
    ComponentMask MaskOf()
    {
        return (ComponentMask{0} | ... | (ComponentMask{1} << IdOf<Args>()));
    }
}
