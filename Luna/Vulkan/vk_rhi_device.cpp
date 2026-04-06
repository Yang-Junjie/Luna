#include "vk_rhi_device.h"

#include "Core/log.h"
#include "Imgui/ImGuiLayer.hpp"
#include "RHI/CommandContext.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_shader.h"
#include "VkBootstrap.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#include <atomic>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <vector>

namespace luna {
namespace {

#ifndef NDEBUG
constexpr bool kUseValidationLayers = true;
#else
constexpr bool kUseValidationLayers = false;
#endif

std::atomic<uint64_t> g_nextVulkanDeviceId{1};
std::atomic<uint64_t> g_nextVulkanSurfaceId{1};
std::atomic<uint64_t> g_nextVulkanSwapchainId{1};

uint64_t make_adapter_id(const VkPhysicalDeviceProperties& properties)
{
    const uint64_t combined =
        (static_cast<uint64_t>(properties.vendorID) << 32u) | static_cast<uint64_t>(properties.deviceID);
    return combined != 0 ? combined : (1ull << 63u) | static_cast<uint64_t>(properties.deviceType);
}

template <typename T> void log_vkb_error(const char* step, const vkb::Result<T>& result)
{
    LUNA_CORE_ERROR("{} failed: {}", step, result.error().message());
    for (const std::string& reason : result.full_error().detailed_failure_reasons) {
        LUNA_CORE_ERROR("  {}", reason);
    }
}

class NativeWindowProxy final : public Window {
public:
    explicit NativeWindowProxy(GLFWwindow* nativeWindow)
        : m_nativeWindow(nativeWindow)
    {}

    void onUpdate() override {}

    uint32_t getWidth() const override
    {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(m_nativeWindow, &width, &height);
        return width > 0 ? static_cast<uint32_t>(width) : 0u;
    }

    uint32_t getHeight() const override
    {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(m_nativeWindow, &width, &height);
        return height > 0 ? static_cast<uint32_t>(height) : 0u;
    }

    void setEventCallback(const EventCallbackFn&) override {}

    void getWindowPos(int* x, int* y) const override
    {
        glfwGetWindowPos(m_nativeWindow, x, y);
    }

    void setWindowPos(int x, int y) override
    {
        glfwSetWindowPos(m_nativeWindow, x, y);
    }

    void setMaximized() override
    {
        glfwMaximizeWindow(m_nativeWindow);
    }

    void setRestored() override
    {
        glfwRestoreWindow(m_nativeWindow);
    }

    void setMinimized() override
    {
        glfwIconifyWindow(m_nativeWindow);
    }

    void setVSync(bool enabled) override
    {
        m_vsync = enabled;
    }

    bool isVSync() const override
    {
        return m_vsync;
    }

