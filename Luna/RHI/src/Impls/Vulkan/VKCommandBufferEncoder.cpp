#include "Device.h"
#include "Impls/Vulkan/VKBuffer.h"
#include "Impls/Vulkan/VKCommandBufferEncoder.h"
#include "Impls/Vulkan/VKCommon.h"
#include "Impls/Vulkan/VKDescriptorSet.h"
#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKPipeline.h"
#include "Impls/Vulkan/VKPipelineLayout.h"
#include "Impls/Vulkan/VKSampler.h"
#include "Impls/Vulkan/VKTexture.h"
#include "Texture.h"

namespace Cacao {
namespace {
bool IsDepthFormat(Format format)
{
    return format == Format::D32F;
}

bool IsStencilFormat(Format format)
{
    return format == Format::D24S8;
}
} // namespace

vk::CommandBufferInheritanceRenderingInfo VKCommandBufferEncoder::ConvertRenderingInfo(const RenderingInfo& info)
{
    vk::CommandBufferInheritanceRenderingInfo vkRenderingInfo{};
    vkRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(info.ColorAttachments.size());
    m_inheritanceColorFormats.resize(info.ColorAttachments.size(), vk::Format::eUndefined);
    for (size_t i = 0; i < info.ColorAttachments.size(); i++) {
        if (!info.ColorAttachments[i].Texture) {
            continue;
        }
        m_inheritanceColorFormats[i] = VKConverter::Convert(info.ColorAttachments[i].Texture->GetFormat());
    }
    vkRenderingInfo.pColorAttachmentFormats = m_inheritanceColorFormats.data();
    if (info.DepthAttachment && info.DepthAttachment->Texture) {
        vkRenderingInfo.depthAttachmentFormat = VKConverter::Convert(info.DepthAttachment->Texture->GetFormat());
    }
    if (info.StencilAttachment && info.StencilAttachment->Texture) {
        vkRenderingInfo.stencilAttachmentFormat = VKConverter::Convert(info.StencilAttachment->Texture->GetFormat());
    }
    return vkRenderingInfo;
}

vk::RenderingInfo VKCommandBufferEncoder::ConvertRenderingInfoBegin(const RenderingInfo& info)
{
    vk::RenderingInfo vkRenderingInfo{};
    vk::Rect2D renderArea = {{info.RenderArea.OffsetX, info.RenderArea.OffsetY},
                             {info.RenderArea.Width, info.RenderArea.Height}};
    vkRenderingInfo.renderArea = renderArea;
    vkRenderingInfo.layerCount = info.LayerCount;
    m_vkColorAttachments.resize(info.ColorAttachments.size());
    for (size_t i = 0; i < info.ColorAttachments.size(); i++) {
        vk::RenderingAttachmentInfo& vkAttachment = m_vkColorAttachments[i];
        const RenderingAttachmentInfo& attachment = info.ColorAttachments[i];
        vkAttachment = vk::RenderingAttachmentInfo();
        if (!attachment.Texture) {
            continue;
        }
        vkAttachment.imageView = std::static_pointer_cast<VKTextureView>(
                                     std::static_pointer_cast<VKTexture>(attachment.Texture)->GetDefaultView())
                                     ->GetHandle();
        vkAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        vkAttachment.resolveMode = vk::ResolveModeFlagBits::eNone;
        switch (attachment.LoadOp) {
            case AttachmentLoadOp::Load:
                vkAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
                break;
            case AttachmentLoadOp::Clear:
                vkAttachment.loadOp = vk::AttachmentLoadOp::eClear;
                break;
            case AttachmentLoadOp::DontCare:
                vkAttachment.loadOp = vk::AttachmentLoadOp::eDontCare;
                break;
        }
        switch (attachment.StoreOp) {
            case AttachmentStoreOp::Store:
                vkAttachment.storeOp = vk::AttachmentStoreOp::eStore;
                break;
            case AttachmentStoreOp::DontCare:
                vkAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
                break;
        }
        if (attachment.LoadOp == AttachmentLoadOp::Clear) {
            vkAttachment.clearValue =
                vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{attachment.ClearValue.Color[0],
                                                                        attachment.ClearValue.Color[1],
                                                                        attachment.ClearValue.Color[2],
                                                                        attachment.ClearValue.Color[3]}));
        }
    }
    vkRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(m_vkColorAttachments.size());
    vkRenderingInfo.pColorAttachments = m_vkColorAttachments.data();
    if (info.DepthAttachment && info.DepthAttachment->Texture) {
        m_vkDepthAttachment = vk::RenderingAttachmentInfo();
        const RenderingAttachmentInfo& attachment = *info.DepthAttachment;
        m_vkDepthAttachment.imageView = std::static_pointer_cast<VKTextureView>(
                                            std::static_pointer_cast<VKTexture>(attachment.Texture)->GetDefaultView())
                                            ->GetHandle();
        m_vkDepthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        m_vkDepthAttachment.resolveMode = vk::ResolveModeFlagBits::eNone;
        switch (attachment.LoadOp) {
            case AttachmentLoadOp::Load:
                m_vkDepthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
                break;
            case AttachmentLoadOp::Clear:
                m_vkDepthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
                break;
            case AttachmentLoadOp::DontCare:
                m_vkDepthAttachment.loadOp = vk::AttachmentLoadOp::eDontCare;
                break;
        }
        switch (attachment.StoreOp) {
            case AttachmentStoreOp::Store:
                m_vkDepthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
                break;
            case AttachmentStoreOp::DontCare:
                m_vkDepthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
                break;
        }
        if (attachment.LoadOp == AttachmentLoadOp::Clear) {
            m_vkDepthAttachment.clearValue = vk::ClearValue(vk::ClearDepthStencilValue(
                attachment.ClearDepthStencil.Depth, static_cast<uint32_t>(attachment.ClearDepthStencil.Stencil)));
        }
        vkRenderingInfo.pDepthAttachment = &m_vkDepthAttachment;
    } else {
        vkRenderingInfo.pDepthAttachment = nullptr;
    }
    if (info.StencilAttachment && info.StencilAttachment->Texture) {
        m_vkStencilAttachment = vk::RenderingAttachmentInfo();
        const RenderingAttachmentInfo& attachment = *info.StencilAttachment;
        m_vkStencilAttachment.imageView = std::static_pointer_cast<VKTextureView>(
                                              std::static_pointer_cast<VKTexture>(attachment.Texture)->GetDefaultView())
                                              ->GetHandle();
        m_vkStencilAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        m_vkStencilAttachment.resolveMode = vk::ResolveModeFlagBits::eNone;
        switch (attachment.LoadOp) {
            case AttachmentLoadOp::Load:
                m_vkStencilAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
                break;
            case AttachmentLoadOp::Clear:
                m_vkStencilAttachment.loadOp = vk::AttachmentLoadOp::eClear;
                break;
            case AttachmentLoadOp::DontCare:
                m_vkStencilAttachment.loadOp = vk::AttachmentLoadOp::eDontCare;
                break;
        }
        switch (attachment.StoreOp) {
            case AttachmentStoreOp::Store:
                m_vkStencilAttachment.storeOp = vk::AttachmentStoreOp::eStore;
                break;
            case AttachmentStoreOp::DontCare:
                m_vkStencilAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
                break;
        }
        if (attachment.LoadOp == AttachmentLoadOp::Clear) {
            m_vkStencilAttachment.clearValue = vk::ClearValue(vk::ClearDepthStencilValue(
                attachment.ClearDepthStencil.Depth, static_cast<uint32_t>(attachment.ClearDepthStencil.Stencil)));
        }
        vkRenderingInfo.pStencilAttachment = &m_vkStencilAttachment;
    } else {
        vkRenderingInfo.pStencilAttachment = nullptr;
    }
    return vkRenderingInfo;
}

