#pragma once
#include <vector>
#include <memory_resource>

#include "Handles.h"
#include "Matrix.h"

/** One thing to draw: which mesh, which material on it, and where it sits in the world. */
struct DrawItem
{
    MeshHandle mesh;
    MaterialHandle material;
    MATH::Matrix4 transform;
    /** False leaves the item out of the top level acceleration structure, so it casts no shadow */
    bool castsShadow = true;
};

/* Two 4-byte handles, 16 floats of matrix and a bool padded to 4 bytes. The frame arena in Application is sized from this */
static_assert(sizeof(DrawItem) == 76);

/** Everything to draw this frame. The caller picks the memory, so a frame arena can stand behind it. */
using DrawList = std::pmr::vector<DrawItem>;

/** Pointer to a draw list */
struct DrawListTarget
{
    DrawList* list = nullptr;
};