    void* getNativeWindow() const override
    {
        return m_nativeWindow;
    }

private:
    GLFWwindow* m_nativeWindow = nullptr;
    bool m_vsync = false;
};

uint32_t max_mip_levels_for_desc(const ImageDesc& desc)
{
    uint32_t maxDimension = std::max(desc.width, desc.height);
    if (desc.type == ImageType::Image3D) {
        maxDimension = std::max(maxDimension, desc.depth);
    }

    uint32_t levels = 0;
    do {
        ++levels;
        maxDimension >>= 1;
    } while (maxDimension > 0);

    return levels;
}

bool validate_image_desc(const ImageDesc& desc)
{
    return is_image_desc_legal(desc);
}

vk::ImageAspectFlags to_vulkan_image_aspect_flags(ImageAspect aspect)
{
    switch (aspect) {
        case ImageAspect::Depth:
            return vk::ImageAspectFlagBits::eDepth;
        case ImageAspect::Color:
        default:
            return vk::ImageAspectFlagBits::eColor;
    }
}

vk::ImageViewType to_vulkan_image_view_type(ImageViewType type)
{
    switch (type) {
        case ImageViewType::Cube:
            return vk::ImageViewType::eCube;
        case ImageViewType::Image2DArray:
            return vk::ImageViewType::e2DArray;
        case ImageViewType::Image3D:
            return vk::ImageViewType::e3D;
        case ImageViewType::Image2D:
        default:
            return vk::ImageViewType::e2D;
    }
}

ImageAspect default_image_aspect(const ImageDesc& desc)
{
    return default_image_aspect_for_format(desc.format);
}

ImageViewType default_image_view_type(const ImageDesc& desc)
{
    return default_view_type_for_image_desc(desc);
}

bool validate_image_view_desc(const ImageDesc& imageDesc, const ImageViewDesc& desc)
{
    return is_image_view_desc_legal(imageDesc, desc);
}

vk::BorderColor to_vulkan_border_color(SamplerBorderColor color)
{
    switch (color) {
        case SamplerBorderColor::FloatOpaqueBlack:
            return vk::BorderColor::eFloatOpaqueBlack;
        case SamplerBorderColor::FloatOpaqueWhite:
            return vk::BorderColor::eFloatOpaqueWhite;
        case SamplerBorderColor::FloatTransparentBlack:
        default:
            return vk::BorderColor::eFloatTransparentBlack;
    }
}


PixelFormat from_vulkan_format(vk::Format format)
{
    switch (format) {
        case vk::Format::eB8G8R8A8Unorm:
            return PixelFormat::BGRA8Unorm;
        case vk::Format::eR8G8B8A8Unorm:
            return PixelFormat::RGBA8Unorm;
        case vk::Format::eR8G8B8A8Srgb:
            return PixelFormat::RGBA8Srgb;
        case vk::Format::eR16G16Sfloat:
            return PixelFormat::RG16Float;
        case vk::Format::eR16G16B16A16Sfloat:
            return PixelFormat::RGBA16Float;
        case vk::Format::eR32Sfloat:
            return PixelFormat::R32Float;
        case vk::Format::eB10G11R11UfloatPack32:
            return PixelFormat::R11G11B10Float;
        case vk::Format::eD32Sfloat:
            return PixelFormat::D32Float;
        default:
            return PixelFormat::Undefined;
    }
}

std::string_view to_backend_format_name(vk::Format format)
{
    switch (format) {
        case vk::Format::eB8G8R8A8Unorm:
            return "VK_FORMAT_B8G8R8A8_UNORM";
        case vk::Format::eR8G8B8A8Unorm:
            return "VK_FORMAT_R8G8B8A8_UNORM";
        case vk::Format::eR8G8B8A8Srgb:
            return "VK_FORMAT_R8G8B8A8_SRGB";
        case vk::Format::eR16G16Sfloat:
            return "VK_FORMAT_R16G16_SFLOAT";
        case vk::Format::eR16G16B16A16Sfloat:
            return "VK_FORMAT_R16G16B16A16_SFLOAT";
        case vk::Format::eR32Sfloat:
            return "VK_FORMAT_R32_SFLOAT";
        case vk::Format::eB10G11R11UfloatPack32:
            return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
        case vk::Format::eD32Sfloat:
            return "VK_FORMAT_D32_SFLOAT";
        case vk::Format::eUndefined:
        default:
            return "VK_FORMAT_UNDEFINED";
    }
}

std::string_view to_present_mode_name(vk::PresentModeKHR mode)
{
    switch (mode) {
        case vk::PresentModeKHR::eImmediate:
            return "VK_PRESENT_MODE_IMMEDIATE_KHR";
        case vk::PresentModeKHR::eMailbox:
            return "VK_PRESENT_MODE_MAILBOX_KHR";
        case vk::PresentModeKHR::eFifo:
            return "VK_PRESENT_MODE_FIFO_KHR";
        case vk::PresentModeKHR::eFifoRelaxed:
            return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
        default:
            return "VK_PRESENT_MODE_UNKNOWN";
    }
}

bool is_vsync_present_mode(vk::PresentModeKHR mode)
{
    return mode == vk::PresentModeKHR::eFifo || mode == vk::PresentModeKHR::eFifoRelaxed;
}

vk::ImageLayout to_vulkan_image_layout(luna::ImageLayout layout)
{
    switch (layout) {
        case luna::ImageLayout::General:
            return vk::ImageLayout::eGeneral;
        case luna::ImageLayout::ColorAttachment:
            return vk::ImageLayout::eColorAttachmentOptimal;
        case luna::ImageLayout::DepthStencilAttachment:
            return vk::ImageLayout::eDepthAttachmentOptimal;
        case luna::ImageLayout::TransferSrc:
            return vk::ImageLayout::eTransferSrcOptimal;
        case luna::ImageLayout::TransferDst:
            return vk::ImageLayout::eTransferDstOptimal;
        case luna::ImageLayout::ShaderReadOnly:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        case luna::ImageLayout::Present:
            return vk::ImageLayout::ePresentSrcKHR;
        case luna::ImageLayout::Undefined:
        default:
            return vk::ImageLayout::eUndefined;
    }
}

vk::PipelineStageFlags2 to_vulkan_pipeline_stages(PipelineStage stages)
{
    const uint32_t bits = static_cast<uint32_t>(stages);
    if (bits == static_cast<uint32_t>(PipelineStage::AllCommands)) {
        return vk::PipelineStageFlagBits2::eAllCommands;
    }

    vk::PipelineStageFlags2 flags{};
    if ((bits & static_cast<uint32_t>(PipelineStage::Top)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eTopOfPipe;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::DrawIndirect)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eDrawIndirect;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::VertexInput)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eVertexInput;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::VertexShader)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eVertexShader;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::FragmentShader)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eFragmentShader;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::ComputeShader)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eComputeShader;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::ColorAttachmentOutput)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::Transfer)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eTransfer;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::Host)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eHost;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::Bottom)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eBottomOfPipe;
    }

    return flags;
}

