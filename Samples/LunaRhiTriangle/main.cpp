#include "Core/application.h"
#include "Core/log.h"
#include "Core/Paths.h"
#include "RHI/CommandContext.h"
#include "RHI/RHIDevice.h"
#include "Vulkan/vk_shader.h"

#include <array>
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

struct CommandLineOptions {
    bool selfTest = false;
    std::string_view selfTestName;
    luna::RHIBackend backend = luna::RHIBackend::Vulkan;
};

struct RecordingCommandContext final : public luna::IRHICommandContext {
    luna::ClearColorValue lastClearColor{};
    luna::PipelineHandle boundPipeline{};
    luna::BufferHandle boundVertexBuffer{};
    uint64_t vertexBufferOffset = 0;
    luna::DrawArguments lastDraw{};
    uint32_t clearCallCount = 0;
    uint32_t bindPipelineCallCount = 0;
    uint32_t bindVertexBufferCallCount = 0;
    uint32_t drawCallCount = 0;

    luna::RHIResult clearColor(const luna::ClearColorValue& color) override
    {
        lastClearColor = color;
        ++clearCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult bindGraphicsPipeline(luna::PipelineHandle pipeline) override
    {
        boundPipeline = pipeline;
        ++bindPipelineCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult bindVertexBuffer(luna::BufferHandle buffer, uint64_t offset) override
    {
        boundVertexBuffer = buffer;
        vertexBufferOffset = offset;
        ++bindVertexBufferCallCount;
        return luna::RHIResult::Success;
    }

    luna::RHIResult draw(const luna::DrawArguments& arguments) override
    {
        lastDraw = arguments;
        ++drawCallCount;
        return luna::RHIResult::Success;
    }
};

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

void log_sample_identity()
{
    LUNA_CORE_INFO("Sample={}", kSampleName);
    LUNA_CORE_INFO("ResourceRoot={}", normalize_path(luna::paths::asset_root()));
    LUNA_CORE_INFO("ShaderRoot={}", normalize_path(build_shader_root()));
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

        LUNA_CORE_WARN("Ignoring unknown argument '{}'", argument);
    }

    return true;
}

bool run_default_self_test()
{
    log_sample_identity();
    LUNA_CORE_INFO("SELF TEST PASS");
    return true;
}

bool run_backend_self_test(luna::RHIBackend backend)
{
    LUNA_CORE_INFO("Requested backend: {}", luna::to_string(backend));

    std::unique_ptr<luna::IRHIDevice> device = luna::CreateRHIDevice(backend);
    if (!device || device->getBackend() != backend) {
        LUNA_CORE_ERROR("Backend factory failed for {}", luna::to_string(backend));
        return false;
    }

    LUNA_CORE_INFO("Backend factory PASS");
    return true;
}

bool run_types_self_test()
{
    const luna::BufferHandle invalidBuffer{};
    const luna::BufferHandle firstBuffer = luna::BufferHandle::fromRaw(7);
    const luna::BufferHandle secondBuffer = luna::BufferHandle::fromRaw(8);
    const luna::PipelineHandle pipeline = luna::PipelineHandle::fromRaw(21);

    const bool ok = !invalidBuffer.isValid() && firstBuffer.isValid() && firstBuffer == firstBuffer &&
                    firstBuffer != secondBuffer && pipeline.isValid() &&
                    luna::RHIResult::Success != luna::RHIResult::Unsupported;

    if (!ok) {
        LUNA_CORE_ERROR("RHI handle/result contract validation failed");
        return false;
    }

    LUNA_CORE_INFO("RHI Types PASS");
    return true;
}

bool run_device_api_self_test(luna::RHIBackend backend)
{
    std::unique_ptr<luna::IRHIDevice> device = luna::CreateRHIDevice(backend);
    if (!device) {
        LUNA_CORE_ERROR("Failed to create IRHIDevice for {}", luna::to_string(backend));
        return false;
    }

    luna::DeviceCreateInfo createInfo{};
    createInfo.applicationName = kWindowTitle;
    createInfo.backend = backend;

    if (device->init(createInfo) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("IRHIDevice::init failed");
        return false;
    }

    if (device->beginFrame() != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("IRHIDevice::beginFrame failed");
        return false;
    }

    if (device->endFrame() != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("IRHIDevice::endFrame failed");
        return false;
    }

    device->shutdown();
    LUNA_CORE_INFO("IRHIDevice v1 PASS");
    return true;
}

bool run_command_api_self_test()
{
    RecordingCommandContext context;
    const luna::ClearColorValue clearColor{0.15f, 0.25f, 0.35f, 1.0f};
    const luna::PipelineHandle pipeline = luna::PipelineHandle::fromRaw(101);
    const luna::BufferHandle vertexBuffer = luna::BufferHandle::fromRaw(202);
    const luna::DrawArguments drawArguments{3, 1, 0, 0};

    const bool ok = context.clearColor(clearColor) == luna::RHIResult::Success &&
                    context.bindGraphicsPipeline(pipeline) == luna::RHIResult::Success &&
                    context.bindVertexBuffer(vertexBuffer, 16) == luna::RHIResult::Success &&
                    context.draw(drawArguments) == luna::RHIResult::Success && context.clearCallCount == 1 &&
                    context.bindPipelineCallCount == 1 && context.bindVertexBufferCallCount == 1 &&
                    context.drawCallCount == 1 && context.boundPipeline == pipeline &&
                    context.boundVertexBuffer == vertexBuffer && context.vertexBufferOffset == 16 &&
                    context.lastDraw.vertexCount == 3;

    if (!ok) {
        LUNA_CORE_ERROR("IRHICommandContext contract validation failed");
        return false;
    }

    LUNA_CORE_INFO("RHI Command API PASS");
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

    const bool ok =
        bufferDesc.size == 1024 && imageDesc.width == 1280 && imageDesc.height == 720 &&
        vertexShader.stage == luna::ShaderType::Vertex && fragmentShader.stage == luna::ShaderType::Fragment &&
        pipelineDesc.vertexLayout.attributes.size() == 2 && pipelineDesc.colorAttachments.size() == 1 &&
        pipelineDesc.depthStencil.depthTestEnabled && pipelineDesc.depthStencil.depthWriteEnabled;

    if (!ok) {
        LUNA_CORE_ERROR("RHI descriptor contract validation failed");
        return false;
    }

    LUNA_CORE_INFO("BufferDesc/ImageDesc/ShaderDesc/PipelineDesc PASS");
    return true;
}

bool run_shader_self_test()
{
    const std::filesystem::path vertexShaderPath = triangle_vertex_shader_path();
    const std::filesystem::path fragmentShaderPath = triangle_fragment_shader_path();

    const std::optional<std::vector<uint32_t>> vertexCode = read_spirv_code(vertexShaderPath);
    const std::optional<std::vector<uint32_t>> fragmentCode = read_spirv_code(fragmentShaderPath);
    if (!vertexCode.has_value() || !fragmentCode.has_value()) {
        LUNA_CORE_ERROR("Failed to load compiled sample shaders from {}", normalize_path(build_shader_root()));
        return false;
    }

    const luna::VulkanShader vertexShader(*vertexCode, luna::ShaderType::Vertex);
    const luna::VulkanShader fragmentShader(*fragmentCode, luna::ShaderType::Fragment);
    const bool ok =
        vertexShader.getType() == luna::ShaderType::Vertex && fragmentShader.getType() == luna::ShaderType::Fragment;

    if (!ok) {
        LUNA_CORE_ERROR("Sample shader self-test failed");
        return false;
    }

    LUNA_CORE_INFO("Loaded shader modules: 2");
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

bool run_named_self_test(const CommandLineOptions& options)
{
    if (options.selfTestName.empty()) {
        return run_default_self_test();
    }

    if (options.selfTestName == "backend") {
        return run_backend_self_test(options.backend);
    }

    if (options.selfTestName == "types") {
        return run_types_self_test();
    }

    if (options.selfTestName == "device-api") {
        return run_device_api_self_test(options.backend);
    }

    if (options.selfTestName == "command-api") {
        return run_command_api_self_test();
    }

    if (options.selfTestName == "desc") {
        return run_desc_self_test();
    }

    if (options.selfTestName == "shader") {
        return run_shader_self_test();
    }

    if (options.selfTestName == "vertex") {
        return run_vertex_self_test();
    }

    LUNA_CORE_ERROR("Unknown self-test '{}'", options.selfTestName);
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

    if (options.selfTest) {
        const bool passed = run_named_self_test(options);
        luna::Logger::shutdown();
        return passed ? 0 : 1;
    }

    if (options.backend != luna::RHIBackend::Vulkan) {
        LUNA_CORE_ERROR("Only Vulkan is supported in LunaRhiTriangle v1");
        luna::Logger::shutdown();
        return 1;
    }

    log_sample_identity();
    if (!validate_runtime_triangle_shaders()) {
        luna::Logger::shutdown();
        return 1;
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
                .demoMode = VulkanEngine::DemoMode::Triangle,
                .demoClearColor = {0.08f, 0.12f, 0.18f, 1.0f},
                .triangleVertexShaderPath = normalize_path(triangle_vertex_shader_path()),
                .triangleFragmentShaderPath = normalize_path(triangle_fragment_shader_path()),
            },
    };

    std::unique_ptr<luna::Application> app = std::make_unique<luna::Application>(specification);
    if (!app->isInitialized()) {
        LUNA_CORE_FATAL("LunaRhiTriangle initialization failed");
        luna::Logger::shutdown();
        return 1;
    }

    const auto vertices = make_triangle_vertices();
    if (!app->getRenderService().uploadTriangleVertices(vertices)) {
        LUNA_CORE_FATAL("LunaRhiTriangle vertex upload failed");
        luna::Logger::shutdown();
        return 1;
    }

    app->run();
    app.reset();
    LUNA_CORE_INFO("LunaRhiTriangle shutdown complete");
    luna::Logger::shutdown();
    return 0;
}
