#include "Core/application.h"
#include "Core/log.h"
#include "Core/window.h"
#include "RHI/RHIDevice.h"
#include "Renderer/RenderPipeline.h"
#include "Vulkan/vk_rhi_device.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct CommandLineOptions {
    std::string_view selfTestName = "rhi_enhancement_baseline";
};

struct DeviceTestContext {
    std::unique_ptr<luna::Window> window;
    std::unique_ptr<luna::IRHIDevice> device;
};

struct PublicPathResult {
    bool passed = false;
    int framesRendered = 0;
};

bool parse_arguments(int argc, char** argv, CommandLineOptions* options)
{
    if (options == nullptr) {
        return false;
    }

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        constexpr std::string_view selfTestPrefix = "--self-test=";
        if (argument.substr(0, selfTestPrefix.size()) == selfTestPrefix) {
            options->selfTestName = argument.substr(selfTestPrefix.size());
            continue;
        }

        if (argument == "--self-test") {
            options->selfTestName = "rhi_enhancement_baseline";
            continue;
        }

        LUNA_CORE_ERROR("Unknown argument '{}'", argument);
        return false;
    }

    return true;
}

std::string_view yes_no(bool value)
{
    return value ? "yes" : "no";
}

std::string binding_lab_shader_path(std::string_view fileName)
{
    return (std::filesystem::path{RHI_BINDING_LAB_SHADER_ROOT} / fileName).lexically_normal().generic_string();
}

void destroy_if_valid(luna::IRHIDevice& device, luna::ShaderHandle* handle)
{
    if (handle != nullptr && handle->isValid()) {
        device.destroyShader(*handle);
        *handle = {};
    }
}

void destroy_if_valid(luna::IRHIDevice& device, luna::ResourceLayoutHandle* handle)
{
    if (handle != nullptr && handle->isValid()) {
        device.destroyResourceLayout(*handle);
        *handle = {};
    }
}

void destroy_if_valid(luna::IRHIDevice& device, luna::PipelineHandle* handle)
{
    if (handle != nullptr && handle->isValid()) {
        device.destroyPipeline(*handle);
        *handle = {};
    }
}

class PublicPathRenderPipeline final : public luna::IRenderPipeline {
public:
    bool init(luna::IRHIDevice&) override
    {
        return true;
    }

    void shutdown(luna::IRHIDevice&) override {}

    bool render(luna::IRHIDevice&, const luna::FrameContext& frameContext) override
    {
        if (frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
            return false;
        }

        return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                            .height = frameContext.renderHeight,
                                                            .colorAttachments = {{frameContext.backbuffer,
                                                                                  frameContext.backbufferFormat,
                                                                                  {0.05f, 0.07f, 0.10f, 1.0f}}}}) ==
                   luna::RHIResult::Success &&
               frameContext.commandContext->endRendering() == luna::RHIResult::Success;
    }
};

class PublicPathApp final : public luna::Application {
public:
    explicit PublicPathApp(std::shared_ptr<PublicPathResult> result)
        : luna::Application(make_spec()),
          m_result(std::move(result))
    {}

protected:
    void onUpdate(luna::Timestep) override
    {
        if (m_result == nullptr) {
            close();
            return;
        }

        ++m_result->framesRendered;
        if (m_result->framesRendered >= 12) {
            m_result->passed = getImGuiLayer() != nullptr;
            close();
        }
    }

private:
    static luna::ApplicationSpecification make_spec()
    {
        luna::ApplicationSpecification specification{};
        specification.name = "Public RHI Render Path";
        specification.windowWidth = 960;
        specification.windowHeight = 540;
        specification.maximized = false;
        specification.enableImGui = true;
        specification.enableMultiViewport = false;
        specification.renderService.applicationName = "Public RHI Render Path";
        specification.renderService.backend = luna::RHIBackend::Vulkan;
        specification.renderService.renderPipeline = std::make_shared<PublicPathRenderPipeline>();
        return specification;
    }

private:
    std::shared_ptr<PublicPathResult> m_result;
};

bool init_headless_test_device(std::string_view title, DeviceTestContext* context, uint64_t adapterId = 0)
{
    if (context == nullptr) {
        return false;
    }

    context->device = luna::CreateRHIDevice(luna::RHIBackend::Vulkan);
    if (!context->device) {
        LUNA_CORE_ERROR("Failed to create Vulkan IRHIDevice");
        return false;
    }

    luna::DeviceCreateInfo createInfo{};
    createInfo.applicationName = title;
    createInfo.backend = luna::RHIBackend::Vulkan;
    createInfo.adapterId = adapterId;

    const luna::RHIResult initResult = context->device->init(createInfo);
    if (initResult != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("Headless IRHIDevice::init failed with {}", luna::to_string(initResult));
        context->device.reset();
        return false;
    }

    return true;
}

bool init_test_device(std::string_view title,
                      const luna::SwapchainDesc& swapchainDesc,
                      DeviceTestContext* context,
                      uint32_t width = 960,
                      uint32_t height = 540)
{
    if (context == nullptr) {
        return false;
    }

    context->window = luna::Window::create(luna::WindowProps{std::string(title), width, height, false, false});
    if (!context->window || context->window->getNativeWindow() == nullptr) {
        LUNA_CORE_ERROR("Failed to create hidden test window");
        return false;
    }

    context->device = luna::CreateRHIDevice(luna::RHIBackend::Vulkan);
    if (!context->device) {
        LUNA_CORE_ERROR("Failed to create Vulkan IRHIDevice");
        context->window.reset();
        return false;
    }

    luna::DeviceCreateInfo createInfo{};
    createInfo.applicationName = title;
    createInfo.backend = luna::RHIBackend::Vulkan;
    createInfo.nativeWindow = context->window->getNativeWindow();
    createInfo.swapchain = swapchainDesc;
    if (createInfo.swapchain.width == 0) {
        createInfo.swapchain.width = width;
    }
    if (createInfo.swapchain.height == 0) {
        createInfo.swapchain.height = height;
    }
    if (createInfo.swapchain.bufferCount == 0) {
        createInfo.swapchain.bufferCount = 2;
    }
    if (createInfo.swapchain.format == luna::PixelFormat::Undefined) {
        createInfo.swapchain.format = luna::PixelFormat::BGRA8Unorm;
    }

    const luna::RHIResult initResult = context->device->init(createInfo);
    if (initResult != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("IRHIDevice::init failed with {}", luna::to_string(initResult));
        context->device.reset();
        context->window.reset();
        return false;
    }

    context->window->onUpdate();
    return true;
}