Ref<VKCommandBufferEncoder>
    VKCommandBufferEncoder::Create(const Ref<Device>& device, vk::CommandBuffer commandBuffer, CommandBufferType type)
{
    return CreateRef<VKCommandBufferEncoder>(device, commandBuffer, type);
}

VKCommandBufferEncoder::VKCommandBufferEncoder(const Ref<Device>& device,
                                               vk::CommandBuffer commandBuffer,
                                               CommandBufferType type)
    : m_commandBuffer(commandBuffer),
      m_type(type)
{
    if (!device) {
        throw std::runtime_error("VKCommandBufferEncoder created with null device");
    }
    m_device = std::dynamic_pointer_cast<VKDevice>(device);
}

void VKCommandBufferEncoder::Free()
{
    m_device->FreeCommandBuffer(shared_from_this());
}

void VKCommandBufferEncoder::Reset()
{
    m_device->ResetCommandBuffer(shared_from_this());
}

void VKCommandBufferEncoder::ReturnToPool()
{
    m_device->ReturnCommandBuffer(shared_from_this());
}

void VKCommandBufferEncoder::Begin(const CommandBufferBeginInfo& info)
{
    vk::CommandBufferBeginInfo beginInfo{};
    if (info.OneTimeSubmit) {
        beginInfo.flags |= vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    }
    if (info.SimultaneousUse) {
        beginInfo.flags |= vk::CommandBufferUsageFlagBits::eSimultaneousUse;
    }
    vk::CommandBufferInheritanceInfo vkInheritanceInfo;
    vk::CommandBufferInheritanceRenderingInfo vkRenderingInfo;
    if (m_type == CommandBufferType::Secondary && info.InheritanceInfo) {
        if (info.InheritanceInfo->pRenderingInfo) {
            vkRenderingInfo = ConvertRenderingInfo(*info.InheritanceInfo->pRenderingInfo);
            vkInheritanceInfo.pNext = &vkRenderingInfo;
        }
    }
    m_commandBuffer.begin(beginInfo);
}

