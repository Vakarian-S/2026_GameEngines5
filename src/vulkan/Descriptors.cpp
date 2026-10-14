#include "vulkan/Descriptors.h"
#include "vulkan/Buffer.h"

#include <stdexcept>

Descriptors::Descriptors(const GPUContext& gpuContext_) : gpuContext(gpuContext_)
{
    CreateLayout();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateUniformBuffers();
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
    uniformBuffers[kCameraBinding][frameSlot]->Write(&camera, sizeof(CameraStruct));
}

/* ============= 1. Set Layout ============= */
/* The shape of a set. It says what sits at each binding number, and holds no data */
void Descriptors::CreateLayout()
{
    /* One layout binding per row of kUniformBindings. Row i becomes binding i */
    std::array<VkDescriptorSetLayoutBinding, kUniformBindings.size()> layoutBindings{};
    for (uint32_t binding = 0; binding < kUniformBindings.size(); ++binding)
    {
        /* binding is the number the shader uses: layout (set = 0, binding = N) */
        layoutBindings[binding].binding = binding;
        /* descriptorType is the kind of thing the descriptor points at */
        layoutBindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        /* descriptorCount is more than 1 only when the shader declares an array at this binding */
        layoutBindings[binding].descriptorCount = 1;
        /* stageFlags lists the shader stages that may read this binding */
        layoutBindings[binding].stageFlags = kUniformBindings[binding].stages;
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    descriptorSetLayoutCreateInfo.pBindings = layoutBindings.data();

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
    /* Every set holds one uniform buffer descriptor per binding, and there is one set per frame slot */
    poolSize.descriptorCount = static_cast<uint32_t>(kUniformBindings.size()) * kMaxFramesInFlight;

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
void Descriptors::CreateUniformBuffers()
{
    for (uint32_t binding = 0; binding < kUniformBindings.size(); ++binding)
    {
        for (std::unique_ptr<Buffer>& uniformBuffer : uniformBuffers[binding])
        {
            uniformBuffer = std::make_unique<Buffer>(gpuContext, kUniformBindings[binding].size,
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }
}

/* ============= 5. Buffer Info And Write ============= */
/* Fills each set once: binding N of set i points at binding N's buffer for slot i. The sets never change after this */
void Descriptors::PointSetsAtBuffers() const
{
    for (uint32_t frameSlot = 0; frameSlot < kMaxFramesInFlight; ++frameSlot)
    {
        /* The writes point into bufferInfos, so both arrays must live until vkUpdateDescriptorSets returns */
        std::array<VkDescriptorBufferInfo, kUniformBindings.size()> bufferInfos{};
        std::array<VkWriteDescriptorSet, kUniformBindings.size()> descriptorWrites{};

        for (uint32_t binding = 0; binding < kUniformBindings.size(); ++binding)
        {
            /* Which buffer, and which bytes of it, the descriptor points at */
            bufferInfos[binding].buffer = uniformBuffers[binding][frameSlot]->GetBuffer();
            bufferInfos[binding].offset = 0;
            bufferInfos[binding].range = kUniformBindings[binding].size;

            /* Which set and which binding receive that pointer */
            descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[binding].dstSet = descriptorSets[frameSlot];
            descriptorWrites[binding].dstBinding = binding;
            descriptorWrites[binding].dstArrayElement = 0;
            descriptorWrites[binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[binding].descriptorCount = 1;
            descriptorWrites[binding].pBufferInfo = &bufferInfos[binding];
        }

        /* One call fills every binding of this slot's set */
        vkUpdateDescriptorSets(gpuContext.GetDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}