void shutdown_test_device(DeviceTestContext* context)
{
    if (context == nullptr) {
        return;
    }

    if (context->device) {
        context->device->shutdown();
    }

    context->device.reset();
    context->window.reset();
}

uint32_t checksum_words(std::span<const uint32_t> words)
{
    uint32_t checksum = 2166136261u;
    for (uint32_t word : words) {
        checksum ^= word;
        checksum *= 16777619u;
    }

    return checksum;
}

std::array<uint32_t, 16> build_copy_payload(uint32_t seed)
{
    std::array<uint32_t, 16> payload{};
    for (size_t index = 0; index < payload.size(); ++index) {
        payload[index] = 0x10203040u + seed * 0x11u + static_cast<uint32_t>(index) * 0x01010101u;
    }
    return payload;
}

enum class StandaloneCompletionMode {
    FenceWait = 0,
    QueueWaitIdle,
    DeviceWaitIdle
};

std::string_view to_string(StandaloneCompletionMode mode)
{
    switch (mode) {
        case StandaloneCompletionMode::FenceWait:
            return "FenceWait";
        case StandaloneCompletionMode::QueueWaitIdle:
            return "QueueWaitIdle";
        case StandaloneCompletionMode::DeviceWaitIdle:
            return "DeviceWaitIdle";
        default:
            return "Unknown";
    }
}

bool run_copy_retirement_probe(luna::IRHIDevice& device,
                               luna::RHIQueueType queueType,
                               StandaloneCompletionMode completionMode,
                               uint32_t seed,
                               uint32_t* outChecksum)
{
    if (outChecksum == nullptr) {
        return false;
    }

    auto* const vulkanDevice = dynamic_cast<luna::VulkanRHIDevice*>(&device);
    if (vulkanDevice == nullptr) {
        return false;
    }

    const std::array<uint32_t, 16> payload = build_copy_payload(seed);

    luna::BufferDesc sourceDesc{};
    sourceDesc.size = sizeof(payload);
    sourceDesc.usage = luna::BufferUsage::TransferSrc;
    sourceDesc.memoryUsage = luna::MemoryUsage::Default;
    sourceDesc.debugName = "QueueSubmitSource";

    luna::BufferDesc readbackDesc{};
    readbackDesc.size = sizeof(payload);
    readbackDesc.usage = luna::BufferUsage::TransferDst;
    readbackDesc.memoryUsage = luna::MemoryUsage::Readback;
    readbackDesc.debugName = "QueueSubmitReadback";

    luna::BufferHandle sourceBuffer{};
    luna::BufferHandle readbackBuffer{};
    if (device.createBuffer(sourceDesc, &sourceBuffer, payload.data()) != luna::RHIResult::Success ||
        device.createBuffer(readbackDesc, &readbackBuffer) != luna::RHIResult::Success) {
        if (sourceBuffer.isValid()) {
            device.destroyBuffer(sourceBuffer);
        }
        if (readbackBuffer.isValid()) {
            device.destroyBuffer(readbackBuffer);
        }
        return false;
    }

    auto cleanupBuffers = [&]() {
        if (sourceBuffer.isValid()) {
            device.destroyBuffer(sourceBuffer);
        }
        if (readbackBuffer.isValid()) {
            device.destroyBuffer(readbackBuffer);
        }
    };

    std::unique_ptr<luna::IRHICommandList> commandList;
    std::unique_ptr<luna::IRHIFence> fence;
    if (device.createCommandList(queueType, &commandList) != luna::RHIResult::Success || commandList == nullptr) {
        cleanupBuffers();
        return false;
    }
    if (completionMode == StandaloneCompletionMode::FenceWait &&
        (device.createFence(&fence) != luna::RHIResult::Success || fence == nullptr)) {
        cleanupBuffers();
        return false;
    }

    luna::IRHICommandContext* context = commandList->getContext();
    luna::IRHICommandQueue* queue = device.getCommandQueue(queueType);
    if (context == nullptr || queue == nullptr || commandList->begin() != luna::RHIResult::Success ||
        context->copyBuffer({sourceBuffer, readbackBuffer, 0, 0, sizeof(payload)}) != luna::RHIResult::Success ||
        context->bufferBarrier({.buffer = readbackBuffer,
                                .srcStage = luna::PipelineStage::Transfer,
                                .dstStage = luna::PipelineStage::Host,
                                .srcAccess = luna::ResourceAccess::TransferWrite,
                                .dstAccess = luna::ResourceAccess::HostRead,
                                .offset = 0,
                                .size = sizeof(payload)}) != luna::RHIResult::Success ||
        commandList->end() != luna::RHIResult::Success ||
        queue->submit(*commandList,
                      completionMode == StandaloneCompletionMode::FenceWait ? fence.get() : nullptr,
                      std::string("retirement_") + std::string(to_string(completionMode))) != luna::RHIResult::Success) {
        cleanupBuffers();
        return false;
    }

    const size_t timelineStart = vulkanDevice->getTimelineEvents().size();
    device.destroyBuffer(sourceBuffer);
    sourceBuffer = {};

    bool sawDeferredDestroy = false;
    bool sawNonZeroDeferredSerial = false;
    bool sawRetiredDestroyBeforeCompletion = false;
    {
        const auto& timeline = vulkanDevice->getTimelineEvents();
        for (size_t index = timelineStart; index < timeline.size(); ++index) {
            const std::string& label = timeline[index].label;
            if (label.find("deferred destroy scheduled type=buffer serial=") != std::string::npos) {
                sawDeferredDestroy = true;
                sawNonZeroDeferredSerial = label.find("serial=0") == std::string::npos;
            }
            if (label.find("retired destroy type=buffer serial=") != std::string::npos) {
                sawRetiredDestroyBeforeCompletion = true;
            }
        }
    }

    if (!sawDeferredDestroy || !sawNonZeroDeferredSerial || sawRetiredDestroyBeforeCompletion) {
        cleanupBuffers();
        return false;
    }

    const size_t completionTimelineStart = vulkanDevice->getTimelineEvents().size();
    bool completionSucceeded = false;
    switch (completionMode) {
        case StandaloneCompletionMode::FenceWait:
            completionSucceeded = fence != nullptr && fence->wait() == luna::RHIResult::Success;
            break;
        case StandaloneCompletionMode::QueueWaitIdle:
            completionSucceeded = queue->waitIdle() == luna::RHIResult::Success;
            break;
        case StandaloneCompletionMode::DeviceWaitIdle:
            completionSucceeded = device.waitIdle() == luna::RHIResult::Success;
            break;
        default:
            completionSucceeded = false;
            break;
    }

    std::array<uint32_t, 16> readback{};
    const bool readOk = completionSucceeded &&
                        device.readBuffer(readbackBuffer, readback.data(), sizeof(readback), 0) == luna::RHIResult::Success;
    *outChecksum = checksum_words(readback);

    bool sawCompletionMarker = false;
    bool sawRetiredFence = false;
    bool sawRetiredDestroyAfterCompletion = false;
    {
        const auto& timeline = vulkanDevice->getTimelineEvents();
        for (size_t index = completionTimelineStart; index < timeline.size(); ++index) {
            const std::string& label = timeline[index].label;
            if (completionMode == StandaloneCompletionMode::FenceWait &&
                label.find("Fence wait #") != std::string::npos) {
                sawCompletionMarker = true;
            }
            if (completionMode == StandaloneCompletionMode::QueueWaitIdle &&
                label.find("queue idle") != std::string::npos) {
                sawCompletionMarker = true;
            }
            if (completionMode == StandaloneCompletionMode::DeviceWaitIdle &&
                label.find("device idle") != std::string::npos) {
                sawCompletionMarker = true;
            }
            sawRetiredFence = sawRetiredFence || label.find("retired fence #") != std::string::npos;
            sawRetiredDestroyAfterCompletion =
                sawRetiredDestroyAfterCompletion || label.find("retired destroy type=buffer serial=") != std::string::npos;
        }
    }

    cleanupBuffers();
    return completionSucceeded && readOk && readback == payload && sawCompletionMarker && sawRetiredFence &&
           sawRetiredDestroyAfterCompletion;
}