vk::AccessFlags2 to_vulkan_access_flags(ResourceAccess access)
{
    vk::AccessFlags2 flags{};
    const uint32_t bits = static_cast<uint32_t>(access);

    if ((bits & static_cast<uint32_t>(ResourceAccess::IndirectCommandRead)) != 0) {
        flags |= vk::AccessFlagBits2::eIndirectCommandRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::VertexBufferRead)) != 0) {
        flags |= vk::AccessFlagBits2::eVertexAttributeRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::IndexBufferRead)) != 0) {
        flags |= vk::AccessFlagBits2::eIndexRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::UniformRead)) != 0) {
        flags |= vk::AccessFlagBits2::eUniformRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::ShaderRead)) != 0) {
        flags |= vk::AccessFlagBits2::eShaderRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::ShaderWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eShaderWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::ColorAttachmentRead)) != 0) {
        flags |= vk::AccessFlagBits2::eColorAttachmentRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::ColorAttachmentWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eColorAttachmentWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::DepthStencilRead)) != 0) {
        flags |= vk::AccessFlagBits2::eDepthStencilAttachmentRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::DepthStencilWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::TransferRead)) != 0) {
        flags |= vk::AccessFlagBits2::eTransferRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::TransferWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eTransferWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::HostRead)) != 0) {
        flags |= vk::AccessFlagBits2::eHostRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::HostWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eHostWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::MemoryRead)) != 0) {
        flags |= vk::AccessFlagBits2::eMemoryRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::MemoryWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eMemoryWrite;
    }

    return flags;
}

bool validate_stage_access_pair(PipelineStage stages, ResourceAccess access)
{
    if (stages == PipelineStage::None) {
        return access == ResourceAccess::None;
    }
    return true;
}

vk::IndexType to_vulkan_index_type(IndexFormat format)
{
    return format == IndexFormat::UInt16 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
}

vk::AttachmentLoadOp to_vulkan_attachment_load_op(AttachmentLoadOp op)
{
    switch (op) {
        case AttachmentLoadOp::Load:
            return vk::AttachmentLoadOp::eLoad;
        case AttachmentLoadOp::Discard:
            return vk::AttachmentLoadOp::eDontCare;
        case AttachmentLoadOp::Clear:
        default:
            return vk::AttachmentLoadOp::eClear;
    }
}

