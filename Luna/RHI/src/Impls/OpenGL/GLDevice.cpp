#include "Adapter.h"
#include "Impls/OpenGL/GLBindingGroup.h"
#include "Impls/OpenGL/GLBuffer.h"
#include "Impls/OpenGL/GLCommandBufferEncoder.h"
#include "Impls/OpenGL/GLDescriptor.h"
#include "Impls/OpenGL/GLDevice.h"
#include "Impls/OpenGL/GLPipeline.h"
#include "Impls/OpenGL/GLPipelineCache.h"
#include "Impls/OpenGL/GLQueue.h"
#include "Impls/OpenGL/GLSampler.h"
#include "Impls/OpenGL/GLShaderModule.h"
#include "Impls/OpenGL/GLSwapchain.h"
#include "Impls/OpenGL/GLSynchronization.h"
#include "Impls/OpenGL/GLTexture.h"

namespace Cacao {
Ref<GLDevice> GLDevice::Create(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo)
{
    return std::make_shared<GLDevice>(adapter, createInfo);
}

GLDevice::GLDevice(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo)
    : m_parentAdapter(adapter)
{
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &m_maxComputeWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &m_maxComputeWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &m_maxComputeWorkGroupCount[2]);
}

GLDevice::~GLDevice() = default;

Ref<Queue> GLDevice::GetQueue(QueueType, uint32_t)
{
    if (!m_queue) {
        m_queue = GLQueue::Create(std::dynamic_pointer_cast<GLDevice>(shared_from_this()));
    }
    return m_queue;
}

Ref<Swapchain> GLDevice::CreateSwapchain(const SwapchainCreateInfo& createInfo)
{
    return GLSwapchain::Create(createInfo);
}

std::vector<uint32_t> GLDevice::GetAllQueueFamilyIndices() const
{
    return {0};
}

Ref<Adapter> GLDevice::GetParentAdapter() const
{
    return m_parentAdapter;
}

Ref<CommandBufferEncoder> GLDevice::CreateCommandBufferEncoder(CommandBufferType type)
{
    return GLCommandBufferEncoder::Create(type);
}

void GLDevice::ResetCommandPool() {}

void GLDevice::ReturnCommandBuffer(const Ref<CommandBufferEncoder>&) {}

void GLDevice::FreeCommandBuffer(const Ref<CommandBufferEncoder>&) {}

void GLDevice::ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder)
{
    if (auto gl = std::dynamic_pointer_cast<GLCommandBufferEncoder>(encoder)) {
        gl->Reset();
    }
}

Ref<Texture> GLDevice::CreateTexture(const TextureCreateInfo& createInfo)
{
    return GLTexture::Create(std::dynamic_pointer_cast<Device>(shared_from_this()), createInfo);
}

Ref<Buffer> GLDevice::CreateBuffer(const BufferCreateInfo& createInfo)
{
    return GLBuffer::Create(std::dynamic_pointer_cast<Device>(shared_from_this()), createInfo);
}

Ref<Sampler> GLDevice::CreateSampler(const SamplerCreateInfo& createInfo)
{
    return GLSampler::Create(createInfo);
}

Ref<DescriptorSetLayout> GLDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
{
    return std::make_shared<GLDescriptorSetLayout>(info);
}

Ref<DescriptorPool> GLDevice::CreateDescriptorPool(const DescriptorPoolCreateInfo& info)
{
    return std::make_shared<GLDescriptorPool>(info);
}

Ref<ShaderModule> GLDevice::CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
{
    return GLShaderModule::Create(blob, info);
}

Ref<PipelineLayout> GLDevice::CreatePipelineLayout(const PipelineLayoutCreateInfo& info)
{
    return std::make_shared<GLPipelineLayout>(info);
}

Ref<PipelineCache> GLDevice::CreatePipelineCache(std::span<const uint8_t>)
{
    return std::make_shared<GLPipelineCache>();
}

Ref<GraphicsPipeline> GLDevice::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info)
{
    ValidateGraphicsPipeline(info);
    return GLGraphicsPipeline::Create(info);
}

Ref<ComputePipeline> GLDevice::CreateComputePipeline(const ComputePipelineCreateInfo& info)
{
    return GLComputePipeline::Create(info);
}

Ref<Synchronization> GLDevice::CreateSynchronization(uint32_t maxFramesInFlight)
{
    return GLSynchronization::Create(maxFramesInFlight);
}
} // namespace Cacao
