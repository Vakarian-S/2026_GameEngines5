#pragma once
#include <array>
#include <cstddef>
#include <vulkan/vulkan_core.h>

#include "Vector.h"

/**
 * One corner of a shape, as it is stored in a vertex buffer.
 * The vertex shader receives one Vertex per run, split over its "in" variables.
 */
struct Vertex
{
    /** Where the corner is, as x, y and z. The shader reads it at location 0. */
    MATH::Vec3 position;

    /** The corner's Normal as a Vec3, The shader reads it at location 1. */
    MATH::Vec3 normal;

    /** Where the corner sits on the texture, from 0 to 1. The shader reads it at location 2 */
    MATH::Vec2 uv;

    /**
     * Tells the pipeline how big one Vertex is, so the GPU knows how far to step from one vertex to the next.
     * @return the description for vertex buffer slot 0, with one step per vertex
     */
    static VkVertexInputBindingDescription GetVertexInputBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescriptionInfo{};
        bindingDescriptionInfo.binding = 0;
        bindingDescriptionInfo.stride = sizeof(Vertex);
        /* inputRate is a VkVertexInputRate value specifying whether vertex attribute addressing is a function of the vertex index or of the instance index.*/
        bindingDescriptionInfo.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptionInfo;
    }

    /**
     * Tells the pipeline where each field sits inside a Vertex, and which shader "in" variable receives it.
     * @return one description per field: position first, next normal, and then uv
     */
    static std::array<VkVertexInputAttributeDescription, 3> GetVertexInputAttributeDescription()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        /*  the binding number which this attribute takes its data from. */
        attributeDescriptions[0].binding = 0;
        /* is the shader input location number for this attribute. */
        attributeDescriptions[0].location = 0;
        /* format is the size and type of the vertex attribute data.*/
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        /* offset is a byte offset of this attribute relative to the start of an element in the vertex input binding. */
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, uv);

        return attributeDescriptions;
    }
};

/* Eight floats and nothing else. Extra bytes inside the struct would shift every field the GPU reads */
static_assert(sizeof(Vertex) == 32);