void VKCommandBufferEncoder::End()
{
    m_commandBuffer.end();
}

void VKCommandBufferEncoder::BeginRendering(const RenderingInfo& info)
{
    vk::RenderingInfo renderInfo = ConvertRenderingInfoBegin(info);
    m_commandBuffer.beginRendering(renderInfo);
}

void VKCommandBufferEncoder::EndRendering()
{
    m_commandBuffer.endRendering();
}

void VKCommandBufferEncoder::BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline)
{
    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 static_cast<VKGraphicsPipeline*>(pipeline.get())->GetHandle());
}

void VKCommandBufferEncoder::BindComputePipeline(const Ref<ComputePipeline>& pipeline)
{
    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute,
                                 static_cast<VKComputePipeline*>(pipeline.get())->GetHandle());
}

void VKCommandBufferEncoder::SetViewport(const Viewport& viewport)
{
    m_commandBuffer.setViewport(
        0, vk::Viewport(viewport.X, viewport.Y, viewport.Width, viewport.Height, viewport.MinDepth, viewport.MaxDepth));
}

void VKCommandBufferEncoder::SetScissor(const Rect2D& scissor)
{
    m_commandBuffer.setScissor(0, vk::Rect2D({scissor.OffsetX, scissor.OffsetY}, {scissor.Width, scissor.Height}));
}

void VKCommandBufferEncoder::BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset)
{
    m_commandBuffer.bindVertexBuffers(binding, static_cast<VKBuffer*>(buffer.get())->GetHandle(), offset);
}

void VKCommandBufferEncoder::BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType)
{
    m_commandBuffer.bindIndexBuffer(
        static_cast<VKBuffer*>(buffer.get())->GetHandle(), offset, VKConverter::Convert(indexType));
}

void VKCommandBufferEncoder::BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline,
                                                uint32_t firstSet,
                                                std::span<const Ref<DescriptorSet>> descriptorSets)
{
    m_boundDescriptorSets.clear();
    m_boundDescriptorSets.reserve(descriptorSets.size());
    for (const auto& descriptorSet : descriptorSets) {
        m_boundDescriptorSets.push_back(static_cast<VKDescriptorSet*>(descriptorSet.get())->GetHandle());
    }
    auto* vkPipeline = static_cast<VKGraphicsPipeline*>(pipeline.get());
    auto* vkLayout = static_cast<VKPipelineLayout*>(vkPipeline->GetLayout().get());
    m_commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       vkLayout->GetHandle(),
                                       firstSet,
                                       static_cast<uint32_t>(m_boundDescriptorSets.size()),
                                       m_boundDescriptorSets.data(),
                                       0,
                                       nullptr);
}

void VKCommandBufferEncoder::PushConstants(
    const Ref<GraphicsPipeline>& pipeline, ShaderStage stageFlags, uint32_t offset, uint32_t size, const void* data)
{
    auto* vkPipeline = static_cast<VKGraphicsPipeline*>(pipeline.get());
    auto* vkLayout = static_cast<VKPipelineLayout*>(vkPipeline->GetLayout().get());
    m_commandBuffer.pushConstants(
        vkLayout->GetHandle(), VKConverter::ConvertShaderStageFlags(stageFlags), offset, size, data);
}

