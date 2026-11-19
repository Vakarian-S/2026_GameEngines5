#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <utility>

#include "Resource.h"

/**
 * Holds one value per resource type for one World. Each value is built in place from the World's memory
 * and keeps its address until the World dies.
 */
class Resources
{
public:
    /** @param memory_ where every value is built. It must outlive this object */
    explicit Resources(std::pmr::memory_resource* memory_) : memory(memory_)
    {
    }

    /** Destroys every value, the last one inserted first. */
    ~Resources();

    Resources(const Resources&) = delete;
    Resources& operator=(const Resources&) = delete;
    Resources(Resources&&) = delete;
    Resources& operator=(Resources&&) = delete;

    /**
     * Builds the one value of type T in place.
     * @tparam T the resource type. A World holds at most one value of it
     * @param args passed to T's constructor
     * @return the new value
     */
    template <typename T, typename... Args>
    T& Insert(Args&&... args)
    {
        assert(!Has<T>() && "This World already holds a value of this resource type.");
        const ResourceId id = RESOURCES::IdOf<T>();
        void* block = memory->allocate(sizeof(T), alignof(T));
        T* value = new(block) T(std::forward<Args>(args)...);

        slots[id] = Slot{
            .value = value,
            .destroy = [](void* value_, std::pmr::memory_resource* memory_)
            {
                static_cast<T*>(value_)->~T();
                memory_->deallocate(value_, sizeof(T), alignof(T));
            }
        };
        resourcesPresentMask |= ResourceMask{1} << id;
        order[count++] = id;
        return *value;
    }

    /**
     * Stores value as the one value of type T. Inserts it when this World holds none, and assigns over the
     * old one when it does. The address stays the same after the first call.
     * @tparam T the resource type. It must be copyable and copy assignable
     * @param value the value to copy in
     * @return the stored value
     */
    template <typename T>
    T& Set(const T& value)
    {
        if (T* stored = TryGet<T>())
        {
            *stored = value;
            return *stored;
        }
        return Insert<T>(value);
    }

    /**
     * @tparam T the resource type
     * @return true when this World holds a value of type T
     */
    template <typename T>
    bool Has() const
    {
        return (resourcesPresentMask & (ResourceMask{1} << RESOURCES::IdOf<T>())) != 0;
    }

    /**
     * The value of type T. Asserts when this World holds none, because a missing resource is a bug.
     * @tparam T the resource type
     */
    template <typename T>
    T& Get()
    {
        assert(Has<T>() && "This World holds no value of this resource type.");
        return *static_cast<T*>(slots[RESOURCES::IdOf<T>()].value);
    }


    /**
     * @tparam T the resource type
     * @return the value of type T, or nullptr when this World holds none
     */
    template <typename T>
    T* TryGet()
    {
        return Has<T>() ? static_cast<T*>(slots[RESOURCES::IdOf<T>()].value) : nullptr;
    }

private:
    /** One resource value, and how to destroy it without knowing its type. */
    struct Slot
    {
        /** Value of the pointed resource */
        void* value = nullptr;
        /** Destructor function for the resource */
        void (*destroy)(void* value, std::pmr::memory_resource* memory) = nullptr;
    };

    /** Where every value is built and returned. */
    std::pmr::memory_resource* memory;
    /** One slot per resource id. */
    std::array<Slot, kMaxResources> slots{};
    /** One bit per resource id that holds a value. */
    ResourceMask resourcesPresentMask = 0;
    /** The ids in the order they were inserted, so the destructor can go backward. */
    std::array<ResourceId, kMaxResources> order{};
    /** How many ids in order are filled. */
    std::uint32_t count = 0;
};
