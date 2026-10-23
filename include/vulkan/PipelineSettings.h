#pragma once
#include <vulkan/vulkan_core.h>

#include "Pipeline.h"

/**
 * The settings for the mesh pipeline: Phong lighting, a material per draw, and one push constant per object.
 * @param frameSet the layout of set 0: the camera and the lights
 * @param materialSet the layout of set 1: one material
 * @return the filled settings, ready for the Pipeline constructor
 */
PipelineSettings MeshPipelineSettings(VkDescriptorSetLayout frameSet, VkDescriptorSetLayout materialSet);

/**
 * The settings for the debug line pipeline: colored lines
 * @param frameSet the layout of set 0: the camera and the lights
 * @return the filled settings, ready for the Pipeline constructor
 */
PipelineSettings LinePipelineSettings(VkDescriptorSetLayout frameSet);