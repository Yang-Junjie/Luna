#include "Core/Log.h"
#include "LunaEditorApp.h"
#include "LunaEditorLayer.h"

#include <cctype>
#include <algorithm>
#include <optional>
#include <string_view>

namespace {

constexpr luna::RHI::PresentMode kRequestedPresentMode = luna::RHI::PresentMode::Immediate;

bool backendSupportsImGui(luna::RHI::BackendType backend)
{
    return backend == luna::RHI::BackendType::Vulkan || backend == luna::RHI::BackendType::DirectX11 ||
           backend == luna::RHI::BackendType::DirectX12;
}

const char* presentModeToString(luna::RHI::PresentMode mode)
{
    switch (mode) {
        case luna::RHI::PresentMode::Immediate:
            return "Immediate";
        case luna::RHI::PresentMode::Mailbox:
            return "Mailbox";
        case luna::RHI::PresentMode::Fifo:
            return "Fifo";
        case luna::RHI::PresentMode::FifoRelaxed:
            return "FifoRelaxed";
        default:
            return "Unknown";
    }
}

const char* backendTypeToString(luna::RHI::BackendType type)
{
    switch (type) {
        case luna::RHI::BackendType::Auto:
            return "Auto";
        case luna::RHI::BackendType::Vulkan:
            return "Vulkan";
        case luna::RHI::BackendType::DirectX12:
            return "DirectX12";
        case luna::RHI::BackendType::DirectX11:
            return "DirectX11";
        case luna::RHI::BackendType::Metal:
            return "Metal";
        case luna::RHI::BackendType::OpenGL:
            return "OpenGL";
        case luna::RHI::BackendType::OpenGLES:
            return "OpenGLES";
        case luna::RHI::BackendType::WebGPU:
            return "WebGPU";
        default:
            return "Unknown";
    }
}

std::optional<luna::RHI::BackendType> parseBackendValue(std::string_view value)
{
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "vulkan" || normalized == "vk") {
        return luna::RHI::BackendType::Vulkan;
    }
    if (normalized == "d3d12" || normalized == "dx12" || normalized == "directx12") {
        return luna::RHI::BackendType::DirectX12;
    }
    if (normalized == "d3d11" || normalized == "dx11" || normalized == "directx11") {
        return luna::RHI::BackendType::DirectX11;
    }

    return std::nullopt;
}

luna::RHI::BackendType parseBackendFromArgs(int argc, char** argv)
{
    luna::RHI::BackendType selected_backend = luna::RHI::BackendType::Vulkan;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i] != nullptr ? std::string_view(argv[i]) : std::string_view{};
        std::string_view backend_value;

        if (argument == "--backend") {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                LUNA_EDITOR_WARN("Missing value after '--backend'; defaulting to '{}'",
                                 backendTypeToString(selected_backend));
                continue;
            }

            backend_value = std::string_view(argv[++i]);
        } else if (argument.starts_with("--backend=")) {
            backend_value = argument.substr(std::string_view("--backend=").size());
        } else {
            continue;
        }

        if (const auto parsed = parseBackendValue(backend_value)) {
            selected_backend = *parsed;
            continue;
        }

        LUNA_EDITOR_WARN("Unsupported backend '{}' requested via command line; defaulting to '{}'",
                         std::string(backend_value),
                         backendTypeToString(selected_backend));
    }

    return selected_backend;
}

} // namespace

namespace luna {

LunaEditorApplication::LunaEditorApplication(luna::RHI::BackendType backend)
    : Application(ApplicationSpecification{
          .m_name = "Luna Editor",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_imgui = backendSupportsImGui(backend),
          .m_enable_multi_viewport = false,
      }),
      m_backend(backend)
{}

luna::RHI::BackendType LunaEditorApplication::getBackend() const
{
    return m_backend;
}

Renderer::InitializationOptions LunaEditorApplication::getRendererInitializationOptions()
{
    LUNA_EDITOR_INFO("LunaEditor requested backend '{}' and present mode '{}' via code",
                     backendTypeToString(m_backend),
                     presentModeToString(kRequestedPresentMode));
    return Renderer::InitializationOptions{m_backend, kRequestedPresentMode};
}

void LunaEditorApplication::onInit()
{
    pushOverlay(std::make_unique<LunaEditorLayer>(*this));
}

Application* createApplication(int argc, char** argv)
{
    const auto backend = parseBackendFromArgs(argc, argv);
    LUNA_EDITOR_INFO("Starting LunaEditor with backend '{}'", backendTypeToString(backend));
    return new LunaEditorApplication(backend);
}

} // namespace luna