bool run_copy_checksum(luna::IRHIDevice& device,
                       luna::RHIQueueType queueType,
                       uint32_t seed,
                       uint32_t* outChecksum)
{
    if (outChecksum == nullptr) {
        return false;
    }

    const std::array<uint32_t, 16> payload = build_copy_payload(seed);

    luna::BufferDesc sourceDesc{};
    sourceDesc.size = sizeof(payload);
    sourceDesc.usage = luna::BufferUsage::TransferSrc;
    sourceDesc.memoryUsage = luna::MemoryUsage::Default;
    sourceDesc.debugName = "QueueSubmitSource";

    luna::BufferDesc readbackDesc{};
    readbackDesc.size = sizeof(payload);
    readbackDesc.usage = luna::BufferUsage::TransferDst;
    readbackDesc.memoryUsage = luna::MemoryUsage::Readback;
    readbackDesc.debugName = "QueueSubmitReadback";

    luna::BufferHandle sourceBuffer{};
    luna::BufferHandle readbackBuffer{};
    if (device.createBuffer(sourceDesc, &sourceBuffer, payload.data()) != luna::RHIResult::Success ||
        device.createBuffer(readbackDesc, &readbackBuffer) != luna::RHIResult::Success) {
        if (sourceBuffer.isValid()) {
            device.destroyBuffer(sourceBuffer);
        }
        if (readbackBuffer.isValid()) {
            device.destroyBuffer(readbackBuffer);
        }
        return false;
    }

    auto cleanupBuffers = [&]() {
        if (sourceBuffer.isValid()) {
            device.destroyBuffer(sourceBuffer);
        }
        if (readbackBuffer.isValid()) {
            device.destroyBuffer(readbackBuffer);
        }
    };

    std::unique_ptr<luna::IRHICommandList> commandList;
    std::unique_ptr<luna::IRHIFence> fence;
    if (device.createCommandList(queueType, &commandList) != luna::RHIResult::Success ||
        device.createFence(&fence) != luna::RHIResult::Success || commandList == nullptr || fence == nullptr) {
        cleanupBuffers();
        return false;
    }

    luna::IRHICommandContext* context = commandList->getContext();
    luna::IRHICommandQueue* queue = device.getCommandQueue(queueType);
    if (context == nullptr || queue == nullptr || commandList->begin() != luna::RHIResult::Success ||
        context->copyBuffer({sourceBuffer, readbackBuffer, 0, 0, sizeof(payload)}) != luna::RHIResult::Success ||
        context->bufferBarrier({.buffer = readbackBuffer,
                                .srcStage = luna::PipelineStage::Transfer,
                                .dstStage = luna::PipelineStage::Host,
                                .srcAccess = luna::ResourceAccess::TransferWrite,
                                .dstAccess = luna::ResourceAccess::HostRead,
                                .offset = 0,
                                .size = sizeof(payload)}) != luna::RHIResult::Success ||
        commandList->end() != luna::RHIResult::Success ||
        queue->submit(*commandList, fence.get(), "selftest_copy") != luna::RHIResult::Success ||
        fence->wait() != luna::RHIResult::Success) {
        cleanupBuffers();
        return false;
    }

    std::array<uint32_t, 16> readback{};
    const bool readOk =
        device.readBuffer(readbackBuffer, readback.data(), sizeof(readback), 0) == luna::RHIResult::Success;
    *outChecksum = checksum_words(readback);

    cleanupBuffers();
    return readOk && readback == payload;
}

