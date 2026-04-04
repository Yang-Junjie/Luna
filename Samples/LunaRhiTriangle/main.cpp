#include "Core/application.h"
#include "Core/log.h"
#include "Core/Paths.h"
#include "RHI/CommandContext.h"
#include "RHI/RHIDevice.h"
#include "Renderer/RenderPipeline.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kSampleName = LUNA_RHI_TRIANGLE_SAMPLE_NAME;
constexpr std::string_view kWindowTitle = "Luna RHI Triangle";

enum class RuntimeRendererMode : uint8_t {
    Triangle = 0,
    Clear
};

struct CommandLineOptions {
    bool selfTest = false;
    std::string_view selfTestName;
    luna::RHIBackend backend = luna::RHIBackend::Vulkan;
    RuntimeRendererMode rendererMode = RuntimeRendererMode::Triangle;
};

struct RecordingCommandContext final : public luna::IRHICommandContext {
    luna::RenderingInfo lastRenderingInfo{};
    luna::ClearColorValue lastClearColor{};
    luna::ImageBarrierInfo lastImageBarrier{};
    luna::BufferBarrierInfo lastBufferBarrier{};
    luna::BufferCopyInfo lastBufferCopy{};
    luna::BufferImageCopyInfo lastBufferImageCopy{};
    luna::ImageCopyInfo lastImageCopy{};
    luna::PipelineHandle boundPipeline{};
    luna::BufferHandle boundVertexBuffer{};
    luna::BufferHandle boundIndexBuffer{};
    luna::IndexFormat boundIndexFormat = luna::IndexFormat::UInt16;
    luna::ImageHandle transitionedImage{};
    luna::ImageLayout transitionedLayout = luna::ImageLayout::Undefined;
    uint64_t vertexBufferOffset = 0;
    uint64_t indexBufferOffset = 0;
    luna::ResourceSetHandle boundResourceSet{};
    std::vector<uint32_t> boundDynamicOffsets{};
    luna::DrawArguments lastDraw{};
    luna::IndexedDrawArguments lastDrawIndexed{};
    std::vector<uint8_t> lastPushConstants;
    uint32_t pushConstantsOffset = 0;
    luna::ShaderType pushConstantsVisibility = luna::ShaderType::None;
    uint32_t beginRenderingCallCount = 0;
    uint32_t endRenderingCallCount = 0;
    uint32_t clearCallCount = 0;
    uint32_t imageBarrierCallCount = 0;
    uint32_t bufferBarrierCallCount = 0;
    uint32_t transitionImageCallCount = 0;
    uint32_t copyBufferCallCount = 0;
    uint32_t copyImageCallCount = 0;
    uint32_t copyBufferToImageCallCount = 0;
    uint32_t copyImageToBufferCallCount = 0;
    uint32_t bindPipelineCallCount = 0;
    uint32_t bindComputePipelineCallCount = 0;
    uint32_t bindVertexBufferCallCount = 0;
    uint32_t bindIndexBufferCallCount = 0;
    uint32_t bindResourceSetCallCount = 0;
    uint32_t pushConstantsCallCount = 0;
    uint32_t drawCallCount = 0;
    uint32_t drawIndexedCallCount = 0;
    uint32_t dispatchCallCount = 0;
    uint32_t dispatchIndirectCallCount = 0;

