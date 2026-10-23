#pragma once
#include <vector>
#include <memory_resource>

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