void VKCommandBufferEncoder::Draw(uint32_t vertexCount,
                                  uint32_t instanceCount,
                                  uint32_t firstVertex,
                                  uint32_t firstInstance)
{
    m_commandBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void VKCommandBufferEncoder::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    m_commandBuffer.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VKCommandBufferEncoder::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    m_commandBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
}

void VKCommandBufferEncoder::BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline,
                                                       uint32_t firstSet,
                                                       std::span<const Ref<DescriptorSet>> descriptorSets)
{
    m_boundDescriptorSets.clear();
    m_boundDescriptorSets.reserve(descriptorSets.size());
    for (const auto& descriptorSet : descriptorSets) {
        m_boundDescriptorSets.push_back(static_cast<VKDescriptorSet*>(descriptorSet.get())->GetHandle());
    }
    auto* vkPipeline = static_cast<VKComputePipeline*>(pipeline.get());
    auto* vkLayout = static_cast<VKPipelineLayout*>(vkPipeline->GetLayout().get());
    m_commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                       vkLayout->GetHandle(),
                                       firstSet,
                                       static_cast<uint32_t>(m_boundDescriptorSets.size()),
                                       m_boundDescriptorSets.data(),
                                       0,
                                       nullptr);
}

void VKCommandBufferEncoder::ComputePushConstants(
    const Ref<ComputePipeline>& pipeline, ShaderStage stageFlags, uint32_t offset, uint32_t size, const void* data)
{
    auto* vkPipeline = static_cast<VKComputePipeline*>(pipeline.get());
    auto* vkLayout = static_cast<VKPipelineLayout*>(vkPipeline->GetLayout().get());
    m_commandBuffer.pushConstants(
        vkLayout->GetHandle(), VKConverter::ConvertShaderStageFlags(stageFlags), offset, size, data);
}