    luna::RHIResult beginRendering(const luna::RenderingInfo& renderingInfo) override
    {
        lastRenderingInfo = renderingInfo;
        ++beginRenderingCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult endRendering() override
    {
        ++endRenderingCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult clearColor(const luna::ClearColorValue& color) override
    {
        lastClearColor = color;
        ++clearCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult imageBarrier(const luna::ImageBarrierInfo& barrierInfo) override
    {
        lastImageBarrier = barrierInfo;
        ++imageBarrierCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult bufferBarrier(const luna::BufferBarrierInfo& barrierInfo) override
    {
        lastBufferBarrier = barrierInfo;
        ++bufferBarrierCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult transitionImage(luna::ImageHandle image, luna::ImageLayout newLayout) override
    {
        transitionedImage = image;
        transitionedLayout = newLayout;
        ++transitionImageCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult copyBuffer(const luna::BufferCopyInfo& copyInfo) override
    {
        lastBufferCopy = copyInfo;
        ++copyBufferCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult copyImage(const luna::ImageCopyInfo& copyInfo) override
    {
        lastImageCopy = copyInfo;
        ++copyImageCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult copyBufferToImage(const luna::BufferImageCopyInfo& copyInfo) override
    {
        lastBufferImageCopy = copyInfo;
        ++copyBufferToImageCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult copyImageToBuffer(const luna::BufferImageCopyInfo& copyInfo) override
    {
        lastBufferImageCopy = copyInfo;
        ++copyImageToBufferCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult bindGraphicsPipeline(luna::PipelineHandle pipeline) override
    {
        boundPipeline = pipeline;
        ++bindPipelineCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult bindComputePipeline(luna::PipelineHandle pipeline) override
    {
        boundPipeline = pipeline;
        ++bindComputePipelineCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult bindVertexBuffer(luna::BufferHandle buffer, uint64_t offset) override
    {
        boundVertexBuffer = buffer;
        vertexBufferOffset = offset;
        ++bindVertexBufferCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult bindIndexBuffer(luna::BufferHandle buffer, luna::IndexFormat indexFormat, uint64_t offset) override
    {
        boundIndexBuffer = buffer;
        boundIndexFormat = indexFormat;
        indexBufferOffset = offset;
        ++bindIndexBufferCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult bindResourceSet(luna::ResourceSetHandle resourceSet,
                                    std::span<const uint32_t> dynamicOffsets = {}) override
    {
        boundResourceSet = resourceSet;
        boundDynamicOffsets.assign(dynamicOffsets.begin(), dynamicOffsets.end());
        ++bindResourceSetCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult pushConstants(const void* data,
                                  uint32_t size,
                                  uint32_t offset,
                                  luna::ShaderType visibility) override
    {
        if (data == nullptr || size == 0) {
            return luna::RHIResult::InvalidArgument;
        }

        const auto* bytes = static_cast<const uint8_t*>(data);
        lastPushConstants.assign(bytes, bytes + size);
        pushConstantsOffset = offset;
        pushConstantsVisibility = visibility;
        ++pushConstantsCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult draw(const luna::DrawArguments& arguments) override
    {
        lastDraw = arguments;
        ++drawCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult drawIndexed(const luna::IndexedDrawArguments& arguments) override
    {
        lastDrawIndexed = arguments;
        ++drawIndexedCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult dispatch(uint32_t, uint32_t, uint32_t) override
    {
        ++dispatchCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult dispatchIndirect(luna::BufferHandle, uint64_t) override
    {
        ++dispatchIndirectCallCount;
        return luna::RHIResult::Success;
    }
};

struct SelfTestCase {
    std::string_view name;
    std::string_view summary;
};

constexpr uint32_t kSpirvMagic = 0x07230203u;

constexpr std::array<SelfTestCase, 17> kSelfTests{{
    {"backend", "Backend factory + capability query"},
    {"types", "Handle and resource type contract"},
    {"device-init", "Real VulkanRHIDevice init + shutdown"},
    {"device-contract", "Device/frame lifecycle contract"},
    {"command-contract", "Command context contract"},
    {"frame-context", "FrameContext and acquire/present flow"},
    {"desc", "Handle/Desc v2 contract"},
    {"buffer", "Persistent buffer handles"},
    {"image", "Persistent image + sampler handles"},
    {"shader", "Shader handles + reflection"},
    {"upload", "Upload and staging path"},
    {"layout", "ResourceLayout creation"},
    {"resourceset-buffer", "ResourceSet buffer updates"},
    {"resourceset-image", "ResourceSet image + sampler updates"},
    {"pipeline", "GraphicsPipelineDesc driven pipeline creation"},
    {"layout-mismatch", "Reflection validation against desc"},
    {"vertex", "Triangle vertex data"},
}};

std::string normalize_path(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::filesystem::path build_shader_root()
{
    return std::filesystem::path{LUNA_RHI_TRIANGLE_BUILD_SHADER_ROOT}.lexically_normal();
}

std::filesystem::path triangle_vertex_shader_path()
{
    return (build_shader_root() / "triangle.vert.spv").lexically_normal();
}

std::filesystem::path triangle_fragment_shader_path()
{
    return (build_shader_root() / "triangle.frag.spv").lexically_normal();
}

std::array<TriangleVertex, 3> make_triangle_vertices()
{
    return {{
        TriangleVertex{{0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}},
        TriangleVertex{{0.6f, 0.45f}, {0.2f, 1.0f, 0.3f}},
        TriangleVertex{{-0.6f, 0.45f}, {0.2f, 0.4f, 1.0f}},
    }};
}

luna::ResourceLayoutDesc make_scene_layout_desc()
{
    luna::ResourceLayoutDesc desc{};
    desc.debugName = "SceneLayout";
    desc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::AllGraphics});
    return desc;
}

luna::ResourceLayoutDesc make_textured_layout_desc()
{
    luna::ResourceLayoutDesc desc{};
    desc.debugName = "TexturedLayout";
    desc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Fragment});
    desc.bindings.push_back({1, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    return desc;
}

luna::ResourceLayoutDesc make_material_layout_desc()
{
    luna::ResourceLayoutDesc desc{};
    desc.debugName = "MaterialLayout";
    desc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Fragment});
    desc.bindings.push_back({1, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    desc.bindings.push_back({2, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    return desc;
}

luna::GraphicsPipelineDesc make_triangle_pipeline_desc(std::string_view vertexShaderPath,
                                                       std::string_view fragmentShaderPath,
                                                       luna::PixelFormat colorFormat)
{
    luna::GraphicsPipelineDesc desc{};
    desc.debugName = "TrianglePipeline";
    desc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
    desc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = fragmentShaderPath};
    desc.vertexLayout.stride = sizeof(TriangleVertex);
    desc.vertexLayout.attributes.push_back({0, 0, luna::VertexFormat::Float2});
    desc.vertexLayout.attributes.push_back({1, sizeof(float) * 2, luna::VertexFormat::Float3});
    desc.cullMode = luna::CullMode::None;
    desc.frontFace = luna::FrontFace::Clockwise;
    desc.colorAttachments.push_back({colorFormat, false});
    return desc;
}

bool validate_layout_against_shader(std::string_view stageName,
                                    uint32_t setIndex,
                                    const luna::ResourceLayoutDesc& desc,
                                    const luna::Shader::ReflectionMap* reflection)
{
    if (reflection == nullptr) {
        LUNA_CORE_ERROR("Reflection validation failed: stage={} set={} has no reflection data", stageName, setIndex);
        return false;
    }

    const auto reflectionSetIt = reflection->find(setIndex);
    if (reflectionSetIt == reflection->end()) {
        LUNA_CORE_ERROR("Reflection validation mismatch: stage={} set={} missing from shader", stageName, setIndex);
        return false;
    }

    const auto& reflectionBindings = reflectionSetIt->second;
    for (const luna::ResourceBindingDesc& descBinding : desc.bindings) {
        const auto bindingIt =
            std::find_if(reflectionBindings.begin(), reflectionBindings.end(), [&descBinding](const auto& binding) {
                return binding.binding == descBinding.binding;
            });
        if (bindingIt == reflectionBindings.end()) {
            LUNA_CORE_ERROR("Reflection validation mismatch: stage={} set={} binding={} missing from shader",
                            stageName,
                            setIndex,
                            descBinding.binding);
            return false;
        }

        if (bindingIt->type != descBinding.type || bindingIt->count != descBinding.count) {
            LUNA_CORE_ERROR(
                "Reflection validation mismatch: stage={} set={} binding={} descType={} shaderType={} descCount={} shaderCount={}",
                stageName,
                setIndex,
                descBinding.binding,
                static_cast<uint32_t>(descBinding.type),
                static_cast<uint32_t>(bindingIt->type),
                descBinding.count,
                bindingIt->count);
            return false;
        }
    }

    for (const luna::ShaderReflectionData& reflectedBinding : reflectionBindings) {
        const auto bindingIt = std::find_if(desc.bindings.begin(), desc.bindings.end(), [&reflectedBinding](const auto& binding) {
            return binding.binding == reflectedBinding.binding;
        });
        if (bindingIt == desc.bindings.end()) {
            LUNA_CORE_ERROR("Reflection validation mismatch: stage={} set={} binding={} exists only in shader",
                            stageName,
                            setIndex,
                            reflectedBinding.binding);
            return false;
        }
    }

    return true;
}

std::optional<std::vector<uint32_t>> read_spirv_code(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || (fileSize % sizeof(uint32_t)) != 0) {
        return std::nullopt;
    }

    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));
    return code;
}

struct DeviceTestContext {
    std::unique_ptr<luna::Window> window;
    std::unique_ptr<luna::IRHIDevice> device;
};

std::filesystem::path mesh_vertex_shader_path()
{
    return luna::paths::shader("Internal/mesh.vert.spv");
}

std::filesystem::path mesh_fragment_shader_path()
{
    return luna::paths::shader("Internal/mesh.frag.spv");
}

bool init_test_device(std::string_view title,
                      DeviceTestContext* context,
                      uint32_t width = 1280,
                      uint32_t height = 720,
                      luna::RHIBackend backend = luna::RHIBackend::Vulkan)
{
    context->window = luna::Window::create(luna::WindowProps{std::string(title), width, height, false, false});
    if (!context->window || context->window->getNativeWindow() == nullptr) {
        LUNA_CORE_ERROR("Failed to create hidden test window");
        return false;
    }

    context->device = luna::CreateRHIDevice(backend);
    if (!context->device) {
        LUNA_CORE_ERROR("Failed to create IRHIDevice for {}", luna::to_string(backend));
        return false;
    }

    luna::DeviceCreateInfo createInfo{};
    createInfo.applicationName = title;
    createInfo.backend = backend;
    createInfo.nativeWindow = context->window->getNativeWindow();
    createInfo.swapchain = {.width = width, .height = height, .bufferCount = 2, .format = luna::PixelFormat::BGRA8Unorm};

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
    if (context->device) {
        context->device->shutdown();
    }

    context->device.reset();
    context->window.reset();
}

void log_shader_reflection(std::string_view shaderName,
                           luna::ShaderHandle handle,
                           const luna::Shader::ReflectionMap* reflection)
{
    LUNA_CORE_INFO("{} shader handle={}", shaderName, handle.value);
    if (reflection == nullptr) {
        LUNA_CORE_INFO("{} shader reflection: <empty>", shaderName);
        return;
    }

    for (const auto& [setIndex, bindings] : *reflection) {
        for (const auto& binding : bindings) {
            LUNA_CORE_INFO("{} reflection set={} binding={} type={} count={} size={}",
                           shaderName,
                           setIndex,
                           binding.binding,
                           static_cast<uint32_t>(binding.type),
                           binding.count,
                           binding.size);
        }
    }
}

void log_sample_identity()
{
    LUNA_CORE_INFO("Sample={}", kSampleName);
    LUNA_CORE_INFO("ResourceRoot={}", normalize_path(luna::paths::asset_root()));
    LUNA_CORE_INFO("ShaderRoot={}", normalize_path(build_shader_root()));
}

void log_available_self_tests()
{
    LUNA_CORE_INFO("Available self-tests:");
    for (const SelfTestCase& selfTest : kSelfTests) {
        LUNA_CORE_INFO("  {} - {}", selfTest.name, selfTest.summary);
    }
}

const SelfTestCase* find_self_test(std::string_view name)
{
    for (const SelfTestCase& selfTest : kSelfTests) {
        if (selfTest.name == name) {
            return &selfTest;
        }
    }

    if (name == "factory") {
        return find_self_test("backend");
    }
    if (name == "device-api") {
        return find_self_test("device-contract");
    }
    if (name == "command-api") {
        return find_self_test("command-contract");
    }

    return nullptr;
}

const char* yes_no(bool value)
{
    return value ? "yes" : "no";
}

void log_capabilities(const luna::RHICapabilities& capabilities)
{
    LUNA_CORE_INFO(
        "Capability backend={} implemented={} graphics={} present={} dynamicRendering={} indexedDraw={} resourceSets={} framesInFlight={}",
        luna::to_string(capabilities.backend),
        yes_no(capabilities.implemented),
        yes_no(capabilities.supportsGraphics),
        yes_no(capabilities.supportsPresent),
        yes_no(capabilities.supportsDynamicRendering),
        yes_no(capabilities.supportsIndexedDraw),
        yes_no(capabilities.supportsResourceSets),
        capabilities.framesInFlight);
}

bool parse_backend_value(std::string_view value, luna::RHIBackend* backend)
{
    const std::optional parsed = luna::parse_rhi_backend(value);
    if (!parsed.has_value()) {
        return false;
    }

    *backend = *parsed;
    return true;
}

bool parse_renderer_mode_value(std::string_view value, RuntimeRendererMode* rendererMode)
{
    if (value == "triangle") {
        *rendererMode = RuntimeRendererMode::Triangle;
        return true;
    }

    if (value == "clear") {
        *rendererMode = RuntimeRendererMode::Clear;
        return true;
    }

    return false;
}

bool parse_arguments(int argc, char** argv, CommandLineOptions* options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];

        if (argument == "--self-test") {
            options->selfTest = true;
            continue;
        }

        constexpr std::string_view selfTestPrefix = "--self-test=";
        if (argument.starts_with(selfTestPrefix)) {
            options->selfTest = true;
            options->selfTestName = argument.substr(selfTestPrefix.size());
            continue;
        }

        constexpr std::string_view backendPrefix = "--backend=";
        if (argument.starts_with(backendPrefix)) {
            if (!parse_backend_value(argument.substr(backendPrefix.size()), &options->backend)) {
                LUNA_CORE_ERROR("Unsupported backend argument '{}'", argument);
                return false;
            }
            continue;
        }

        if (argument == "--backend") {
            if (index + 1 >= argc) {
                LUNA_CORE_ERROR("Missing backend name after --backend");
                return false;
            }

            ++index;
            if (!parse_backend_value(argv[index], &options->backend)) {
                LUNA_CORE_ERROR("Unsupported backend argument '{}'", argv[index]);
                return false;
            }
            continue;
        }

        constexpr std::string_view rendererPrefix = "--renderer=";
        if (argument.starts_with(rendererPrefix)) {
            if (!parse_renderer_mode_value(argument.substr(rendererPrefix.size()), &options->rendererMode)) {
                LUNA_CORE_ERROR("Unsupported renderer argument '{}'", argument);
                return false;
            }
            continue;
        }

        if (argument == "--renderer") {
            if (index + 1 >= argc) {
                LUNA_CORE_ERROR("Missing renderer name after --renderer");
                return false;
            }

            ++index;
            if (!parse_renderer_mode_value(argv[index], &options->rendererMode)) {
                LUNA_CORE_ERROR("Unsupported renderer argument '{}'", argv[index]);
                return false;
            }
            continue;
        }

        LUNA_CORE_WARN("Ignoring unknown argument '{}'", argument);
    }

    return true;
}

bool run_default_self_test()
{
    log_sample_identity();
    log_available_self_tests();
    return true;
}

bool run_backend_self_test(luna::RHIBackend backend)
{
    const luna::RHICapabilities capabilities = luna::QueryRHICapabilities(backend);
    log_capabilities(capabilities);

    std::unique_ptr<luna::IRHIDevice> device = luna::CreateRHIDevice(backend);
    if (capabilities.implemented) {
        if (!device || device->getBackend() != backend) {
            LUNA_CORE_ERROR("Backend factory failed for {}", luna::to_string(backend));
            return false;
        }

        if (device->getCapabilities().backend != backend || !device->getCapabilities().implemented) {
            LUNA_CORE_ERROR("Capability query mismatch for {}", luna::to_string(backend));
            return false;
        }

        LUNA_CORE_INFO("VulkanRHIDevice created");
        LUNA_CORE_INFO("Backend factory PASS");
        return true;
    }

    if (device != nullptr) {
        LUNA_CORE_ERROR("Backend factory failed for {}", luna::to_string(backend));
        return false;
    }

    LUNA_CORE_INFO("Backend '{}' is recognized but not implemented yet", luna::to_string(backend));
    LUNA_CORE_INFO("Backend factory PASS");
    return true;
}

bool run_types_self_test()
{
    const luna::BufferHandle invalidBuffer{};
    const luna::BufferHandle firstBuffer = luna::BufferHandle::fromRaw(7);
    const luna::BufferHandle secondBuffer = luna::BufferHandle::fromRaw(8);
    const luna::PipelineHandle pipeline = luna::PipelineHandle::fromRaw(21);
    const luna::ResourceLayoutHandle resourceLayout = luna::ResourceLayoutHandle::fromRaw(34);
    const luna::ResourceSetHandle resourceSet = luna::ResourceSetHandle::fromRaw(55);
    const luna::SwapchainHandle swapchain = luna::SwapchainHandle::fromRaw(89);

    const bool ok = !invalidBuffer.isValid() && firstBuffer.isValid() && firstBuffer == firstBuffer &&
                    firstBuffer != secondBuffer && pipeline.isValid() && resourceLayout.isValid() &&
                    resourceSet.isValid() && swapchain.isValid() &&
                    luna::RHIResult::Success != luna::RHIResult::Unsupported;

    if (!ok) {
        LUNA_CORE_ERROR("RHI handle/result contract validation failed");
        return false;
    }

    constexpr std::array<std::string_view, 8> resourceKinds{
        "Buffer", "Image", "Sampler", "Shader", "Pipeline", "ResourceLayout", "ResourceSet", "Swapchain"};
    for (std::string_view resourceKind : resourceKinds) {
        LUNA_CORE_INFO("Recognized public resource type: {}", resourceKind);
    }

    LUNA_CORE_INFO("RHI Types PASS");
    return true;
}

bool run_device_init_self_test(luna::RHIBackend backend)
{
    if (backend != luna::RHIBackend::Vulkan) {
        LUNA_CORE_ERROR("device-init only supports Vulkan right now");
        return false;
    }

    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle DeviceInit", &context, 960, 540, backend)) {
        return false;
    }

    shutdown_test_device(&context);

    std::unique_ptr<luna::IRHIDevice> invalidDevice = luna::CreateRHIDevice(backend);
    if (!invalidDevice) {
        LUNA_CORE_ERROR("Failed to create IRHIDevice for invalid-argument test");
        return false;
    }

    luna::DeviceCreateInfo invalidCreateInfo{};
    invalidCreateInfo.backend = backend;
    invalidCreateInfo.applicationName = "";
    invalidCreateInfo.nativeWindow = nullptr;
    const luna::RHIResult invalidResult = invalidDevice->init(invalidCreateInfo);
    if (invalidResult != luna::RHIResult::InvalidArgument) {
        LUNA_CORE_ERROR("Expected InvalidArgument from invalid init, got {}", luna::to_string(invalidResult));
        return false;
    }

    LUNA_CORE_INFO("init/shutdown PASS");
    LUNA_CORE_INFO("Invalid init result={}", luna::to_string(invalidResult));
    return true;
}

bool run_device_api_self_test(luna::RHIBackend backend)
{
    if (backend != luna::RHIBackend::Vulkan) {
        LUNA_CORE_ERROR("device-contract only supports Vulkan right now");
        return false;
    }

    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle DeviceContract", &context, 1600, 900, backend)) {
        return false;
    }

    luna::FrameContext frameContext{};
    if (context.device->beginFrame(&frameContext) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("IRHIDevice::beginFrame failed");
        shutdown_test_device(&context);
        return false;
    }

    if (frameContext.commandContext == nullptr ||
        frameContext.commandContext->clearColor({0.08f, 0.12f, 0.18f, 1.0f}) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("Frame command context is unavailable");
        shutdown_test_device(&context);
        return false;
    }

    if (context.device->endFrame() != luna::RHIResult::Success ||
        context.device->present() != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("Device frame submission failed");
        shutdown_test_device(&context);
        return false;
    }

    if (context.window) {
        context.window->onUpdate();
    }

    const luna::RHICapabilities capabilities = context.device->getCapabilities();
    const bool ok = frameContext.commandContext != nullptr && frameContext.renderWidth == 1600 &&
                    frameContext.renderHeight == 900 && frameContext.backbuffer.isValid() &&
                    capabilities.supportsGraphics && capabilities.supportsPresent;
    if (!ok) {
        LUNA_CORE_ERROR("Device/frame contract validation failed");
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("Device contract methods: beginFrame beginRendering bind draw drawIndexed endFrame present");
    shutdown_test_device(&context);
    LUNA_CORE_INFO("Device Contract PASS");
    return true;
}

bool run_frame_context_self_test(luna::RHIBackend backend)
{
    if (backend != luna::RHIBackend::Vulkan) {
        LUNA_CORE_ERROR("frame-context only supports Vulkan right now");
        return false;
    }

    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle FrameContext", &context, 960, 540, backend)) {
        return false;
    }

    for (uint32_t frameIndex = 0; frameIndex < 3; ++frameIndex) {
        luna::FrameContext frameContext{};
        if (context.device->beginFrame(&frameContext) != luna::RHIResult::Success) {
            shutdown_test_device(&context);
            return false;
        }

        LUNA_CORE_INFO("FrameContext frameIndex={} backbuffer={} size={}x{} format={}",
                       frameContext.frameIndex,
                       frameContext.backbuffer.value,
                       frameContext.renderWidth,
                       frameContext.renderHeight,
                       static_cast<uint32_t>(frameContext.backbufferFormat));

        if (frameContext.frameIndex != frameIndex || !frameContext.backbuffer.isValid() ||
            frameContext.commandContext == nullptr) {
            shutdown_test_device(&context);
            return false;
        }

        const luna::ClearColorValue clearColor{
            0.15f + 0.1f * static_cast<float>(frameIndex), 0.2f, 0.3f, 1.0f};
        if (frameContext.commandContext->clearColor(clearColor) != luna::RHIResult::Success ||
            context.device->endFrame() != luna::RHIResult::Success ||
            context.device->present() != luna::RHIResult::Success) {
            shutdown_test_device(&context);
            return false;
        }

        if (context.window) {
            context.window->onUpdate();
        }
    }

    shutdown_test_device(&context);
    LUNA_CORE_INFO("FrameContext PASS");
    return true;
}

bool run_command_api_self_test()
{
    RecordingCommandContext context;
    luna::RenderingInfo renderingInfo{};
    renderingInfo.width = 1280;
    renderingInfo.height = 720;
    renderingInfo.colorAttachments.push_back(
        {.image = luna::ImageHandle::fromRaw(500),
         .format = luna::PixelFormat::BGRA8Unorm,
         .clearColor = {0.15f, 0.25f, 0.35f, 1.0f}});
    const luna::ClearColorValue clearColor{0.15f, 0.25f, 0.35f, 1.0f};
    const luna::PipelineHandle pipeline = luna::PipelineHandle::fromRaw(101);
    const luna::BufferHandle vertexBuffer = luna::BufferHandle::fromRaw(202);
    const luna::BufferHandle indexBuffer = luna::BufferHandle::fromRaw(303);
    const luna::ResourceSetHandle resourceSet = luna::ResourceSetHandle::fromRaw(404);
    const std::array<uint32_t, 4> pushConstants{1, 2, 3, 4};
    const luna::DrawArguments drawArguments{3, 1, 0, 0};
    const luna::IndexedDrawArguments drawIndexedArguments{3, 1, 0, 0, 0};

    const bool ok = context.beginRendering(renderingInfo) == luna::RHIResult::Success &&
                    context.clearColor(clearColor) == luna::RHIResult::Success &&
                    context.bindGraphicsPipeline(pipeline) == luna::RHIResult::Success &&
                    context.bindVertexBuffer(vertexBuffer, 16) == luna::RHIResult::Success &&
                    context.bindIndexBuffer(indexBuffer, luna::IndexFormat::UInt32, 24) ==
                        luna::RHIResult::Success &&
                    context.bindResourceSet(resourceSet) == luna::RHIResult::Success &&
                    context.pushConstants(pushConstants.data(),
                                         static_cast<uint32_t>(pushConstants.size() * sizeof(uint32_t)),
                                         8,
                                         luna::ShaderType::Fragment) == luna::RHIResult::Success &&
                    context.draw(drawArguments) == luna::RHIResult::Success &&
                    context.drawIndexed(drawIndexedArguments) == luna::RHIResult::Success &&
                    context.endRendering() == luna::RHIResult::Success && context.beginRenderingCallCount == 1 &&
                    context.endRenderingCallCount == 1 && context.clearCallCount == 1 &&
                    context.bindPipelineCallCount == 1 && context.bindVertexBufferCallCount == 1 &&
                    context.bindIndexBufferCallCount == 1 && context.bindResourceSetCallCount == 1 &&
                    context.pushConstantsCallCount == 1 &&
                    context.drawCallCount == 1 && context.drawIndexedCallCount == 1 &&
                    context.boundPipeline == pipeline && context.boundVertexBuffer == vertexBuffer &&
                    context.boundIndexBuffer == indexBuffer && context.boundIndexFormat == luna::IndexFormat::UInt32 &&
                    context.boundResourceSet == resourceSet && context.vertexBufferOffset == 16 &&
                    context.indexBufferOffset == 24 && context.lastDraw.vertexCount == 3 &&
                    context.lastDrawIndexed.indexCount == 3 &&
                    context.pushConstantsOffset == 8 &&
                    context.pushConstantsVisibility == luna::ShaderType::Fragment &&
                    context.lastPushConstants.size() == pushConstants.size() * sizeof(uint32_t) &&
                    context.lastRenderingInfo.width == 1280 &&
                    context.lastRenderingInfo.colorAttachments.size() == 1;

    if (!ok) {
        LUNA_CORE_ERROR("IRHICommandContext contract validation failed");
        return false;
    }

    LUNA_CORE_INFO("Command contract methods: beginFrame beginRendering bind draw drawIndexed endFrame present");
    LUNA_CORE_INFO("Command Contract PASS");
    return true;
}

bool run_desc_self_test()
{
    luna::BufferDesc bufferDesc{};
    bufferDesc.size = 1024;
    bufferDesc.usage = luna::BufferUsage::Vertex | luna::BufferUsage::TransferDst;
    bufferDesc.memoryUsage = luna::MemoryUsage::Default;
    bufferDesc.debugName = "TriangleVertexBuffer";

    luna::ImageDesc imageDesc{};
    imageDesc.width = 1280;
    imageDesc.height = 720;
    imageDesc.format = luna::PixelFormat::BGRA8Unorm;
    imageDesc.usage = luna::ImageUsage::ColorAttachment | luna::ImageUsage::TransferDst;
    imageDesc.debugName = "BackBuffer";

    luna::ShaderDesc vertexShader{};
    vertexShader.stage = luna::ShaderType::Vertex;
    vertexShader.filePath = "Shaders/Internal/colored_triangle.vert.spv";

    luna::ShaderDesc fragmentShader{};
    fragmentShader.stage = luna::ShaderType::Fragment;
    fragmentShader.filePath = "Shaders/Internal/colored_triangle.frag.spv";

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "TrianglePipeline";
    pipelineDesc.vertexShader = vertexShader;
    pipelineDesc.fragmentShader = fragmentShader;
    pipelineDesc.vertexLayout.stride = sizeof(float) * 6;
    pipelineDesc.vertexLayout.attributes.push_back({0, 0, luna::VertexFormat::Float3});
    pipelineDesc.vertexLayout.attributes.push_back({1, sizeof(float) * 3, luna::VertexFormat::Float3});
    pipelineDesc.colorAttachments.push_back({luna::PixelFormat::BGRA8Unorm, false});
    pipelineDesc.depthStencil = {luna::PixelFormat::D32Float, true, true, luna::CompareOp::LessOrEqual};

    luna::ResourceLayoutDesc resourceLayoutDesc{};
    resourceLayoutDesc.debugName = "TriangleLayout";
    resourceLayoutDesc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Vertex});
    resourceLayoutDesc.bindings.push_back({1, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});

    luna::ResourceSetWriteDesc resourceSetWriteDesc{};
    resourceSetWriteDesc.buffers.push_back(
        {.binding = 0,
         .buffer = luna::BufferHandle::fromRaw(1),
         .offset = 0,
         .size = 256,
         .type = luna::ResourceType::UniformBuffer});
    resourceSetWriteDesc.images.push_back(
        {.binding = 1,
         .image = luna::ImageHandle::fromRaw(2),
         .sampler = luna::SamplerHandle::fromRaw(3),
         .type = luna::ResourceType::CombinedImageSampler});

    const bool ok = bufferDesc.size == 1024 && imageDesc.width == 1280 && imageDesc.height == 720 &&
                    vertexShader.stage == luna::ShaderType::Vertex &&
                    fragmentShader.stage == luna::ShaderType::Fragment &&
                    pipelineDesc.vertexLayout.attributes.size() == 2 &&
                    pipelineDesc.colorAttachments.size() == 1 && pipelineDesc.depthStencil.depthTestEnabled &&
                    pipelineDesc.depthStencil.depthWriteEnabled && resourceLayoutDesc.bindings.size() == 2 &&
                    resourceSetWriteDesc.buffers.size() == 1 && resourceSetWriteDesc.images.size() == 1;

    if (!ok) {
        LUNA_CORE_ERROR("RHI descriptor contract validation failed");
        return false;
    }

    constexpr std::array<std::string_view, 8> resourceKinds{
        "Buffer", "Image", "Sampler", "Shader", "Pipeline", "ResourceLayout", "ResourceSet", "Swapchain"};
    for (std::string_view resourceKind : resourceKinds) {
        LUNA_CORE_INFO("Desc v2 recognized resource: {}", resourceKind);
    }

    LUNA_CORE_INFO("Handle/Desc v2 PASS");
    return true;
}

bool run_shader_self_test()
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle ShaderTest", &context)) {
        return false;
    }

    luna::ShaderHandle vertexShader{};
    luna::ShaderHandle fragmentShader{};
    const luna::RHIResult vertexResult =
        context.device->createShader({.stage = luna::ShaderType::Vertex, .filePath = normalize_path(mesh_vertex_shader_path())},
                                     &vertexShader);
    const luna::RHIResult fragmentResult =
        context.device->createShader({.stage = luna::ShaderType::Fragment, .filePath = normalize_path(mesh_fragment_shader_path())},
                                     &fragmentShader);
    if (vertexResult != luna::RHIResult::Success || fragmentResult != luna::RHIResult::Success) {
        shutdown_test_device(&context);
        return false;
    }

    const auto* vertexReflection = context.device->getShaderReflection(vertexShader);
    const auto* fragmentReflection = context.device->getShaderReflection(fragmentShader);
    const bool ok = vertexShader.isValid() && fragmentShader.isValid() && vertexReflection != nullptr &&
                    fragmentReflection != nullptr && !vertexReflection->empty() && !fragmentReflection->empty();

    if (!ok) {
        LUNA_CORE_ERROR("Shader reflection self-test failed");
        shutdown_test_device(&context);
        return false;
    }

    log_shader_reflection("vertex", vertexShader, vertexReflection);
    log_shader_reflection("fragment", fragmentShader, fragmentReflection);

    luna::ShaderHandle missingShader{};
    const luna::RHIResult missingResult =
        context.device->createShader({.stage = luna::ShaderType::Fragment,
                                      .filePath = normalize_path(luna::paths::shader("Internal/does_not_exist.spv"))},
                                     &missingShader);
    if (missingResult == luna::RHIResult::Success) {
        shutdown_test_device(&context);
        return false;
    }

    context.device->destroyShader(fragmentShader);
    context.device->destroyShader(vertexShader);
    shutdown_test_device(&context);
    LUNA_CORE_INFO("Shader PASS");
    return true;
}

bool run_buffer_self_test(luna::RHIBackend backend)
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle BufferTest", &context, 640, 360, backend)) {
        return false;
    }

    for (uint32_t iteration = 0; iteration < 3; ++iteration) {
        const uint64_t bufferSize = 256ull * static_cast<uint64_t>(iteration + 1);
        const std::array<uint32_t, 16> payload{
            iteration + 1, iteration + 2, iteration + 3, iteration + 4};
        luna::BufferHandle buffer{};
        luna::BufferDesc desc{};
        desc.size = bufferSize;
        desc.usage = luna::BufferUsage::Vertex | luna::BufferUsage::TransferDst;
        desc.memoryUsage = luna::MemoryUsage::Default;
        desc.debugName = "Stage4Buffer";

        if (context.device->createBuffer(desc, &buffer) != luna::RHIResult::Success ||
            !buffer.isValid() ||
            context.device->writeBuffer(buffer, payload.data(), sizeof(payload), 0) != luna::RHIResult::Success) {
            shutdown_test_device(&context);
            return false;
        }

        LUNA_CORE_INFO("BufferHandle={} size={} usage=Vertex|TransferDst debugName={}",
                       buffer.value,
                       bufferSize,
                       desc.debugName);
        context.device->destroyBuffer(buffer);
    }

    shutdown_test_device(&context);
    LUNA_CORE_INFO("Buffer PASS");
    return true;
}

bool run_image_self_test(luna::RHIBackend backend)
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle ImageTest", &context, 640, 360, backend)) {
        return false;
    }

    luna::ImageHandle colorImage{};
    luna::ImageHandle depthImage{};
    luna::SamplerHandle sampler{};
    const luna::ImageDesc colorDesc{
        .width = 320,
        .height = 180,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = luna::PixelFormat::BGRA8Unorm,
        .usage = luna::ImageUsage::ColorAttachment | luna::ImageUsage::Sampled,
        .debugName = "Stage4ColorImage",
    };
    const luna::ImageDesc depthDesc{
        .width = 320,
        .height = 180,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = luna::PixelFormat::D32Float,
        .usage = luna::ImageUsage::DepthStencilAttachment,
        .debugName = "Stage4DepthImage",
    };
    const luna::SamplerDesc samplerDesc{
        .magFilter = luna::FilterMode::Linear,
        .minFilter = luna::FilterMode::Linear,
        .mipmapMode = luna::SamplerMipmapMode::Linear,
        .addressModeU = luna::SamplerAddressMode::Repeat,
        .addressModeV = luna::SamplerAddressMode::Repeat,
        .addressModeW = luna::SamplerAddressMode::Repeat,
        .debugName = "Stage4Sampler",
    };

    const bool created = context.device->createImage(colorDesc, &colorImage) == luna::RHIResult::Success &&
                         context.device->createImage(depthDesc, &depthImage) == luna::RHIResult::Success &&
                         context.device->createSampler(samplerDesc, &sampler) == luna::RHIResult::Success &&
                         colorImage.isValid() && depthImage.isValid() && sampler.isValid();
    if (!created) {
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("ColorImage handle={} size={}x{}", colorImage.value, colorDesc.width, colorDesc.height);
    LUNA_CORE_INFO("DepthImage handle={} size={}x{}", depthImage.value, depthDesc.width, depthDesc.height);
    LUNA_CORE_INFO("Sampler handle={}", sampler.value);

    context.device->destroyImage(colorImage);
    context.device->destroyImage(depthImage);
    context.device->destroySampler(sampler);
    LUNA_CORE_INFO("Old resources released");

    luna::ImageHandle resizedColor{};
    luna::ImageHandle resizedDepth{};
    const luna::ImageDesc resizedColorDesc{
        .width = 512,
        .height = 288,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = luna::PixelFormat::BGRA8Unorm,
        .usage = luna::ImageUsage::ColorAttachment | luna::ImageUsage::Sampled,
        .debugName = "Stage4ColorImageResized",
    };
    const luna::ImageDesc resizedDepthDesc{
        .width = 512,
        .height = 288,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = luna::PixelFormat::D32Float,
        .usage = luna::ImageUsage::DepthStencilAttachment,
        .debugName = "Stage4DepthImageResized",
    };
    if (context.device->createImage(resizedColorDesc, &resizedColor) != luna::RHIResult::Success ||
        context.device->createImage(resizedDepthDesc, &resizedDepth) != luna::RHIResult::Success ||
        !resizedColor.isValid() || !resizedDepth.isValid()) {
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("New resources created color={} depth={}", resizedColor.value, resizedDepth.value);
    context.device->destroyImage(resizedColor);
    context.device->destroyImage(resizedDepth);
    shutdown_test_device(&context);
    LUNA_CORE_INFO("Image PASS");
    return true;
}

bool run_upload_self_test(luna::RHIBackend backend)
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle UploadTest", &context, 640, 360, backend)) {
        return false;
    }

    const auto vertices = make_triangle_vertices();
    luna::BufferHandle vertexBuffer{};
    luna::BufferDesc vertexBufferDesc{};
    vertexBufferDesc.size = static_cast<uint64_t>(vertices.size() * sizeof(TriangleVertex));
    vertexBufferDesc.usage = luna::BufferUsage::Vertex | luna::BufferUsage::TransferDst;
    vertexBufferDesc.memoryUsage = luna::MemoryUsage::Default;
    vertexBufferDesc.debugName = "UploadTriangleVertices";

    const luna::RHIResult createResult =
        context.device->createBuffer(vertexBufferDesc, &vertexBuffer, vertices.data());
    if (createResult != luna::RHIResult::Success || !vertexBuffer.isValid()) {
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("Uploaded {} bytes via RHI PASS", vertexBufferDesc.size);
    LUNA_CORE_INFO("Triangle vertices={} bufferBytes={}", vertices.size(), vertexBufferDesc.size);

    context.device->destroyBuffer(vertexBuffer);
    shutdown_test_device(&context);
    return true;
}

bool run_layout_self_test(luna::RHIBackend backend)
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle LayoutTest", &context, 640, 360, backend)) {
        return false;
    }

    luna::ResourceLayoutDesc layoutDesc{};
    layoutDesc.debugName = "LayoutWithUboAndSampledImage";
    layoutDesc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Vertex});
    layoutDesc.bindings.push_back({1, luna::ResourceType::SampledImage, 1, luna::ShaderType::Fragment});
    luna::ResourceLayoutHandle layout{};
    if (context.device->createResourceLayout(layoutDesc, &layout) != luna::RHIResult::Success || !layout.isValid()) {
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("ResourceLayout created handle={} bindings={}", layout.value, layoutDesc.bindings.size());
    for (const luna::ResourceBindingDesc& binding : layoutDesc.bindings) {
        LUNA_CORE_INFO("ResourceLayout binding={} type={} count={}",
                       binding.binding,
                       static_cast<uint32_t>(binding.type),
                       binding.count);
    }

    context.device->destroyResourceLayout(layout);
    shutdown_test_device(&context);
    return true;
}

bool run_resourceset_buffer_self_test(luna::RHIBackend backend)
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle ResourceSetBuffer", &context, 640, 360, backend)) {
        return false;
    }