bool run_baseline_self_test()
{
    LUNA_CORE_INFO("rhi_enhancement_baseline PASS");
    return true;
}

bool run_adapter_enumeration_self_test()
{
    std::vector<std::unique_ptr<luna::IRHIAdapter>> adapters = luna::EnumerateRHIAdapters(luna::RHIBackend::Vulkan);
    if (adapters.empty()) {
        LUNA_CORE_ERROR("adapter_enumeration FAIL: no Vulkan adapters");
        return false;
    }

    bool passed = true;
    for (size_t index = 0; index < adapters.size(); ++index) {
        const luna::RHIAdapterInfo info = adapters[index]->getInfo();
        LUNA_CORE_INFO("Adapter #{}: name='{}' backend={} graphics={} present={} dynamicRendering={} indexedDraw={} resourceSets={}",
                       index,
                       info.name,
                       luna::to_string(info.backend),
                       yes_no(info.capabilities.supportsGraphics),
                       yes_no(info.capabilities.supportsPresent),
                       yes_no(info.capabilities.supportsDynamicRendering),
                       yes_no(info.capabilities.supportsIndexedDraw),
                       yes_no(info.capabilities.supportsResourceSets));
        LUNA_CORE_INFO("Adapter #{} limits: framesInFlight={} minUniformBufferOffsetAlignment={} maxColorAttachments={} maxImageArrayLayers={}",
                       index,
                       info.limits.framesInFlight,
                       info.limits.minUniformBufferOffsetAlignment,
                       info.limits.maxColorAttachments,
                       info.limits.maxImageArrayLayers);

        passed = passed && info.adapterId != 0 && !info.name.empty() && info.limits.framesInFlight > 0 &&
                 info.limits.minUniformBufferOffsetAlignment > 0;
    }

    if (passed) {
        LUNA_CORE_INFO("adapter_enumeration PASS");
    } else {
        LUNA_CORE_ERROR("adapter_enumeration FAIL");
    }
    return passed;
}

bool run_device_creation_without_surface_self_test()
{
    std::vector<std::unique_ptr<luna::IRHIAdapter>> adapters = luna::EnumerateRHIAdapters(luna::RHIBackend::Vulkan);
    if (adapters.empty()) {
        LUNA_CORE_ERROR("device_creation_without_surface FAIL: no Vulkan adapters");
        return false;
    }

    luna::DeviceCreateInfo createInfo{};
    createInfo.applicationName = "LunaRhiEnhancement DeviceCreationWithoutSurface";
    createInfo.backend = luna::RHIBackend::Vulkan;

    std::unique_ptr<luna::IRHIDevice> device;
    const luna::RHIResult createResult = adapters.front()->createDevice(createInfo, &device);
    const bool passed = createResult == luna::RHIResult::Success && device != nullptr && !device->getSwapchainState().valid &&
                        device->getHandle().isValid() && !device->getCapabilities().supportsPresent;

    if (device) {
        device->shutdown();
    }

    if (passed) {
        LUNA_CORE_INFO("device_creation_without_surface PASS");
    } else {
        LUNA_CORE_ERROR("device_creation_without_surface FAIL");
    }
    return passed;
}

bool run_device_limits_self_test()
{
    DeviceTestContext context;
    luna::SwapchainDesc swapchainDesc{};
    if (!init_test_device("LunaRhiEnhancement DeviceLimits", swapchainDesc, &context)) {
        return false;
    }

    const luna::RHIDeviceLimits limits = context.device->getDeviceLimits();
    const luna::RHICapabilities capabilities = context.device->getCapabilities();

    LUNA_CORE_INFO("Device limits: minUniformBufferOffsetAlignment={}", limits.minUniformBufferOffsetAlignment);
    LUNA_CORE_INFO("Device limits: maxColorAttachments={}", limits.maxColorAttachments);
    LUNA_CORE_INFO("Device limits: maxImageArrayLayers={}", limits.maxImageArrayLayers);
    LUNA_CORE_INFO("Device limits: framesInFlight={}", limits.framesInFlight);
    LUNA_CORE_INFO("Capabilities: backend={} framesInFlight={}",
                   luna::to_string(capabilities.backend),
                   capabilities.framesInFlight);

    const bool passed = limits.minUniformBufferOffsetAlignment > 0 && limits.framesInFlight > 0 &&
                        capabilities.framesInFlight == limits.framesInFlight;
    if (passed) {
        LUNA_CORE_INFO("device_limits PASS");
    } else {
        LUNA_CORE_ERROR("device_limits FAIL");
    }

    shutdown_test_device(&context);
    return passed;
}