void VKCommandBufferEncoder::PipelineBarrier(PipelineStage srcStage,
                                             PipelineStage dstStage,
                                             std::span<const CMemoryBarrier> globalBarriers,
                                             std::span<const BufferBarrier> bufferBarriers,
                                             std::span<const TextureBarrier> textureBarriers)
{
    auto convertSyncScope = [](SyncScope scope) -> VkPipelineStageFlags {
        uint32_t bits = static_cast<uint32_t>(scope);
        if (bits == 0) {
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        VkPipelineStageFlags result = 0;
        if (bits & static_cast<uint32_t>(SyncScope::VertexStage)) {
            result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }
        if (bits & static_cast<uint32_t>(SyncScope::FragmentStage)) {
            result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
        if (bits & static_cast<uint32_t>(SyncScope::ComputeStage)) {
            result |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        if (bits & static_cast<uint32_t>(SyncScope::TransferStage)) {
            result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        if (bits & static_cast<uint32_t>(SyncScope::HostStage)) {
            result |= VK_PIPELINE_STAGE_HOST_BIT;
        }
        if (bits & static_cast<uint32_t>(SyncScope::AllGraphics)) {
            result |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        }
        if (bits & static_cast<uint32_t>(SyncScope::AllCommands)) {
            result |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        return result;
    };
    const VkPipelineStageFlags vkSrcStage = convertSyncScope(srcStage);
    const VkPipelineStageFlags vkDstStage = convertSyncScope(dstStage);
    m_cachedMemoryBarriers.clear();
    m_cachedBufferBarriers.clear();
    m_cachedImageBarriers.clear();
    if (!globalBarriers.empty()) {
        m_cachedMemoryBarriers.resize(globalBarriers.size());
        for (size_t i = 0; i < globalBarriers.size(); ++i) {
            auto oldMapping = VKResourceStateConvert::Convert(globalBarriers[i].OldState);
            auto newMapping = VKResourceStateConvert::Convert(globalBarriers[i].NewState);
            m_cachedMemoryBarriers[i].srcAccessMask = static_cast<vk::AccessFlags>(oldMapping.access);
            m_cachedMemoryBarriers[i].dstAccessMask = static_cast<vk::AccessFlags>(newMapping.access);
        }
    }
    if (!bufferBarriers.empty()) {
        m_cachedBufferBarriers.resize(bufferBarriers.size());
        for (size_t i = 0; i < bufferBarriers.size(); ++i) {
            const auto& barrier = bufferBarriers[i];
            auto* vkBuffer = static_cast<VKBuffer*>(barrier.Buffer.get());
            auto& vkBarrier = m_cachedBufferBarriers[i];
            auto oldMapping = VKResourceStateConvert::Convert(barrier.OldState);
            auto newMapping = VKResourceStateConvert::Convert(barrier.NewState);
            vkBarrier.srcAccessMask = static_cast<vk::AccessFlags>(oldMapping.access);
            vkBarrier.dstAccessMask = static_cast<vk::AccessFlags>(newMapping.access);
            vkBarrier.srcQueueFamilyIndex = barrier.SrcQueueFamily;
            vkBarrier.dstQueueFamilyIndex = barrier.DstQueueFamily;
            vkBarrier.buffer = vkBuffer->GetHandle();
            vkBarrier.offset = barrier.Offset;
            vkBarrier.size = barrier.Size;
        }
    }
    if (!textureBarriers.empty()) {
        m_cachedImageBarriers.resize(textureBarriers.size());
        for (size_t i = 0; i < textureBarriers.size(); ++i) {
            const auto& barrier = textureBarriers[i];
            auto* vkTexture = static_cast<VKTexture*>(barrier.Texture.get());
            auto& vkBarrier = m_cachedImageBarriers[i];
            auto oldMapping = VKResourceStateConvert::Convert(barrier.OldState);
            auto newMapping = VKResourceStateConvert::Convert(barrier.NewState);
            vkBarrier.srcAccessMask = static_cast<vk::AccessFlags>(oldMapping.access);
            vkBarrier.dstAccessMask = static_cast<vk::AccessFlags>(newMapping.access);
            vkBarrier.oldLayout = static_cast<vk::ImageLayout>(oldMapping.layout);
            vkBarrier.newLayout = static_cast<vk::ImageLayout>(newMapping.layout);
            vkBarrier.srcQueueFamilyIndex = barrier.SrcQueueFamily;
            vkBarrier.dstQueueFamilyIndex = barrier.DstQueueFamily;
            vkBarrier.image = vkTexture->GetHandle();
            vkBarrier.subresourceRange.aspectMask =
                static_cast<vk::ImageAspectFlags>(VKFastConvert::ImageAspectFlags(barrier.SubresourceRange.AspectMask));
            vkBarrier.subresourceRange.baseMipLevel = barrier.SubresourceRange.BaseMipLevel;
            vkBarrier.subresourceRange.levelCount = barrier.SubresourceRange.LevelCount;
            vkBarrier.subresourceRange.baseArrayLayer = barrier.SubresourceRange.BaseArrayLayer;
            vkBarrier.subresourceRange.layerCount = barrier.SubresourceRange.LayerCount;
        }
    }
    m_commandBuffer.pipelineBarrier(static_cast<vk::PipelineStageFlags>(vkSrcStage),
                                    static_cast<vk::PipelineStageFlags>(vkDstStage),
                                    vk::DependencyFlags(),
                                    m_cachedMemoryBarriers,
                                    m_cachedBufferBarriers,
                                    m_cachedImageBarriers);
}

namespace {
struct TransitionParams {
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
    VkAccessFlags srcAccess;
    VkAccessFlags dstAccess;
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
};

constexpr TransitionParams kImageTransitionLUT[] = {
    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     0,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
     0,
     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED,
     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
     VK_PIPELINE_STAGE_TRANSFER_BIT,
     0,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED,
     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     0,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     0,
     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED,
     VK_IMAGE_LAYOUT_GENERAL},
    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     0,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_ACCESS_TRANSFER_READ_BIT,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
    {VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_TRANSFER_READ_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_ACCESS_TRANSFER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
     VK_IMAGE_LAYOUT_GENERAL},
    {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     0,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_MEMORY_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_GENERAL,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_ACCESS_MEMORY_WRITE_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_GENERAL,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_ACCESS_MEMORY_WRITE_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_IMAGE_LAYOUT_GENERAL,
     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
};

struct BufferTransitionParams {
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
    VkAccessFlags srcAccess;
    VkAccessFlags dstAccess;
};

constexpr BufferTransitionParams kBufferTransitionLUT[] = {
    {VK_PIPELINE_STAGE_HOST_BIT,
     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
     VK_ACCESS_HOST_WRITE_BIT,
     VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT},
    {VK_PIPELINE_STAGE_HOST_BIT,
     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
     VK_ACCESS_HOST_WRITE_BIT,
     VK_ACCESS_INDEX_READ_BIT},
    {VK_PIPELINE_STAGE_HOST_BIT,
     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_HOST_WRITE_BIT,
     VK_ACCESS_UNIFORM_READ_BIT},
    {VK_PIPELINE_STAGE_HOST_BIT,
     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_HOST_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT},
    {VK_PIPELINE_STAGE_HOST_BIT,
     VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
     VK_ACCESS_HOST_WRITE_BIT,
     VK_ACCESS_INDIRECT_COMMAND_READ_BIT},
    {VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT},
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT},
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_ACCESS_INDEX_READ_BIT},
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_ACCESS_UNIFORM_READ_BIT},
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT},
    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT},
    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_PIPELINE_STAGE_HOST_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_HOST_READ_BIT},
    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_TRANSFER_READ_BIT},
    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT},
    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_INDIRECT_COMMAND_READ_BIT},
};