vk::AttachmentStoreOp to_vulkan_attachment_store_op(AttachmentStoreOp op)
{
    switch (op) {
        case AttachmentStoreOp::Discard:
            return vk::AttachmentStoreOp::eDontCare;
        case AttachmentStoreOp::Store:
        default:
            return vk::AttachmentStoreOp::eStore;
    }
}

vk::Extent2D mip_extent(const ImageDesc& desc, uint32_t baseMipLevel)
{
    return {
        std::max(1u, desc.width >> baseMipLevel),
        std::max(1u, desc.height >> baseMipLevel),
    };
}

PFN_vkCmdBeginDebugUtilsLabelEXT begin_debug_utils_label_fn(vk::Device device)
{
    return reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(static_cast<VkDevice>(device), "vkCmdBeginDebugUtilsLabelEXT"));
}

PFN_vkCmdEndDebugUtilsLabelEXT end_debug_utils_label_fn(vk::Device device)
{
    return reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(static_cast<VkDevice>(device), "vkCmdEndDebugUtilsLabelEXT"));
}

std::optional<std::vector<uint32_t>> load_spirv_code(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || (fileSize % sizeof(uint32_t)) != 0) {
        return std::nullopt;
    }

    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
    return buffer;
}

uint64_t reflection_binding_key(uint32_t setIndex, uint32_t binding)
{
    return (static_cast<uint64_t>(setIndex) << 32u) | static_cast<uint64_t>(binding);
}

bool shader_visibility_contains(ShaderType actual, ShaderType required)
{
    return (static_cast<uint32_t>(actual) & static_cast<uint32_t>(required)) == static_cast<uint32_t>(required);
}

std::string_view resource_type_label(ResourceType type)
{
    switch (type) {
        case ResourceType::UniformBuffer:
            return "UniformBuffer";
        case ResourceType::DynamicUniformBuffer:
            return "DynamicUniformBuffer";
        case ResourceType::CombinedImageSampler:
            return "CombinedImageSampler";
        case ResourceType::SampledImage:
            return "SampledImage";
        case ResourceType::Sampler:
            return "Sampler";
        case ResourceType::StorageBuffer:
            return "StorageBuffer";
        case ResourceType::StorageImage:
            return "StorageImage";
        case ResourceType::InputAttachment:
            return "InputAttachment";
        default:
            return "Unknown";
    }
}

std::string_view shader_stage_label(ShaderType stage)
{
    switch (stage) {
        case ShaderType::Vertex:
            return "Vertex";
        case ShaderType::Fragment:
            return "Fragment";
        case ShaderType::Compute:
            return "Compute";
        default:
            return "Unknown";
    }
}

struct ReflectedBindingInfo {
    uint32_t setIndex = 0;
    uint32_t binding = 0;
    ResourceType type = ResourceType::UniformBuffer;
    uint32_t count = 1;
    uint32_t size = 0;
    ShaderType visibility = ShaderType::None;
    std::string name;
};

bool merge_reflection_map(const Shader::ReflectionMap& reflection,
                          ShaderType stage,
                          std::map<uint64_t, ReflectedBindingInfo>* merged,
                          std::string* outError)
{
    if (merged == nullptr) {
        return false;
    }

    for (const auto& [setIndex, bindings] : reflection) {
        for (const ShaderReflectionData& binding : bindings) {
            const uint64_t key = reflection_binding_key(setIndex, binding.binding);
            auto [it, inserted] = merged->try_emplace(key,
                                                      ReflectedBindingInfo{.setIndex = setIndex,
                                                                           .binding = binding.binding,
                                                                           .type = binding.type,
                                                                           .count = binding.count,
                                                                           .size = binding.size,
                                                                           .visibility = stage,
                                                                           .name = binding.name});
            if (inserted) {
                continue;
            }

            ReflectedBindingInfo& mergedBinding = it->second;
            if (mergedBinding.type != binding.type || mergedBinding.count != binding.count) {
                if (outError != nullptr) {
                    std::ostringstream builder;
                    builder << "Reflection merge failed for set " << setIndex << " binding " << binding.binding
                            << ": expected type=" << resource_type_label(mergedBinding.type)
                            << " count=" << mergedBinding.count << ", got type=" << resource_type_label(binding.type)
                            << " count=" << binding.count << " from " << shader_stage_label(stage) << " shader.";
                    *outError = builder.str();
                }
                return false;
            }

            mergedBinding.visibility = mergedBinding.visibility | stage;
            if (mergedBinding.size == 0) {
                mergedBinding.size = binding.size;
            }
            if (mergedBinding.name.empty()) {
                mergedBinding.name = binding.name;
            }
        }
    }

    return true;
}