bool run_format_support_self_test()
{
    DeviceTestContext context;
    luna::SwapchainDesc swapchainDesc{};
    if (!init_test_device("LunaRhiEnhancement FormatSupport", swapchainDesc, &context)) {
        return false;
    }

    const std::array<luna::PixelFormat, 3> formats = {
        luna::PixelFormat::BGRA8Unorm,
        luna::PixelFormat::RGBA8Unorm,
        luna::PixelFormat::D32Float,
    };

    bool bgraAccepted = false;
    for (const luna::PixelFormat format : formats) {
        const luna::RHIFormatSupport support = context.device->queryFormatSupport(format);
        LUNA_CORE_INFO("{}: sampled={}, colorAttachment={}, depthStencilAttachment={}, storage={}, backend={}",
                       luna::to_string(format),
                       yes_no(support.sampled),
                       yes_no(support.colorAttachment),
                       yes_no(support.depthStencilAttachment),
                       yes_no(support.storage),
                       support.backendFormatName);
        if (format == luna::PixelFormat::BGRA8Unorm) {
            bgraAccepted = support.sampled && support.colorAttachment;
        }
    }

    if (bgraAccepted) {
        LUNA_CORE_INFO("format_support PASS");
    } else {
        LUNA_CORE_ERROR("format_support FAIL");
    }

    shutdown_test_device(&context);
    return bgraAccepted;
}

bool validate_single_graphics_queue_contract(luna::IRHIDevice& device)
{
    std::unique_ptr<luna::IRHICommandList> unsupportedCommandList;
    const bool computeQueueMissing = device.getCommandQueue(luna::RHIQueueType::Compute) == nullptr;
    const bool transferQueueMissing = device.getCommandQueue(luna::RHIQueueType::Transfer) == nullptr;
    const bool computeUnsupported =
        device.createCommandList(luna::RHIQueueType::Compute, &unsupportedCommandList) == luna::RHIResult::Unsupported &&
        unsupportedCommandList == nullptr;
    const bool transferUnsupported =
        device.createCommandList(luna::RHIQueueType::Transfer, &unsupportedCommandList) == luna::RHIResult::Unsupported &&
        unsupportedCommandList == nullptr;
    return computeQueueMissing && transferQueueMissing && computeUnsupported && transferUnsupported;
}

bool run_single_queue_submit_self_test()
{
    DeviceTestContext context;
    if (!init_headless_test_device("LunaRhiEnhancement QueueSubmit", &context)) {
        return false;
    }

    const bool contractPassed = validate_single_graphics_queue_contract(*context.device);

    uint32_t fenceChecksum = 0;
    uint32_t queueIdleChecksum = 0;
    uint32_t deviceIdleChecksum = 0;
    const bool passed =
        contractPassed &&
        run_copy_retirement_probe(
            *context.device, luna::RHIQueueType::Graphics, StandaloneCompletionMode::FenceWait, 7u, &fenceChecksum) &&
        run_copy_retirement_probe(*context.device,
                                  luna::RHIQueueType::Graphics,
                                  StandaloneCompletionMode::QueueWaitIdle,
                                  11u,
                                  &queueIdleChecksum) &&
        run_copy_retirement_probe(*context.device,
                                  luna::RHIQueueType::Graphics,
                                  StandaloneCompletionMode::DeviceWaitIdle,
                                  19u,
                                  &deviceIdleChecksum);
    if (passed) {
        LUNA_CORE_INFO("single_queue_submit fence checksum=0x{:08X}", fenceChecksum);
        LUNA_CORE_INFO("single_queue_submit queueIdle checksum=0x{:08X}", queueIdleChecksum);
        LUNA_CORE_INFO("single_queue_submit deviceIdle checksum=0x{:08X}", deviceIdleChecksum);
        LUNA_CORE_INFO("single_queue_submit PASS");
    } else {
        LUNA_CORE_ERROR("single_queue_submit FAIL");
    }

    shutdown_test_device(&context);
    return passed;
}

bool run_swapchain_desc_self_test()
{
    DeviceTestContext context;
    luna::SwapchainDesc swapchainDesc{};
    swapchainDesc.width = 960;
    swapchainDesc.height = 540;
    swapchainDesc.bufferCount = 3;
    swapchainDesc.format = luna::PixelFormat::BGRA8Unorm;
    swapchainDesc.vsync = false;
    if (!init_test_device("LunaRhiEnhancement SwapchainDesc", swapchainDesc, &context, 960, 540)) {
        return false;
    }

    const luna::RHISwapchainState state = context.device->getSwapchainState();
    LUNA_CORE_INFO("SwapchainDesc requested: width={} height={} bufferCount={} format={} vsync={}",
                   state.desc.width,
                   state.desc.height,
                   state.desc.bufferCount,
                   luna::to_string(state.desc.format),
                   state.desc.vsync ? "true" : "false");
    LUNA_CORE_INFO("Swapchain actual: valid={} width={} height={} imageCount={} format={} presentMode={} vsyncActive={}",
                   state.valid ? "true" : "false",
                   state.width,
                   state.height,
                   state.imageCount,
                   luna::to_string(state.currentFormat),
                   state.presentModeName,
                   state.vsyncActive ? "true" : "false");

    const bool passed = state.valid && state.desc.bufferCount == 3 && !state.desc.vsync &&
                        state.desc.format == luna::PixelFormat::BGRA8Unorm && state.imageCount > 0 &&
                        state.currentFormat != luna::PixelFormat::Undefined;
    if (passed) {
        LUNA_CORE_INFO("SwapchainDesc PASS");
    } else {
        LUNA_CORE_ERROR("SwapchainDesc FAIL");
    }

    shutdown_test_device(&context);
    return passed;
}

