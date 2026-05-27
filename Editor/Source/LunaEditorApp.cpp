#include "Core/Log.h"
#include "EditorStyle.h"
#include "LunaEditorApp.h"
#include "LunaEditorLayer.h"

#include <Backend.h>
#include <Capabilities.h>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr luna::RHI::PresentMode kRequestedPresentMode = luna::RHI::PresentMode::Immediate;
constexpr const char* kDefaultEditorFontRelativePath = "Editor/Asset/fonts/Play-Regular.ttf";
constexpr float kDefaultEditorFontSizePixels = 16.0f;

std::filesystem::path sourceRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT).lexically_normal();
}

luna::ImGuiFontConfig defaultEditorFontConfig()
{
    return luna::ImGuiFontConfig{
        .font_path = sourceRoot() / kDefaultEditorFontRelativePath,
        .size_pixels = kDefaultEditorFontSizePixels,
    };
}

luna::editor::EditorSettingsStore makeEditorSettingsStore(const luna::editor::EditorEnginePaths& engine_paths)
{
    std::vector<std::filesystem::path> font_roots;
    font_roots.push_back(engine_paths.engine_resources_root / "fonts");
    font_roots.push_back(engine_paths.engine_resources_root / "Editor" / "fonts");
    font_roots.push_back(sourceRoot() / "Editor" / "Asset" / "fonts");

    std::filesystem::path default_font_path = defaultEditorFontConfig().font_path;
    const std::filesystem::path installed_default_font =
        engine_paths.engine_resources_root / "fonts" / "Play-Regular.ttf";
    std::error_code exists_ec;
    if (std::filesystem::exists(installed_default_font, exists_ec) && !exists_ec) {
        default_font_path = installed_default_font;
    }

    luna::editor::EditorSettingsStore settings{
        engine_paths.engine_data_root / "Settings" / "EditorSettings.yaml",
        std::move(default_font_path),
        std::move(font_roots),
    };
    (void) settings.load();
    return settings;
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
            LUNA_EDITOR_INFO("Auto RHI backend will resolve to '{}'", luna::RHI::BackendTypeToString(default_backend));
        } catch (const std::exception& error) {
            LUNA_EDITOR_WARN("Failed to resolve default RHI backend: {}", error.what());
        }
        return;
    }

    if (!luna::RHI::Instance::IsBackendCompiled(requested_backend)) {
        LUNA_EDITOR_WARN(
            "Requested RHI backend '{}' is not compiled into this build; renderer initialization will fail. "
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

LunaEditorApplication::LunaEditorApplication(luna::RHI::BackendType backend, editor::EditorEnginePaths engine_paths)
    : LunaEditorApplication(backend, engine_paths, makeEditorSettingsStore(engine_paths))
{}

LunaEditorApplication::LunaEditorApplication(luna::RHI::BackendType backend,
                                             editor::EditorEnginePaths engine_paths,
                                             editor::EditorSettingsStore editor_settings)
    : Application(ApplicationSpecification{
          .m_name = "Luna Editor",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_imgui = luna::RHI::makeCapabilitiesForBackend(resolveCapabilitiesBackend(backend)).supports_imgui,
          .m_enable_multi_viewport = false,
          .m_imgui_font = editor_settings.imguiFontConfig(),
      }),
      m_backend(backend),
      m_engine_paths(std::move(engine_paths)),
      m_editor_settings(std::move(editor_settings))
{}

luna::RHI::BackendType LunaEditorApplication::getBackend() const
{
    return m_backend;
}

const editor::EditorEnginePaths& LunaEditorApplication::enginePaths() const noexcept
{
    return m_engine_paths;
}

editor::EditorSettingsStore& LunaEditorApplication::editorSettings() noexcept
{
    return m_editor_settings;
}

const editor::EditorSettingsStore& LunaEditorApplication::editorSettings() const noexcept
{
    return m_editor_settings;
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
    if (auto* imgui_layer = getImGuiLayer(); imgui_layer != nullptr && imgui_layer->isInitialized()) {
        editor::applyEditorTheme(m_editor_settings.data().theme_preset);
    }
    pushOverlay(std::make_unique<LunaEditorLayer>(*this));
}

Application* createApplication(int argc, char** argv)
{
    const auto backend = parseBackendFromArgs(argc, argv);
    const editor::EditorStartupOptions startup_options = editor::parseEditorStartupOptions(argc, argv);
    editor::EditorEnginePaths engine_paths = editor::resolveEditorEnginePaths(startup_options);
    LUNA_EDITOR_INFO("Starting LunaEditor with requested backend '{}'", luna::RHI::BackendTypeToString(backend));
    logBackendStartupSelection(backend);
    return new LunaEditorApplication(backend, std::move(engine_paths));
}

} // namespace luna
