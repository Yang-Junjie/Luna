#include "Impls/Vulkan/VKAccelerationStructure.h"
#include "Impls/Vulkan/VKBuffer.h"
#include "Impls/Vulkan/VKCommandBufferEncoder.h"
#include "Impls/Vulkan/VKDescriptorSet.h"
#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKPipelineLayout.h"
#include "Impls/Vulkan/VKQueryPool.h"
#include "Impls/Vulkan/VKRayTracingPipeline.h"
#include "Impls/Vulkan/VKShaderBindingTable.h"
#include "Impls/Vulkan/VKTexture.h"

namespace luna::RHI {
void VKCommandBufferEncoder::DrawIndirect(const Ref<Buffer>& argBuffer,
                                          uint64_t offset,
                                          uint32_t drawCount,
                                          uint32_t stride)
{
    auto* vkBuf = static_cast<VKBuffer*>(argBuffer.get());
    m_commandBuffer.drawIndirect(vkBuf->GetHandle(), offset, drawCount, stride);
}

void VKCommandBufferEncoder::DrawIndexedIndirect(const Ref<Buffer>& argBuffer,
                                                 uint64_t offset,
                                                 uint32_t drawCount,
                                                 uint32_t stride)
{
    auto* vkBuf = static_cast<VKBuffer*>(argBuffer.get());
    m_commandBuffer.drawIndexedIndirect(vkBuf->GetHandle(), offset, drawCount, stride);
}

void VKCommandBufferEncoder::DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset)
{
    auto* vkBuf = static_cast<VKBuffer*>(argBuffer.get());
    m_commandBuffer.dispatchIndirect(vkBuf->GetHandle(), offset);
}

void VKCommandBufferEncoder::DrawIndirectCount(const Ref<Buffer>& argBuffer,
                                               uint64_t offset,
                                               const Ref<Buffer>& countBuffer,
                                               uint64_t countOffset,
                                               uint32_t maxDrawCount,
                                               uint32_t stride)
{
    auto* vkArg = static_cast<VKBuffer*>(argBuffer.get());
    auto* vkCount = static_cast<VKBuffer*>(countBuffer.get());
    m_commandBuffer.drawIndirectCount(
        vkArg->GetHandle(), offset, vkCount->GetHandle(), countOffset, maxDrawCount, stride);
}

void VKCommandBufferEncoder::DrawIndexedIndirectCount(const Ref<Buffer>& argBuffer,
                                                      uint64_t offset,
                                                      const Ref<Buffer>& countBuffer,
                                                      uint64_t countOffset,
                                                      uint32_t maxDrawCount,
                                                      uint32_t stride)
{
    auto* vkArg = static_cast<VKBuffer*>(argBuffer.get());
    auto* vkCount = static_cast<VKBuffer*>(countBuffer.get());
    m_commandBuffer.drawIndexedIndirectCount(
        vkArg->GetHandle(), offset, vkCount->GetHandle(), countOffset, maxDrawCount, stride);
}

void VKCommandBufferEncoder::DispatchMesh(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    auto vkDev = std::dynamic_pointer_cast<VKDevice>(m_device);
    if (!vkDev) {
        return;
    }
    auto fn = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
        vkGetDeviceProcAddr(static_cast<VkDevice>(vkDev->GetHandle()), "vkCmdDrawMeshTasksEXT"));
    if (fn) {
        fn(static_cast<VkCommandBuffer>(m_commandBuffer), groupCountX, groupCountY, groupCountZ);
    }
}

void VKCommandBufferEncoder::BeginDebugLabel(const std::string& name, float r, float g, float b, float a)
{
    // Debug utils extension functions require dynamic loading
    // Skipped when VK_EXT_debug_utils is not available
}

void VKCommandBufferEncoder::EndDebugLabel() {}

void VKCommandBufferEncoder::InsertDebugLabel(const std::string& name, float r, float g, float b, float a) {}

void VKCommandBufferEncoder::BeginQuery(const Ref<QueryPool>& pool, uint32_t queryIndex)
{
    auto* vkPool = static_cast<VKQueryPool*>(pool.get());
    vk::QueryControlFlags flags{};
    if (vkPool->GetType() == QueryType::Occlusion) {
        flags = vk::QueryControlFlagBits::ePrecise;
    }
    m_commandBuffer.beginQuery(vkPool->GetNativeHandle(), queryIndex, flags);
}