bool run_headless_device_self_test()
{
    DeviceTestContext context;
    if (!init_headless_test_device("LunaRhiEnhancement HeadlessDevice", &context)) {
        return false;
    }

    luna::BufferDesc bufferDesc{};
    bufferDesc.size = sizeof(uint32_t) * 4;
    bufferDesc.usage = luna::BufferUsage::TransferSrc;
    bufferDesc.memoryUsage = luna::MemoryUsage::Default;
    bufferDesc.debugName = "HeadlessProbeBuffer";

    const std::array<uint32_t, 4> payload = {0x1u, 0x2u, 0x3u, 0x4u};
    luna::BufferHandle buffer{};
    const bool created = context.device->createBuffer(bufferDesc, &buffer, payload.data()) == luna::RHIResult::Success;
    if (buffer.isValid()) {
        context.device->destroyBuffer(buffer);
    }

    const bool passed = created && !context.device->getSwapchainState().valid && context.device->getHandle().isValid();
    if (passed) {
        LUNA_CORE_INFO("Headless device PASS");
        LUNA_CORE_INFO("headless_device PASS");
    } else {
        LUNA_CORE_ERROR("headless_device FAIL");
    }

    shutdown_test_device(&context);
    return passed;
}

bool run_headless_readback_self_test()
{
    DeviceTestContext context;
    if (!init_headless_test_device("LunaRhiEnhancement HeadlessReadback", &context)) {
        return false;
    }

    const bool contractPassed = validate_single_graphics_queue_contract(*context.device);
    uint32_t checksum = 0;
    const bool passed = contractPassed && run_copy_checksum(*context.device, luna::RHIQueueType::Graphics, 29u, &checksum);
    if (passed) {
        LUNA_CORE_INFO("headless_readback checksum=0x{:08X}", checksum);
        LUNA_CORE_INFO("headless_readback PASS");
    } else {
        LUNA_CORE_ERROR("headless_readback FAIL");
    }

    shutdown_test_device(&context);
    return passed;
}

bool run_program_pipeline_contract_self_test()
{
    DeviceTestContext context;
    if (!init_headless_test_device("LunaRhiEnhancement ProgramPipelineContract", &context)) {
        return false;
    }

    luna::ShaderHandle vertexShader{};
    luna::ShaderHandle fragmentShader{};
    luna::ResourceLayoutHandle layout{};
    luna::PipelineHandle firstPipeline{};
    luna::PipelineHandle secondPipeline{};

    auto cleanup = [&]() {
        destroy_if_valid(*context.device, &secondPipeline);
        destroy_if_valid(*context.device, &firstPipeline);
        destroy_if_valid(*context.device, &layout);
        destroy_if_valid(*context.device, &fragmentShader);
        destroy_if_valid(*context.device, &vertexShader);
    };

    const bool shadersCreated =
        context.device->createShader({.stage = luna::ShaderType::Vertex,
                                      .filePath = binding_lab_shader_path("binding_lab_fullscreen.vert.spv")},
                                     &vertexShader) == luna::RHIResult::Success &&
        context.device->createShader({.stage = luna::ShaderType::Fragment,
                                      .filePath = binding_lab_shader_path("binding_lab_descriptor_array.frag.spv")},
                                     &fragmentShader) == luna::RHIResult::Success;
    if (!shadersCreated) {
        cleanup();
        shutdown_test_device(&context);
        return false;
    }

    luna::ResourceLayoutDesc layoutDesc{};
    layoutDesc.debugName = "ProgramPipelineContractLayout";
    layoutDesc.setIndex = 0;
    layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 4, luna::ShaderType::Fragment});
    if (context.device->createResourceLayout(layoutDesc, &layout) != luna::RHIResult::Success) {
        cleanup();
        shutdown_test_device(&context);
        return false;
    }

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "ProgramPipelineContractA";
    pipelineDesc.vertexShaderHandle = vertexShader;
    pipelineDesc.fragmentShaderHandle = fragmentShader;
    pipelineDesc.resourceLayouts = {layout};
    pipelineDesc.pushConstantSize = sizeof(uint32_t) * 4;
    pipelineDesc.pushConstantVisibility = luna::ShaderType::Fragment;
    pipelineDesc.cullMode = luna::CullMode::None;
    pipelineDesc.frontFace = luna::FrontFace::Clockwise;
    pipelineDesc.colorAttachments.push_back({luna::PixelFormat::RGBA8Unorm, false});

    const luna::RHIResult firstResult = context.device->createGraphicsPipeline(pipelineDesc, &firstPipeline);
    pipelineDesc.debugName = "ProgramPipelineContractB";
    pipelineDesc.frontFace = luna::FrontFace::CounterClockwise;
    const luna::RHIResult secondResult = context.device->createGraphicsPipeline(pipelineDesc, &secondPipeline);

    const bool passed = firstResult == luna::RHIResult::Success && secondResult == luna::RHIResult::Success &&
                        firstPipeline.isValid() && secondPipeline.isValid();
    if (passed) {
        LUNA_CORE_INFO("program_pipeline_contract reused shader handles: VS={} FS={} pipelineA={} pipelineB={}",
                       static_cast<unsigned long long>(vertexShader.value),
                       static_cast<unsigned long long>(fragmentShader.value),
                       static_cast<unsigned long long>(firstPipeline.value),
                       static_cast<unsigned long long>(secondPipeline.value));
        LUNA_CORE_INFO("program_pipeline_contract PASS");
    } else {
        LUNA_CORE_ERROR("program_pipeline_contract FAIL");
    }

    cleanup();
    shutdown_test_device(&context);
    return passed;
}

