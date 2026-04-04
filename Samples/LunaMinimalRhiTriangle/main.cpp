#include "Core/application.h"
#include "Core/log.h"

#include <array>
#include <filesystem>
#include <memory>

namespace {

std::filesystem::path shader_root()
{
    return std::filesystem::path{LUNA_MINIMAL_RHI_TRIANGLE_SHADER_ROOT}.lexically_normal();
}

std::array<TriangleVertex, 3> make_triangle_vertices()
{
    return {{
        TriangleVertex{{0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}},
        TriangleVertex{{0.6f, 0.45f}, {0.2f, 1.0f, 0.3f}},
        TriangleVertex{{-0.6f, 0.45f}, {0.2f, 0.4f, 1.0f}},
    }};
}

} // namespace

int main()
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    const std::filesystem::path triangleShaderRoot = shader_root();
    luna::ApplicationSpecification specification{
        .name = "Luna Minimal RHI Triangle",
        .windowWidth = 1280,
        .windowHeight = 720,
        .maximized = false,
        .enableImGui = false,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "Luna Minimal RHI Triangle",
                .backend = luna::RHIBackend::Vulkan,
                .demoMode = VulkanEngine::DemoMode::Triangle,
                .demoClearColor = {0.08f, 0.12f, 0.18f, 1.0f},
                .triangleVertexShaderPath = (triangleShaderRoot / "triangle.vert.spv").generic_string(),
                .triangleFragmentShaderPath = (triangleShaderRoot / "triangle.frag.spv").generic_string(),
            },
    };

    std::unique_ptr<luna::Application> app = std::make_unique<luna::Application>(specification);
    if (!app->isInitialized()) {
        app.reset();
        luna::Logger::shutdown();
        return 1;
    }

    const auto triangleVertices = make_triangle_vertices();
    if (!app->getRenderService().uploadTriangleVertices(triangleVertices)) {
        app.reset();
        luna::Logger::shutdown();
        return 1;
    }

    app->run();
    app.reset();
    luna::Logger::shutdown();
    return 0;
}