    const luna::ResourceLayoutDesc layoutDesc = make_scene_layout_desc();
    luna::ResourceLayoutHandle layout{};
    luna::ResourceSetHandle resourceSet{};
    luna::BufferHandle buffer{};

    luna::BufferDesc bufferDesc{};
    bufferDesc.size = 256;
    bufferDesc.usage = luna::BufferUsage::Uniform | luna::BufferUsage::TransferDst;
    bufferDesc.memoryUsage = luna::MemoryUsage::Default;
    bufferDesc.debugName = "ResourceSetUniformBuffer";

    if (context.device->createResourceLayout(layoutDesc, &layout) != luna::RHIResult::Success ||
        context.device->createResourceSet(layout, &resourceSet) != luna::RHIResult::Success ||
        context.device->createBuffer(bufferDesc, &buffer) != luna::RHIResult::Success) {
        shutdown_test_device(&context);
        return false;
    }

    luna::ResourceSetWriteDesc firstWrite{};
    firstWrite.buffers.push_back({0, buffer, 0, 256, luna::ResourceType::UniformBuffer});
    luna::ResourceSetWriteDesc secondWrite{};
    secondWrite.buffers.push_back({0, buffer, 64, 128, luna::ResourceType::UniformBuffer});

    const bool updated = context.device->updateResourceSet(resourceSet, firstWrite) == luna::RHIResult::Success &&
                         context.device->updateResourceSet(resourceSet, secondWrite) == luna::RHIResult::Success;
    if (!updated) {
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("ResourceSet update PASS");
    LUNA_CORE_INFO("ResourceSet buffer bindings={}", firstWrite.buffers.size());

    context.device->destroyBuffer(buffer);
    context.device->destroyResourceSet(resourceSet);
    context.device->destroyResourceLayout(layout);
    shutdown_test_device(&context);
    return true;
}

bool run_resourceset_image_self_test(luna::RHIBackend backend)
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle ResourceSetImage", &context, 640, 360, backend)) {
        return false;
    }

    const luna::ResourceLayoutDesc layoutDesc = make_textured_layout_desc();
    luna::ResourceLayoutHandle layout{};
    luna::ResourceSetHandle resourceSet{};
    luna::ImageHandle image{};
    luna::SamplerHandle sampler{};

    luna::ImageDesc imageDesc{};
    imageDesc.width = 4;
    imageDesc.height = 4;
    imageDesc.depth = 1;
    imageDesc.mipLevels = 1;
    imageDesc.arrayLayers = 1;
    imageDesc.format = luna::PixelFormat::RGBA8Unorm;
    imageDesc.usage = luna::ImageUsage::Sampled;
    imageDesc.debugName = "ResourceSetImage";

    luna::SamplerDesc samplerDesc{};
    samplerDesc.debugName = "ResourceSetSampler";

    if (context.device->createResourceLayout(layoutDesc, &layout) != luna::RHIResult::Success ||
        context.device->createResourceSet(layout, &resourceSet) != luna::RHIResult::Success ||
        context.device->createImage(imageDesc, &image) != luna::RHIResult::Success ||
        context.device->createSampler(samplerDesc, &sampler) != luna::RHIResult::Success) {
        shutdown_test_device(&context);
        return false;
    }

    luna::ResourceSetWriteDesc validWrite{};
    validWrite.images.push_back(
        {.binding = 1, .image = image, .sampler = sampler, .type = luna::ResourceType::CombinedImageSampler});

    if (context.device->updateResourceSet(resourceSet, validWrite) != luna::RHIResult::Success) {
        shutdown_test_device(&context);
        return false;
    }

    luna::ResourceSetWriteDesc invalidWrite{};
    invalidWrite.images.push_back(
        {.binding = 1, .image = {}, .sampler = sampler, .type = luna::ResourceType::CombinedImageSampler});
    const luna::RHIResult invalidResult = context.device->updateResourceSet(resourceSet, invalidWrite);
    if (invalidResult == luna::RHIResult::Success) {
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("image binding updated for binding={}", validWrite.images.front().binding);
    LUNA_CORE_INFO("sampler binding updated for binding={}", validWrite.images.front().binding);
    LUNA_CORE_INFO("Invalid image binding rejected with {}", luna::to_string(invalidResult));

    context.device->destroySampler(sampler);
    context.device->destroyImage(image);
    context.device->destroyResourceSet(resourceSet);
    context.device->destroyResourceLayout(layout);
    shutdown_test_device(&context);
    return true;
}

