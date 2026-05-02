#include "Core/Log.h"
#include "LunaEditorApp.h"
#include "LunaEditorLayer.h"

#include <Backend.h>
#include <Capabilities.h>

#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr luna::RHI::PresentMode kRequestedPresentMode = luna::RHI::PresentMode::Immediate;

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

luna::RHI::BackendType parseBackendFromArgs(int argc, char** argv)
{
    luna::RHI::BackendType selected_backend = luna::RHI::BackendType::Auto;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i] != nullptr ? std::string_view(argv[i]) : std::string_view{};
        std::string_view backend_value;

        if (argument == "--backend") {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                LUNA_EDITOR_WARN("Missing value after '--backend'; defaulting to '{}'",
                                 luna::RHI::BackendTypeToString(selected_backend));
                continue;
            }

            backend_value = std::string_view(argv[++i]);
        } else if (argument.starts_with("--backend=")) {
            backend_value = argument.substr(std::string_view("--backend=").size());
        } else {
            continue;
        }

        if (const auto parsed = luna::RHI::ParseBackendType(backend_value)) {
            selected_backend = *parsed;
            continue;
        }

        LUNA_EDITOR_WARN("Unsupported backend '{}' requested via command line; defaulting to '{}'",
                         std::string(backend_value),
                         luna::RHI::BackendTypeToString(selected_backend));
    }

    return selected_backend;
}

void logBackendStartupSelection(luna::RHI::BackendType requested_backend)
{
    const std::vector<luna::RHI::BackendType> compiled_backends = luna::RHI::Instance::GetCompiledBackends();
    const std::string compiled_backend_names = luna::RHI::DescribeBackendTypes(compiled_backends);

    LUNA_EDITOR_INFO("Compiled RHI backends: {}", compiled_backend_names);
    if (requested_backend == luna::RHI::BackendType::Auto) {
        try {
            const luna::RHI::BackendType default_backend = luna::RHI::Instance::GetDefaultBackend();
            LUNA_EDITOR_INFO("Auto RHI backend will resolve to '{}'",
                             luna::RHI::BackendTypeToString(default_backend));
        } catch (const std::exception& error) {
            LUNA_EDITOR_WARN("Failed to resolve default RHI backend: {}", error.what());
        }
        return;
    }

    if (!luna::RHI::Instance::IsBackendCompiled(requested_backend)) {
        LUNA_EDITOR_WARN("Requested RHI backend '{}' is not compiled into this build; renderer initialization will fail. "
                         "Compiled backends: {}",
                         luna::RHI::BackendTypeToString(requested_backend),
                         compiled_backend_names);
    }
}

luna::RHI::BackendType resolveCapabilitiesBackend(luna::RHI::BackendType backend)
{
    if (backend != luna::RHI::BackendType::Auto) {
        return backend;
    }

    try {
        return luna::RHI::Instance::GetDefaultBackend();
    } catch (const std::exception& error) {
        LUNA_EDITOR_WARN("Failed to resolve default RHI backend for editor capabilities: {}", error.what());
        return luna::RHI::BackendType::Auto;
    }
}

} // namespace

namespace luna {

LunaEditorApplication::LunaEditorApplication(luna::RHI::BackendType backend)
    : Application(ApplicationSpecification{
          .m_name = "Luna Editor",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_imgui = luna::RHI::makeCapabilitiesForBackend(resolveCapabilitiesBackend(backend)).supports_imgui,
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
                     luna::RHI::BackendTypeToString(m_backend),
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
    LUNA_EDITOR_INFO("Starting LunaEditor with requested backend '{}'", luna::RHI::BackendTypeToString(backend));
    logBackendStartupSelection(backend);
    return new LunaEditorApplication(backend);
}

} // namespace luna