void VKCommandBufferEncoder::EndQuery(const Ref<QueryPool>& pool, uint32_t queryIndex)
{
    auto* vkPool = static_cast<VKQueryPool*>(pool.get());
    m_commandBuffer.endQuery(vkPool->GetNativeHandle(), queryIndex);
}

void VKCommandBufferEncoder::WriteTimestamp(const Ref<QueryPool>& pool, uint32_t queryIndex)
{
    auto* vkPool = static_cast<VKQueryPool*>(pool.get());
    m_commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, vkPool->GetNativeHandle(), queryIndex);
}

void VKCommandBufferEncoder::ResetQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count)
{
    auto* vkPool = static_cast<VKQueryPool*>(pool.get());
    m_commandBuffer.resetQueryPool(vkPool->GetNativeHandle(), first, count);
}

void VKCommandBufferEncoder::TraceRays(const Ref<ShaderBindingTable>& sbt, uint32_t w, uint32_t h, uint32_t d)
{
    auto* vkSBT = static_cast<VKShaderBindingTable*>(sbt.get());
    auto raygenRegion = vkSBT->GetRaygenRegion();
    auto missRegion = vkSBT->GetMissRegion();
    auto hitRegion = vkSBT->GetHitRegion();
    VkStridedDeviceAddressRegionKHR callableRegion = {};

    auto vkDev = std::dynamic_pointer_cast<VKDevice>(m_device);
    static auto fn = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(static_cast<VkDevice>(vkDev->GetHandle()), "vkCmdTraceRaysKHR"));
    if (fn) {
        fn(static_cast<VkCommandBuffer>(m_commandBuffer),
           &raygenRegion,
           &missRegion,
           &hitRegion,
           &callableRegion,
           w,
           h,
           d);
    }
}

void VKCommandBufferEncoder::BindRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline)
{
    auto* vkPipeline = static_cast<VKRayTracingPipeline*>(pipeline.get());
    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR,
                                 static_cast<vk::Pipeline>(vkPipeline->GetHandle()));
}

void VKCommandBufferEncoder::BindRayTracingDescriptorSets(const Ref<RayTracingPipeline>& pipeline,
                                                          uint32_t firstSet,
                                                          std::span<const Ref<DescriptorSet>> descriptorSets)
{
    auto* vkLayout = static_cast<VKPipelineLayout*>(pipeline->GetLayout().get());
    std::vector<vk::DescriptorSet> sets;
    for (auto& ds : descriptorSets) {
        auto* vkDS = static_cast<VKDescriptorSet*>(ds.get());
        sets.push_back(vkDS->GetHandle());
    }
    m_commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eRayTracingKHR, vkLayout->GetHandle(), firstSet, sets, {});
}

void VKCommandBufferEncoder::BuildAccelerationStructure(const Ref<AccelerationStructure>& as)
{
    auto* vkAS = static_cast<VKAccelerationStructure*>(as.get());
    auto buildInfo = vkAS->GetBuildInfo();
    auto& rangeInfos = vkAS->GetRangeInfos();
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfos = rangeInfos.data();

    auto vkDev = std::dynamic_pointer_cast<VKDevice>(m_device);
    static auto fn = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(static_cast<VkDevice>(vkDev->GetHandle()), "vkCmdBuildAccelerationStructuresKHR"));
    if (fn) {
        fn(static_cast<VkCommandBuffer>(m_commandBuffer), 1, &buildInfo, &pRangeInfos);
    }
}

void VKCommandBufferEncoder::CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst)
{
    auto* vkSrc = static_cast<VKTexture*>(src.get());
    auto* vkDst = static_cast<VKTexture*>(dst.get());

    VkImageBlit region = {};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.srcOffsets[1] = {(int32_t) vkSrc->GetWidth(), (int32_t) vkSrc->GetHeight(), 1};
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.dstOffsets[1] = {(int32_t) vkDst->GetWidth(), (int32_t) vkDst->GetHeight(), 1};

    vkCmdBlitImage(static_cast<VkCommandBuffer>(m_commandBuffer),
                   static_cast<VkImage>(vkSrc->GetHandle()),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   static_cast<VkImage>(vkDst->GetHandle()),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &region,
                   VK_FILTER_NEAREST);
}
} // namespace luna::RHI