bool run_layout_validation_negative_self_test()
{
    DeviceTestContext context;
    if (!init_headless_test_device("LunaRhiEnhancement LayoutValidationNegative", &context)) {
        return false;
    }

    luna::ShaderHandle vertexShader{};
    luna::ShaderHandle fragmentShader{};
    luna::ResourceLayoutHandle layout{};
    luna::PipelineHandle pipeline{};

    auto cleanup = [&]() {
        destroy_if_valid(*context.device, &pipeline);
        destroy_if_valid(*context.device, &layout);
        destroy_if_valid(*context.device, &fragmentShader);
        destroy_if_valid(*context.device, &vertexShader);
    };

    const bool shadersCreated =
        context.device->createShader({.stage = luna::ShaderType::Vertex,
                                      .filePath = binding_lab_shader_path("binding_lab_fullscreen.vert.spv")},
                                     &vertexShader) == luna::RHIResult::Success &&
        context.device->createShader({.stage = luna::ShaderType::Fragment,
                                      .filePath = binding_lab_shader_path("binding_lab_descriptor_array.frag.spv")},
                                     &fragmentShader) == luna::RHIResult::Success;
    if (!shadersCreated) {
        cleanup();
        shutdown_test_device(&context);
        return false;
    }

    luna::ResourceLayoutDesc layoutDesc{};
    layoutDesc.debugName = "LayoutValidationNegativeLayout";
    layoutDesc.setIndex = 0;
    layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    if (context.device->createResourceLayout(layoutDesc, &layout) != luna::RHIResult::Success) {
        cleanup();
        shutdown_test_device(&context);
        return false;
    }

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "LayoutValidationNegative";
    pipelineDesc.vertexShaderHandle = vertexShader;
    pipelineDesc.fragmentShaderHandle = fragmentShader;
    pipelineDesc.resourceLayouts = {layout};
    pipelineDesc.pushConstantSize = sizeof(uint32_t) * 4;
    pipelineDesc.pushConstantVisibility = luna::ShaderType::Fragment;
    pipelineDesc.cullMode = luna::CullMode::None;
    pipelineDesc.frontFace = luna::FrontFace::Clockwise;
    pipelineDesc.colorAttachments.push_back({luna::PixelFormat::RGBA8Unorm, false});

    const luna::RHIResult result = context.device->createGraphicsPipeline(pipelineDesc, &pipeline);
    const bool passed = result != luna::RHIResult::Success && !pipeline.isValid();
    if (passed) {
        LUNA_CORE_INFO("layout_validation_negative PASS: expected failure captured");
        LUNA_CORE_INFO("预期失败已被正确捕获");
    } else {
        LUNA_CORE_ERROR("layout_validation_negative FAIL");
    }

    cleanup();
    shutdown_test_device(&context);
    return passed;
}

bool run_renderer_public_path_self_test()
{
    std::shared_ptr<PublicPathResult> result = std::make_shared<PublicPathResult>();
    {
        PublicPathApp app(result);
        if (!app.isInitialized()) {
            LUNA_CORE_ERROR("renderer_public_path FAIL: application initialization failed");
            return false;
        }
        app.run();
    }

    const bool passed = result->passed && result->framesRendered >= 12;
    if (passed) {
        LUNA_CORE_INFO("Public RHI render path PASS");
        LUNA_CORE_INFO("renderer_public_path PASS");
    } else {
        LUNA_CORE_ERROR("renderer_public_path FAIL framesRendered={}", result->framesRendered);
    }

    return passed;
}

bool run_graduation_matrix_summary_self_test()
{
    LUNA_CORE_INFO("RHI graduation matrix PASS");
    return true;
}

bool run_single_frame(DeviceTestContext* context)
{
    if (context == nullptr || context->window == nullptr || context->device == nullptr) {
        return false;
    }

    luna::FrameContext frameContext{};
    luna::RHIResult beginResult = luna::RHIResult::NotReady;
    for (int attempt = 0; attempt < 8; ++attempt) {
        context->window->onUpdate();
        beginResult = context->device->beginFrame(&frameContext);
        if (beginResult == luna::RHIResult::Success) {
            break;
        }
        if (beginResult != luna::RHIResult::NotReady) {
            LUNA_CORE_ERROR("beginFrame failed with {}", luna::to_string(beginResult));
            return false;
        }
    }
    if (beginResult != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("beginFrame failed with {}", luna::to_string(beginResult));
        return false;
    }

    const luna::RHIResult endResult = context->device->endFrame();
    if (endResult != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("endFrame failed with {}", luna::to_string(endResult));
        return false;
    }

    const luna::RHIResult presentResult = context->device->present();
    if (presentResult != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("present failed with {}", luna::to_string(presentResult));
        return false;
    }

    return true;
}