bool validate_layouts_against_reflection(std::span<const ResourceLayoutDesc* const> layoutDescs,
                                         const std::map<uint64_t, ReflectedBindingInfo>& reflectedBindings,
                                         std::string* outError)
{
    std::map<uint64_t, const ResourceBindingDesc*> declaredBindings;
    for (const ResourceLayoutDesc* layoutDesc : layoutDescs) {
        if (layoutDesc == nullptr) {
            continue;
        }

        for (const ResourceBindingDesc& binding : layoutDesc->bindings) {
            const uint64_t key = reflection_binding_key(layoutDesc->setIndex, binding.binding);
            auto [declaredIt, inserted] = declaredBindings.emplace(key, &binding);
            if (!inserted) {
                if (outError != nullptr) {
                    std::ostringstream builder;
                    builder << "Layout validation failed: duplicate layout binding set " << layoutDesc->setIndex
                            << " binding " << binding.binding << ".";
                    *outError = builder.str();
                }
                return false;
            }

            const auto reflectedIt = reflectedBindings.find(key);
            if (reflectedIt == reflectedBindings.end()) {
                if (outError != nullptr) {
                    std::ostringstream builder;
                    builder << "Layout validation failed: set " << layoutDesc->setIndex << " binding "
                            << binding.binding << " is not declared by shader reflection.";
                    *outError = builder.str();
                }
                return false;
            }

            const ReflectedBindingInfo& reflected = reflectedIt->second;
            if (binding.type != reflected.type || binding.count != reflected.count) {
                if (outError != nullptr) {
                    std::ostringstream builder;
                    builder << "Layout validation failed: set " << layoutDesc->setIndex << " binding "
                            << binding.binding << " expects type=" << resource_type_label(reflected.type)
                            << " count=" << reflected.count << ", but layout declared type="
                            << resource_type_label(binding.type) << " count=" << binding.count << ".";
                    *outError = builder.str();
                }
                return false;
            }

            if (!shader_visibility_contains(binding.visibility, reflected.visibility)) {
                if (outError != nullptr) {
                    std::ostringstream builder;
                    builder << "Layout validation failed: set " << layoutDesc->setIndex << " binding "
                            << binding.binding << " visibility does not cover reflected stages.";
                    *outError = builder.str();
                }
                return false;
            }
        }
    }

    for (const auto& [key, reflected] : reflectedBindings) {
        if (!declaredBindings.contains(key)) {
            if (outError != nullptr) {
                std::ostringstream builder;
                builder << "Layout validation failed: missing reflected binding set " << reflected.setIndex
                        << " binding " << reflected.binding << " (" << resource_type_label(reflected.type) << ").";
                *outError = builder.str();
            }
            return false;
        }
    }

    return true;
}

const auto& descriptor_pool_ratios()
{
    static const std::array<DescriptorAllocatorGrowable::PoolSizeRatio, 7> kRatios = {
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eUniformBuffer, 4.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eUniformBufferDynamic, 2.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eCombinedImageSampler, 4.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eSampledImage, 2.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eSampler, 2.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eStorageBuffer, 2.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eStorageImage, 2.0f},
    };
    return kRatios;
}

} // namespace

#include "detail/vk_rhi_device_classes.inl"
#include "detail/vk_rhi_device_device.inl"
#include "detail/vk_rhi_device_frame.inl"
#include "detail/vk_rhi_device_command_context.inl"
#include "detail/vk_rhi_device_tail.inl"

} // namespace luna