struct MemoryTransitionParams {
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
    VkAccessFlags srcAccess;
    VkAccessFlags dstAccess;
};

constexpr MemoryTransitionParams kMemoryTransitionLUT[] = {
    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT},
    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT},
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT},
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_ACCESS_SHADER_READ_BIT},
    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_ACCESS_TRANSFER_READ_BIT},
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_ACCESS_MEMORY_WRITE_BIT,
     VK_ACCESS_MEMORY_READ_BIT},
};
} // namespace

void VKCommandBufferEncoder::TransitionImage(const Ref<Texture>& texture,
                                             ImageTransition transition,
                                             const ImageSubresourceRange& range)
{
    const auto& params = kImageTransitionLUT[static_cast<uint8_t>(transition)];
    VkImageMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = params.srcAccess;
    barrier.dstAccessMask = params.dstAccess;
    barrier.oldLayout = params.oldLayout;
    barrier.newLayout = params.newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = static_cast<VKTexture*>(texture.get())->GetHandle();
    VkImageAspectFlags aspectMask = static_cast<VkImageAspectFlags>(range.AspectMask);
    if (texture->IsDepthStencil()) {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = range.BaseMipLevel;
    barrier.subresourceRange.levelCount = range.LevelCount;
    barrier.subresourceRange.baseArrayLayer = range.BaseArrayLayer;
    barrier.subresourceRange.layerCount = range.LayerCount;
    vkCmdPipelineBarrier(m_commandBuffer, params.srcStage, params.dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VKCommandBufferEncoder::TransitionImageFast(VkImage image,
                                                 ImageTransition transition,
                                                 uint32_t baseMipLevel,
                                                 uint32_t levelCount,
                                                 uint32_t baseArrayLayer,
                                                 uint32_t layerCount,
                                                 VkImageAspectFlags aspectMask)
{
    const auto& params = kImageTransitionLUT[static_cast<uint8_t>(transition)];
    VkImageMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = params.srcAccess;
    barrier.dstAccessMask = params.dstAccess;
    barrier.oldLayout = params.oldLayout;
    barrier.newLayout = params.newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = layerCount;
    vkCmdPipelineBarrier(m_commandBuffer, params.srcStage, params.dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VKCommandBufferEncoder::TransitionBuffer(const Ref<Buffer>& buffer,
                                              BufferTransition transition,
                                              uint64_t offset,
                                              uint64_t size)
{
    TransitionBufferFast(static_cast<VKBuffer*>(buffer.get())->GetHandle(), transition, offset, size);
}

void VKCommandBufferEncoder::TransitionBufferFast(VkBuffer buffer,
                                                  BufferTransition transition,
                                                  uint64_t offset,
                                                  uint64_t size)
{
    const auto& params = kBufferTransitionLUT[static_cast<uint8_t>(transition)];
    VkBufferMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = params.srcAccess;
    barrier.dstAccessMask = params.dstAccess;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    vkCmdPipelineBarrier(m_commandBuffer, params.srcStage, params.dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VKCommandBufferEncoder::MemoryBarrierFast(MemoryTransition transition)
{
    const auto& params = kMemoryTransitionLUT[static_cast<uint8_t>(transition)];
    VkMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = params.srcAccess;
    barrier.dstAccessMask = params.dstAccess;
    vkCmdPipelineBarrier(m_commandBuffer, params.srcStage, params.dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void VKCommandBufferEncoder::ResolveTexture(const Ref<Texture>& srcTexture,
                                            const Ref<Texture>& dstTexture,
                                            const ImageSubresourceLayers& srcSubresource,
                                            const ImageSubresourceLayers& dstSubresource)
{
    auto* vkSrc = static_cast<VKTexture*>(srcTexture.get());
    auto* vkDst = static_cast<VKTexture*>(dstTexture.get());

    VkImageResolve region = {};
    region.srcSubresource.aspectMask =
        static_cast<VkImageAspectFlags>(VKFastConvert::ImageAspectFlags(srcSubresource.AspectMask));
    region.srcSubresource.mipLevel = srcSubresource.MipLevel;
    region.srcSubresource.baseArrayLayer = srcSubresource.BaseArrayLayer;
    region.srcSubresource.layerCount = srcSubresource.LayerCount;
    region.srcOffset = {0, 0, 0};
    region.dstSubresource.aspectMask =
        static_cast<VkImageAspectFlags>(VKFastConvert::ImageAspectFlags(dstSubresource.AspectMask));
    region.dstSubresource.mipLevel = dstSubresource.MipLevel;
    region.dstSubresource.baseArrayLayer = dstSubresource.BaseArrayLayer;
    region.dstSubresource.layerCount = dstSubresource.LayerCount;
    region.dstOffset = {0, 0, 0};
    region.extent = {srcTexture->GetWidth(), srcTexture->GetHeight(), 1};

    vkCmdResolveImage(m_commandBuffer,
                      vkSrc->GetHandle(),
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      vkDst->GetHandle(),
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      1,
                      &region);
}

void VKCommandBufferEncoder::ExecuteNative(const std::function<void(void* nativeCommandBuffer)>& func)
{
    func(&m_commandBuffer);
}

void VKCommandBufferEncoder::CopyBufferToImage(const Ref<Buffer>& srcBuffer,
                                               const Ref<Texture>& dstImage,
                                               ImageLayout dstImageLayout,
                                               std::span<const BufferImageCopy> regions)
{
    auto* vkBuffer = static_cast<VKBuffer*>(srcBuffer.get());
    auto* vkTexture = static_cast<VKTexture*>(dstImage.get());
    std::vector<VkBufferImageCopy> vkRegions(regions.size());
    for (size_t i = 0; i < regions.size(); ++i) {
        const auto& region = regions[i];
        auto& vkRegion = vkRegions[i];
        vkRegion.bufferOffset = region.BufferOffset;
        vkRegion.bufferRowLength = region.BufferRowLength;
        vkRegion.bufferImageHeight = region.BufferImageHeight;
        vkRegion.imageSubresource.aspectMask =
            static_cast<VkImageAspectFlags>(VKFastConvert::ImageAspectFlags(region.ImageSubresource.AspectMask));
        vkRegion.imageSubresource.mipLevel = region.ImageSubresource.MipLevel;
        vkRegion.imageSubresource.baseArrayLayer = region.ImageSubresource.BaseArrayLayer;
        vkRegion.imageSubresource.layerCount = region.ImageSubresource.LayerCount;
        vkRegion.imageOffset = {region.ImageOffsetX, region.ImageOffsetY, region.ImageOffsetZ};
        vkRegion.imageExtent = {region.ImageExtentWidth, region.ImageExtentHeight, region.ImageExtentDepth};
    }
    vkCmdCopyBufferToImage(m_commandBuffer,
                           vkBuffer->GetHandle(),
                           vkTexture->GetHandle(),
                           VKResourceStateConvert::GetLayout(dstImageLayout),
                           static_cast<uint32_t>(vkRegions.size()),
                           vkRegions.data());
}

void VKCommandBufferEncoder::CopyBuffer(
    const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
    auto* vkSrcBuffer = static_cast<VKBuffer*>(srcBuffer.get());
    auto* vkDstBuffer = static_cast<VKBuffer*>(dstBuffer.get());
    VkBufferCopy copyRegion;
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    vkCmdCopyBuffer(m_commandBuffer, vkSrcBuffer->GetHandle(), vkDstBuffer->GetHandle(), 1, &copyRegion);
}
} // namespace Cacao