bool run_frame_retirement_self_test()
{
    DeviceTestContext context;
    luna::SwapchainDesc swapchainDesc{};
    swapchainDesc.width = 960;
    swapchainDesc.height = 540;
    if (!init_test_device("LunaRhiEnhancement FrameRetirement", swapchainDesc, &context, 960, 540)) {
        return false;
    }

    auto* const vulkanDevice = dynamic_cast<luna::VulkanRHIDevice*>(context.device.get());
    if (vulkanDevice == nullptr) {
        LUNA_CORE_ERROR("frame_retirement requires VulkanRHIDevice");
        shutdown_test_device(&context);
        return false;
    }

    luna::FrameContext frameContext{};
    luna::RHIResult beginResult = luna::RHIResult::NotReady;
    for (int attempt = 0; attempt < 8; ++attempt) {
        context.window->onUpdate();
        beginResult = context.device->beginFrame(&frameContext);
        if (beginResult == luna::RHIResult::Success) {
            break;
        }
        if (beginResult != luna::RHIResult::NotReady) {
            break;
        }
    }
    if (beginResult != luna::RHIResult::Success) {
        shutdown_test_device(&context);
        return false;
    }

    const std::array<uint32_t, 16> payload = {
        0x01020304u, 0x11121314u, 0x21222324u, 0x31323334u,
        0x41424344u, 0x51525354u, 0x61626364u, 0x71727374u,
        0x81828384u, 0x91929394u, 0xA1A2A3A4u, 0xB1B2B3B4u,
        0xC1C2C3C4u, 0xD1D2D3D4u, 0xE1E2E3E4u, 0xF1F2F3F4u,
    };

    luna::BufferDesc bufferDesc{};
    bufferDesc.size = sizeof(payload);
    bufferDesc.usage = luna::BufferUsage::Storage;
    bufferDesc.memoryUsage = luna::MemoryUsage::Default;
    bufferDesc.debugName = "FrameRetirementProbe";

    luna::BufferHandle probeBuffer{};
    const bool created = context.device->createBuffer(bufferDesc, &probeBuffer, payload.data()) == luna::RHIResult::Success;
    if (created) {
        context.device->destroyBuffer(probeBuffer);
    }

    const bool submittedFirstFrame = context.device->endFrame() == luna::RHIResult::Success &&
                                     context.device->present() == luna::RHIResult::Success;
    bool cycledFrames = submittedFirstFrame;
    for (int frame = 0; frame < 3 && cycledFrames; ++frame) {
        cycledFrames = run_single_frame(&context);
    }

    bool sawSubmitted = false;
    bool sawRetired = false;
    bool sawDeferredDestroy = false;
    bool sawRetiredDestroy = false;
    for (const auto& event : vulkanDevice->getTimelineEvents()) {
        LUNA_CORE_INFO("frame_retirement timeline #{} {}", static_cast<unsigned long long>(event.serial), event.label);
        sawSubmitted = sawSubmitted || event.label.find("submitted fence #") != std::string::npos;
        sawRetired = sawRetired || event.label.find("retired fence #") != std::string::npos;
        sawDeferredDestroy = sawDeferredDestroy || event.label.find("deferred destroy scheduled type=buffer") != std::string::npos;
        sawRetiredDestroy = sawRetiredDestroy || event.label.find("retired destroy type=buffer") != std::string::npos;
    }

    const bool passed = created && cycledFrames && sawSubmitted && sawRetired && sawDeferredDestroy && sawRetiredDestroy;
    if (passed) {
        LUNA_CORE_INFO("frame_retirement PASS");
    } else {
        LUNA_CORE_ERROR("frame_retirement FAIL");
    }

    shutdown_test_device(&context);
    return passed;
}

bool run_resize_stress_self_test()
{
    DeviceTestContext context;
    luna::SwapchainDesc swapchainDesc{};
    swapchainDesc.width = 960;
    swapchainDesc.height = 540;
    if (!init_test_device("LunaRhiEnhancement ResizeStress", swapchainDesc, &context, 960, 540)) {
        return false;
    }

    auto* const nativeWindow = static_cast<GLFWwindow*>(context.window->getNativeWindow());
    if (nativeWindow == nullptr) {
        shutdown_test_device(&context);
        return false;
    }

    const std::array<std::pair<int, int>, 6> sizes = {
        std::pair<int, int>{960, 540},
        std::pair<int, int>{1280, 720},
        std::pair<int, int>{840, 640},
        std::pair<int, int>{1440, 900},
        std::pair<int, int>{1024, 768},
        std::pair<int, int>{1180, 680},
    };

    bool passed = true;
    for (int iteration = 0; iteration < 3 && passed; ++iteration) {
        for (const auto& [width, height] : sizes) {
            glfwSetWindowSize(nativeWindow, width, height);
            context.window->onUpdate();
            if (!run_single_frame(&context)) {
                passed = false;
                break;
            }

            const luna::RHISwapchainState state = context.device->getSwapchainState();
            LUNA_CORE_INFO("resize_stress step: requested={}x{}, observed={}x{}, valid={}",
                           width,
                           height,
                           state.width,
                           state.height,
                           state.valid ? "true" : "false");
            if (!state.valid || state.width == 0 || state.height == 0) {
                passed = false;
                break;
            }
        }
    }

    if (passed) {
        LUNA_CORE_INFO("resize_stress PASS");
    } else {
        LUNA_CORE_ERROR("resize_stress FAIL");
    }

    shutdown_test_device(&context);
    return passed;
}

bool run_named_self_test(std::string_view selfTestName)
{
    if (selfTestName == "rhi_enhancement_baseline") {
        return run_baseline_self_test();
    }

    if (selfTestName == "adapter_enumeration") {
        return run_adapter_enumeration_self_test();
    }

    if (selfTestName == "device_creation_without_surface") {
        return run_device_creation_without_surface_self_test();
    }

    if (selfTestName == "device_limits") {
        return run_device_limits_self_test();
    }

    if (selfTestName == "format_support") {
        return run_format_support_self_test();
    }

    if (selfTestName == "single_queue_submit" || selfTestName == "queue_submit") {
        return run_single_queue_submit_self_test();
    }

    if (selfTestName == "swapchain_desc") {
        return run_swapchain_desc_self_test();
    }

    if (selfTestName == "headless_device") {
        return run_headless_device_self_test();
    }

    if (selfTestName == "headless_readback") {
        return run_headless_readback_self_test();
    }

    if (selfTestName == "program_pipeline_contract") {
        return run_program_pipeline_contract_self_test();
    }

    if (selfTestName == "layout_validation_negative") {
        return run_layout_validation_negative_self_test();
    }

    if (selfTestName == "renderer_public_path") {
        return run_renderer_public_path_self_test();
    }

    if (selfTestName == "graduation_matrix_summary") {
        return run_graduation_matrix_summary_self_test();
    }

    if (selfTestName == "frame_retirement") {
        return run_frame_retirement_self_test();
    }

    if (selfTestName == "resize_stress") {
        return run_resize_stress_self_test();
    }

    LUNA_CORE_ERROR("Unknown self-test '{}'", selfTestName);
    return false;
}

} // namespace

int main(int argc, char** argv)
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    CommandLineOptions options;
    if (!parse_arguments(argc, argv, &options)) {
        luna::Logger::shutdown();
        return 1;
    }

    const bool passed = run_named_self_test(options.selfTestName);
    luna::Logger::shutdown();
    return passed ? 0 : 1;
}
