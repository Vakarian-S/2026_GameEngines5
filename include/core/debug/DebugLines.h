#pragma once
#include <vector>
#include <memory_resource>

#include "Matrix.h"
#include "Vector.h"

/**
 * Single Vertex to be drawn as a debug line
 */
struct LineVertex
{
    MATH::Vec3 position;
    MATH::Vec3 color;
};

static_assert(sizeof(LineVertex) == 24);

/** Two entries of DebugLines make one line */
using DebugLines = std::pmr::vector<LineVertex>;

/**
 * Adds 2 vertices to the given Debug Lines
 * @param lines DebugLines vector to store the vertices
 * @param from Starting point of the line to draw
 * @param to Ending point of the line to draw
 * @param color colour used to paint the lines
 */
void AddLine(DebugLines& lines, const MATH::Vec3& from, const MATH::Vec3& to, const MATH::Vec3& color);

/**
 * Adds 2 vertices for each axis to the given debug lines
 * @param lines DebugLines vector to store the vertices
 * @param length length of the axes to draw
 */
void AddAxes(DebugLines& lines, float length);

/**
 * Adds the vertices for a grid line in the given Debug Lines
 * @param lines DebugLines vector to store the vertices
 * @param halfSize Half size of the grid as the grid is drawn on both sides of the axes
 * @param spacing spacing between the lines on the grid
 * @param color color to draw the grid
 */
void AddGrid(DebugLines& lines, float halfSize, float spacing, const MATH::Vec3& color);

/**
 * Adds a wireframe box to the debug line data for visualization purposes.
 *
 * @param lines A reference to the DebugLines collection where the computed box lines will be added.
 * @param world The transform matrix to position and orient the wireframe box in world space.
 * @param halfExtents The half dimensions of the box along each axis, defining its size.
 * @param color The colour of the wireframe box, specified as a vector.
 */
void AddWireBox(DebugLines& lines, const MATH::Matrix4& world, MATH::Vec3 halfExtents, const MATH::Vec3& color);

/**
 * Adds a wireframe sphere to the debug line data for visualization purposes.
 *
 * @param lines A reference to the DebugLines collection where the computed sphere lines will be added.
 * @param world The transform matrix to position and orient the wireframe sphere in world space.
 * @param radius The radius of the sphere
 * @param color The colour of the wireframe sphere, specified as a vector.
 */
void AddWireSphere(DebugLines& lines, const MATH::Matrix4& world, float radius, const MATH::Vec3& color);

/**
 * Adds a wireframe representation of a cylinder to the debug drawing lines.
 *
 * @param lines A reference to the DebugLines vector that will store the debug line vertices of the cylinder.
 * @param world A 4x4 transformation matrix representing the cylinder's world transform.
 * @param radius The radius of the cylinder.
 * @param halfHeight The Half-height of the cylinder.
 * @param color The colour of the wireframe cylinder, specified as a vector.
 */
void AddWireCylinder(DebugLines& lines, const MATH::Matrix4& world, float radius, float halfHeight,
                     const MATH::Vec3& color);

/**
 * Adds a wireframe representation of a capsule to the debug drawing lines.
 *
 * @param lines A reference to the DebugLines vector that will store the debug line vertices of the capsule.
 * @param world A 4x4 transformation matrix representing the capsule's world transform.
 * @param radius The radius of the capsule.
 * @param halfHeight The Half-height of the straight part of the capsule.
 * @param color The colour of the wireframe capsule, specified as a vector.
 */
void AddWireCapsule(DebugLines& lines, const MATH::Matrix4& world, float radius, float halfHeight, const MATH::Vec3& color);
