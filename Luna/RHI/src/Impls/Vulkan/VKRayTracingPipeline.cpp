#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKPipeline.h"
#include "Impls/Vulkan/VKPipelineLayout.h"
#include "Impls/Vulkan/VKRayTracingPipeline.h"
#include "Impls/Vulkan/VKShaderModule.h"

#include <vector>

namespace luna::RHI {
VKRayTracingPipeline::VKRayTracingPipeline(const Ref<Device>& device, const RayTracingPipelineCreateInfo& info)
    : m_layout(info.Layout),
      m_device(device)
{
    auto vkDevice = std::dynamic_pointer_cast<VKDevice>(device);
    auto& dev = vkDevice->GetHandle();

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    uint32_t rayGenIdx = UINT32_MAX, missIdx = UINT32_MAX, chitIdx = UINT32_MAX;

    for (uint32_t i = 0; i < info.Shaders.size(); i++) {
        VkPipelineShaderStageCreateInfo stageCI = {};
        stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCI.pName = "main";
        auto* vkShader = static_cast<VKShaderModule*>(info.Shaders[i].get());
        stageCI.module = static_cast<VkShaderModule>(vkShader->GetHandle());

        auto stageFlag = info.Shaders[i]->GetStage();
        if (stageFlag == ShaderStage::RayGen) {
            stageCI.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            rayGenIdx = i;
        } else if (stageFlag == ShaderStage::RayMiss) {
            stageCI.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
            missIdx = i;
        } else if (stageFlag == ShaderStage::RayClosestHit) {
            stageCI.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            chitIdx = i;
        }
        stages.push_back(stageCI);
    }

    if (rayGenIdx != UINT32_MAX) {
        VkRayTracingShaderGroupCreateInfoKHR group = {};
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = rayGenIdx;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(group);
    }

    if (missIdx != UINT32_MAX) {
        VkRayTracingShaderGroupCreateInfoKHR group = {};
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = missIdx;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(group);
    }

    if (chitIdx != UINT32_MAX) {
        VkRayTracingShaderGroupCreateInfoKHR group = {};
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = chitIdx;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(group);
    }

    auto* vkLayout = static_cast<VKPipelineLayout*>(info.Layout.get());

    VkRayTracingPipelineCreateInfoKHR rtCI = {};
    rtCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rtCI.stageCount = static_cast<uint32_t>(stages.size());
    rtCI.pStages = stages.data();
    rtCI.groupCount = static_cast<uint32_t>(groups.size());
    rtCI.pGroups = groups.data();
    rtCI.maxPipelineRayRecursionDepth = info.MaxRecursionDepth;
    rtCI.layout = static_cast<VkPipelineLayout>(vkLayout->GetHandle());

    auto vkCreateRTPipeline =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(dev.getProcAddr("vkCreateRayTracingPipelinesKHR"));
    if (vkCreateRTPipeline) {
        VkResult result = vkCreateRTPipeline(
            static_cast<VkDevice>(dev), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rtCI, nullptr, &m_pipeline);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan ray tracing pipeline");
        }
    }
}

VKRayTracingPipeline::~VKRayTracingPipeline()
{
    if (m_pipeline != VK_NULL_HANDLE) {
        auto vkDevice = std::dynamic_pointer_cast<VKDevice>(m_device);
        if (vkDevice) {
            vkDevice->GetHandle().destroyPipeline(m_pipeline);
        }
    }
}
} // namespace luna::RHI