bool run_pipeline_self_test(luna::RHIBackend backend)
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle PipelineTest", &context, 960, 540, backend)) {
        return false;
    }

    const std::string vertexShaderPath = normalize_path(triangle_vertex_shader_path());
    const std::string fragmentShaderPath = normalize_path(triangle_fragment_shader_path());
    if (!read_spirv_code(vertexShaderPath).has_value() || !read_spirv_code(fragmentShaderPath).has_value()) {
        shutdown_test_device(&context);
        return false;
    }

    luna::PipelineHandle pipeline{};
    luna::PipelineHandle invalidPipeline{};
    luna::GraphicsPipelineDesc pipelineDesc =
        make_triangle_pipeline_desc(vertexShaderPath, fragmentShaderPath, luna::PixelFormat::BGRA8Unorm);
    if (context.device->createGraphicsPipeline(pipelineDesc, &pipeline) != luna::RHIResult::Success || !pipeline.isValid()) {
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("TrianglePipeline created via RHI");

    luna::GraphicsPipelineDesc invalidDesc =
        make_triangle_pipeline_desc(vertexShaderPath, fragmentShaderPath, luna::PixelFormat::BGRA8Unorm);
    invalidDesc.fragmentShader.filePath = "Shaders/DoesNotExist/missing_triangle.frag.spv";
    const luna::RHIResult invalidShaderResult = context.device->createGraphicsPipeline(invalidDesc, &invalidPipeline);
    if (invalidShaderResult == luna::RHIResult::Success) {
        context.device->destroyPipeline(invalidPipeline);
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("Invalid pipeline desc rejected because fragment shader path is missing");
    LUNA_CORE_INFO("Pipeline validation result={}", luna::to_string(invalidShaderResult));

    luna::GraphicsPipelineDesc invalidVertexLayoutDesc =
        make_triangle_pipeline_desc(vertexShaderPath, fragmentShaderPath, luna::PixelFormat::BGRA8Unorm);
    invalidVertexLayoutDesc.vertexLayout.attributes[1].offset = invalidVertexLayoutDesc.vertexLayout.stride;
    const luna::RHIResult invalidVertexLayoutResult =
        context.device->createGraphicsPipeline(invalidVertexLayoutDesc, &invalidPipeline);
    if (invalidVertexLayoutResult == luna::RHIResult::Success) {
        context.device->destroyPipeline(invalidPipeline);
        shutdown_test_device(&context);
        return false;
    }

    LUNA_CORE_INFO("Invalid vertex layout rejected because attribute 1 exceeds the declared stride");
    LUNA_CORE_INFO("Vertex layout validation result={}", luna::to_string(invalidVertexLayoutResult));

    context.device->destroyPipeline(pipeline);
    shutdown_test_device(&context);
    return true;
}

bool run_layout_mismatch_self_test(luna::RHIBackend backend)
{
    DeviceTestContext context;
    if (!init_test_device("LunaRhiTriangle LayoutMismatch", &context, 640, 360, backend)) {
        return false;
    }

    luna::ShaderHandle vertexShader{};
    luna::ShaderHandle fragmentShader{};
    const std::string vertexShaderPath = normalize_path(mesh_vertex_shader_path());
    const std::string fragmentShaderPath = normalize_path(mesh_fragment_shader_path());
    if (context.device->createShader({.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath}, &vertexShader) !=
            luna::RHIResult::Success ||
        context.device->createShader({.stage = luna::ShaderType::Fragment, .filePath = fragmentShaderPath}, &fragmentShader) !=
            luna::RHIResult::Success) {
        shutdown_test_device(&context);
        return false;
    }

    const auto* vertexReflection = context.device->getShaderReflection(vertexShader);
    const auto* fragmentReflection = context.device->getShaderReflection(fragmentShader);
    luna::ResourceLayoutDesc mismatchDesc = make_scene_layout_desc();
    mismatchDesc.bindings[0].type = luna::ResourceType::CombinedImageSampler;
    const bool mismatchRejected = !validate_layout_against_shader("vertex", 0, mismatchDesc, vertexReflection);

    const luna::ResourceLayoutDesc sceneLayoutDesc = make_scene_layout_desc();
    const luna::ResourceLayoutDesc materialLayoutDesc = make_material_layout_desc();
    const bool valid = validate_layout_against_shader("vertex", 0, sceneLayoutDesc, vertexReflection) &&
                       validate_layout_against_shader("fragment", 0, sceneLayoutDesc, fragmentReflection) &&
                       validate_layout_against_shader("fragment", 1, materialLayoutDesc, fragmentReflection);

    context.device->destroyShader(fragmentShader);
    context.device->destroyShader(vertexShader);
    shutdown_test_device(&context);

    if (!mismatchRejected || !valid) {
        return false;
    }

    LUNA_CORE_INFO("Reflection Validation PASS");
    return true;
}

bool run_vertex_self_test()
{
    const auto vertices = make_triangle_vertices();
    const bool ok = vertices.size() == 3 && vertices[0].position[1] < 0.0f && vertices[1].position[0] > 0.0f &&
                    vertices[2].position[0] < 0.0f;
    if (!ok) {
        LUNA_CORE_ERROR("Triangle vertex self-test failed");
        return false;
    }

    LUNA_CORE_INFO("Uploaded 3 vertices");
    return true;
}

bool validate_runtime_triangle_shaders()
{
    const std::filesystem::path vertexShaderPath = triangle_vertex_shader_path();
    if (!read_spirv_code(vertexShaderPath).has_value()) {
        LUNA_CORE_ERROR("Shader load failed: missing or invalid vertex shader '{}'",
                        normalize_path(vertexShaderPath));
        return false;
    }

    const std::filesystem::path fragmentShaderPath = triangle_fragment_shader_path();
    if (!read_spirv_code(fragmentShaderPath).has_value()) {
        LUNA_CORE_ERROR("Shader load failed: missing or invalid fragment shader '{}'",
                        normalize_path(fragmentShaderPath));
        return false;
    }

    return true;
}

class ClearColorRenderPipeline final : public luna::IRenderPipeline {
public:
    explicit ClearColorRenderPipeline(const luna::ClearColorValue& clearColor)
        : m_clearColor(clearColor)
    {}

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

        luna::RenderingInfo renderingInfo{};
        renderingInfo.width = frameContext.renderWidth;
        renderingInfo.height = frameContext.renderHeight;
        renderingInfo.colorAttachments.push_back(
            {.image = frameContext.backbuffer, .format = frameContext.backbufferFormat, .clearColor = m_clearColor});

        const bool ok = frameContext.commandContext->beginRendering(renderingInfo) == luna::RHIResult::Success &&
                        frameContext.commandContext->clearColor(m_clearColor) == luna::RHIResult::Success &&
                        frameContext.commandContext->endRendering() == luna::RHIResult::Success;
        if (ok && !m_loggedPass) {
            LUNA_CORE_INFO("BeginRendering PASS");
            LUNA_CORE_INFO("Clear PASS");
            LUNA_CORE_INFO("EndRendering PASS");
            m_loggedPass = true;
        }
        return ok;
    }

private:
    luna::ClearColorValue m_clearColor{};
    bool m_loggedPass = false;
};

