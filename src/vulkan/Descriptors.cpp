#include "vulkan/Descriptors.h"
#include "vulkan/Buffer.h"

#include <stdexcept>

Descriptors::Descriptors(const GPUContext& gpuContext_) : gpuContext(gpuContext_)
{
    CreateLayout();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCameraBuffers();
    PointSetsAtBuffers();
}

Descriptors::~Descriptors()
{
    /* Destroying the pool frees the sets allocated from it */
    vkDestroyDescriptorPool(gpuContext.GetDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(gpuContext.GetDevice(), descriptorSetLayout, nullptr);
}

void Descriptors::WriteCamera(const uint32_t frameSlot, const CameraStruct& camera) const
{
    cameraBuffers[frameSlot]->Write(&camera, sizeof(CameraStruct));
}

/* ============= 1. Set Layout ============= */
/* The shape of a set. It says what sits at each binding number, and holds no data */
void Descriptors::CreateLayout()
{
    VkDescriptorSetLayoutBinding cameraBinding{};
    /* binding is the number the shader uses: layout (set = 0, binding = 0) */
    cameraBinding.binding = 0;
    /* descriptorType is the kind of thing the descriptor points at */
    cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    /* descriptorCount is more than 1 only when the shader declares an array at this binding */
    cameraBinding.descriptorCount = 1;
    /* stageFlags lists the shader stages that may read this binding */
    cameraBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &cameraBinding;

    if (vkCreateDescriptorSetLayout(gpuContext.GetDevice(), &descriptorSetLayoutCreateInfo, nullptr,
                                    &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

/* ============= 2. Pool ============= */
/* The memory that sets come from. Its size is fixed here: how many sets, and how many descriptors of each type */
void Descriptors::CreateDescriptorPool()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    /* Every set holds one uniform buffer descriptor, and there is one set per frame slot */
    poolSize.descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    /* maxSets is the number of sets that can be allocated from the pool */
    poolInfo.maxSets = kMaxFramesInFlight;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(gpuContext.GetDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

/* ============= 3. Sets ============= */
/* One set per frame slot, each made from the same layout. After this call they exist and point at nothing */
void Descriptors::CreateDescriptorSets()
{
    /* The call wants one layout per set, so the same layout is listed once per frame slot */
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts;
    layouts.fill(descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = kMaxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(gpuContext.GetDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
}

/* ============= 4. Buffers ============= */
/* The data itself. The CPU writes these every frame, so they use memory the CPU can reach */
void Descriptors::CreateCameraBuffers()
{
    for (std::unique_ptr<Buffer>& cameraBuffer : cameraBuffers)
    {
        cameraBuffer = std::make_unique<Buffer>(gpuContext, sizeof(CameraStruct), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

/* ============= 5. Buffer Info And Write ============= */
/* Fills each set once: binding 0 of set i points at camera buffer i. The sets never change after this */
void Descriptors::PointSetsAtBuffers() const
{
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        /* Which buffer, and which bytes of it, the descriptor points at */
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = cameraBuffers[i]->GetBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraStruct);

        /* Which set and which binding receive that pointer */
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(gpuContext.GetDevice(), 1, &descriptorWrite, 0, nullptr);
    }
}