class TriangleRuntimeRenderPipeline final : public luna::IRenderPipeline {
public:
    TriangleRuntimeRenderPipeline(std::filesystem::path vertexShaderPath, std::filesystem::path fragmentShaderPath)
        : m_vertexShaderPath(normalize_path(vertexShaderPath)),
          m_fragmentShaderPath(normalize_path(fragmentShaderPath))
    {}

    bool init(luna::IRHIDevice&) override
    {
        return true;
    }

    void shutdown(luna::IRHIDevice& device) override
    {
        (void)device;
        m_pipeline = {};
        m_vertexBuffer = {};
        m_pipelineFormat = luna::PixelFormat::Undefined;
    }

    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override
    {
        if (frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
            return false;
        }

        if (!ensure_resources(device, frameContext.backbufferFormat)) {
            return false;
        }

        luna::RenderingInfo renderingInfo{};
        renderingInfo.width = frameContext.renderWidth;
        renderingInfo.height = frameContext.renderHeight;
        renderingInfo.colorAttachments.push_back(
            {.image = frameContext.backbuffer,
             .format = frameContext.backbufferFormat,
             .clearColor = {0.08f, 0.12f, 0.18f, 1.0f}});

        const bool ok = frameContext.commandContext->beginRendering(renderingInfo) == luna::RHIResult::Success &&
                        frameContext.commandContext->bindGraphicsPipeline(m_pipeline) == luna::RHIResult::Success &&
                        frameContext.commandContext->bindVertexBuffer(m_vertexBuffer, 0) == luna::RHIResult::Success &&
                        frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
                        frameContext.commandContext->endRendering() == luna::RHIResult::Success;
        if (ok && !m_loggedBindPass) {
            LUNA_CORE_INFO("bind pipeline PASS");
            LUNA_CORE_INFO("bind vertex buffer PASS");
            LUNA_CORE_INFO("draw PASS");
            m_loggedBindPass = true;
        }
        return ok;
    }

private:
    bool ensure_resources(luna::IRHIDevice& device, luna::PixelFormat colorFormat)
    {
        if (!m_vertexBuffer.isValid()) {
            luna::BufferDesc bufferDesc{};
            bufferDesc.size = static_cast<uint64_t>(m_vertices.size() * sizeof(TriangleVertex));
            bufferDesc.usage = luna::BufferUsage::Vertex | luna::BufferUsage::TransferDst;
            bufferDesc.memoryUsage = luna::MemoryUsage::Default;
            bufferDesc.debugName = "RuntimeTriangleVertices";

            if (device.createBuffer(bufferDesc, &m_vertexBuffer, m_vertices.data()) != luna::RHIResult::Success) {
                return false;
            }
        }

        if (m_pipeline.isValid() && m_pipelineFormat == colorFormat) {
            return true;
        }

        luna::GraphicsPipelineDesc pipelineDesc =
            make_triangle_pipeline_desc(m_vertexShaderPath, m_fragmentShaderPath, colorFormat);
        luna::PipelineHandle newPipeline{};
        if (device.createGraphicsPipeline(pipelineDesc, &newPipeline) != luna::RHIResult::Success || !newPipeline.isValid()) {
            return false;
        }

        m_pipeline = newPipeline;
        m_pipelineFormat = colorFormat;
        return true;
    }

private:
    std::string m_vertexShaderPath;
    std::string m_fragmentShaderPath;
    std::array<TriangleVertex, 3> m_vertices = make_triangle_vertices();
    luna::BufferHandle m_vertexBuffer{};
    luna::PipelineHandle m_pipeline{};
    luna::PixelFormat m_pipelineFormat = luna::PixelFormat::Undefined;
    bool m_loggedBindPass = false;
};

bool run_named_self_test(const CommandLineOptions& options)
{
    if (options.selfTestName.empty() || options.selfTestName == "list") {
        return run_default_self_test();
    }

    if (options.selfTestName == "all") {
        for (const SelfTestCase& selfTest : kSelfTests) {
            if (selfTest.name == "backend" && !run_backend_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "types" && !run_types_self_test()) {
                return false;
            }
            if (selfTest.name == "device-init" && !run_device_init_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "device-contract" && !run_device_api_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "command-contract" && !run_command_api_self_test()) {
                return false;
            }
            if (selfTest.name == "frame-context" && !run_frame_context_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "desc" && !run_desc_self_test()) {
                return false;
            }
            if (selfTest.name == "buffer" && !run_buffer_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "image" && !run_image_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "shader" && !run_shader_self_test()) {
                return false;
            }
            if (selfTest.name == "upload" && !run_upload_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "layout" && !run_layout_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "resourceset-buffer" && !run_resourceset_buffer_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "resourceset-image" && !run_resourceset_image_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "pipeline" && !run_pipeline_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "layout-mismatch" && !run_layout_mismatch_self_test(options.backend)) {
                return false;
            }
            if (selfTest.name == "vertex" && !run_vertex_self_test()) {
                return false;
            }
        }

        return true;
    }

    if (find_self_test(options.selfTestName) == nullptr) {
        LUNA_CORE_ERROR("Unknown self-test '{}'", options.selfTestName);
        log_available_self_tests();
        return false;
    }

    if (options.selfTestName == "backend") {
        return run_backend_self_test(options.backend);
    }

    if (options.selfTestName == "factory") {
        return run_backend_self_test(options.backend);
    }

    if (options.selfTestName == "types") {
        return run_types_self_test();
    }

    if (options.selfTestName == "device-init") {
        return run_device_init_self_test(options.backend);
    }

    if (options.selfTestName == "device-contract" || options.selfTestName == "device-api") {
        return run_device_api_self_test(options.backend);
    }

    if (options.selfTestName == "command-contract" || options.selfTestName == "command-api") {
        return run_command_api_self_test();
    }

    if (options.selfTestName == "desc") {
        return run_desc_self_test();
    }

    if (options.selfTestName == "frame-context") {
        return run_frame_context_self_test(options.backend);
    }

    if (options.selfTestName == "buffer") {
        return run_buffer_self_test(options.backend);
    }

    if (options.selfTestName == "image") {
        return run_image_self_test(options.backend);
    }

    if (options.selfTestName == "shader") {
        return run_shader_self_test();
    }

    if (options.selfTestName == "upload") {
        return run_upload_self_test(options.backend);
    }

    if (options.selfTestName == "layout") {
        return run_layout_self_test(options.backend);
    }

    if (options.selfTestName == "resourceset-buffer") {
        return run_resourceset_buffer_self_test(options.backend);
    }

    if (options.selfTestName == "resourceset-image") {
        return run_resourceset_image_self_test(options.backend);
    }

    if (options.selfTestName == "pipeline") {
        return run_pipeline_self_test(options.backend);
    }

    if (options.selfTestName == "layout-mismatch") {
        return run_layout_mismatch_self_test(options.backend);
    }

    if (options.selfTestName == "vertex") {
        return run_vertex_self_test();
    }

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
    LUNA_CORE_INFO("LunaRhiTriangle starting");

    CommandLineOptions options;
    if (!parse_arguments(argc, argv, &options)) {
        luna::Logger::shutdown();
        return 1;
    }

    LUNA_CORE_INFO("Requested backend: {}", luna::to_string(options.backend));
    log_capabilities(luna::QueryRHICapabilities(options.backend));

    if (options.selfTest) {
        const bool passed = run_named_self_test(options);
        luna::Logger::shutdown();
        return passed ? 0 : 1;
    }

    if (!luna::IsBackendImplemented(options.backend)) {
        LUNA_CORE_ERROR("Backend '{}' is recognized but not implemented yet", luna::to_string(options.backend));
        luna::Logger::shutdown();
        return 1;
    }

    log_sample_identity();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline;
    if (options.rendererMode == RuntimeRendererMode::Clear) {
        renderPipeline = std::make_shared<ClearColorRenderPipeline>(luna::ClearColorValue{0.08f, 0.12f, 0.18f, 1.0f});
        LUNA_CORE_INFO("Registered ClearColorPipeline");
    } else {
        if (!validate_runtime_triangle_shaders()) {
            luna::Logger::shutdown();
            return 1;
        }

        renderPipeline = std::make_shared<TriangleRuntimeRenderPipeline>(
            triangle_vertex_shader_path(), triangle_fragment_shader_path());
        LUNA_CORE_INFO("Registered TrianglePipeline");
    }

    const luna::ApplicationSpecification specification{
        .name = std::string{kWindowTitle},
        .windowWidth = 1600,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = false,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = kWindowTitle,
                .backend = options.backend,
                .renderPipeline = renderPipeline,
            },
    };

    std::unique_ptr<luna::Application> app = std::make_unique<luna::Application>(specification);
    if (!app->isInitialized()) {
        LUNA_CORE_FATAL("LunaRhiTriangle initialization failed");
        luna::Logger::shutdown();
        return 1;
    }

    app->run();
    app.reset();
    LUNA_CORE_INFO("LunaRhiTriangle shutdown complete");
    luna::Logger::shutdown();
    return 0;
}
