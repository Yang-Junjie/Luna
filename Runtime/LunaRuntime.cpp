#include "LunaRuntime.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/Editor/ImporterManager.h"
#include "Core/Log.h"
#include "Project/ProjectManager.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr luna::RHI::PresentMode kRequestedPresentMode = luna::RHI::PresentMode::Immediate;

struct RuntimeStartupOptions {
    luna::RHI::BackendType backend = luna::RHI::BackendType::Vulkan;
    std::filesystem::path project_file_path;
};

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

std::filesystem::path resolveProjectFilePath(const std::filesystem::path& input_path)
{
    if (input_path.empty()) {
        return {};
    }

    std::error_code ec;
    if (std::filesystem::is_directory(input_path, ec) && !ec) {
        for (const auto& entry : std::filesystem::directory_iterator(input_path, ec)) {
            if (ec) {
                return {};
            }

            if (entry.is_regular_file() && entry.path().extension() == ".lunaproj") {
                return entry.path().lexically_normal();
            }
        }

        return {};
    }

    return input_path.lexically_normal();
}

RuntimeStartupOptions parseStartupOptions(int argc, char** argv)
{
    RuntimeStartupOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i] != nullptr ? std::string_view(argv[i]) : std::string_view{};
        std::string_view option_value;

        if (argument == "--backend") {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                LUNA_RUNTIME_WARN("Missing value after '--backend'; defaulting to '{}'",
                                  backendTypeToString(options.backend));
                continue;
            }

            option_value = std::string_view(argv[++i]);
            if (const auto parsed_backend = parseBackendValue(option_value)) {
                options.backend = *parsed_backend;
            } else {
                LUNA_RUNTIME_WARN("Unsupported backend '{}' requested via command line; defaulting to '{}'",
                                  std::string(option_value),
                                  backendTypeToString(options.backend));
            }
            continue;
        }

        if (argument.starts_with("--backend=")) {
            option_value = argument.substr(std::string_view("--backend=").size());
            if (const auto parsed_backend = parseBackendValue(option_value)) {
                options.backend = *parsed_backend;
            } else {
                LUNA_RUNTIME_WARN("Unsupported backend '{}' requested via command line; defaulting to '{}'",
                                  std::string(option_value),
                                  backendTypeToString(options.backend));
            }
            continue;
        }

        if (argument == "--project") {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                LUNA_RUNTIME_WARN("Missing value after '--project'; runtime will start without a loaded project");
                continue;
            }

            options.project_file_path = resolveProjectFilePath(argv[++i]);
            continue;
        }

        if (argument.starts_with("--project=")) {
            option_value = argument.substr(std::string_view("--project=").size());
            options.project_file_path = resolveProjectFilePath(std::filesystem::path(option_value));
            continue;
        }
    }

    return options;
}

} // namespace

namespace luna {

LunaRuntimeApplication::LunaRuntimeApplication(luna::RHI::BackendType backend, std::filesystem::path project_file_path)
    : Application(ApplicationSpecification{
          .m_name = "Luna Runtime",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_imgui = false,
          .m_enable_multi_viewport = false,
      }),
      m_project_file_path(std::move(project_file_path)),
      m_backend(backend)
{}

Renderer::InitializationOptions LunaRuntimeApplication::getRendererInitializationOptions()
{
    LUNA_RUNTIME_INFO("LunaRuntime requested backend '{}' and present mode '{}' via code",
                      backendTypeToString(m_backend),
                      presentModeToString(kRequestedPresentMode));
    return Renderer::InitializationOptions{m_backend, kRequestedPresentMode};
}

void LunaRuntimeApplication::resetCamera()
{
    auto& camera = getRenderer().getMainCamera();
    camera.m_position = glm::vec3(0.0f, 0.45f, 4.75f);
    camera.m_pitch = -0.12f;
    camera.m_yaw = 0.0f;
}

void LunaRuntimeApplication::onInit()
{
    auto& renderer = getRenderer();
    renderer.setSceneOutputMode(Renderer::SceneOutputMode::Swapchain);
    renderer.getClearColor() = glm::vec4(0.08f, 0.09f, 0.11f, 1.0f);
    resetCamera();
    m_scene.setName("Untitled");

    if (!loadStartupScene()) {
        LUNA_RUNTIME_WARN("Runtime started without a loaded scene. Pass '--project <path-to-.lunaproj>' to load one.");
    }
}

void LunaRuntimeApplication::onUpdate(Timestep)
{
    m_scene.onUpdateRuntime();
}

bool LunaRuntimeApplication::loadStartupScene()
{
    if (m_project_file_path.empty()) {
        return false;
    }

    if (!ProjectManager::instance().loadProject(m_project_file_path)) {
        LUNA_RUNTIME_WARN("Failed to load project '{}'", m_project_file_path.string());
        return false;
    }

    AssetManager::get().clear();
    AssetDatabase::clear();
    ImporterManager::import();
    AssetManager::get().init();

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_root || !project_info) {
        LUNA_RUNTIME_WARN("Project '{}' was loaded but project state is incomplete", m_project_file_path.string());
        return false;
    }

    if (project_info->StartScene.empty()) {
        LUNA_RUNTIME_WARN("Project '{}' does not define StartScene", m_project_file_path.string());
        return false;
    }

    const std::filesystem::path start_scene_path =
        SceneSerializer::normalizeScenePath((*project_root / project_info->StartScene).lexically_normal());
    if (!std::filesystem::exists(start_scene_path)) {
        LUNA_RUNTIME_WARN("Configured StartScene '{}' does not exist", start_scene_path.string());
        return false;
    }

    if (!SceneSerializer::deserialize(m_scene, start_scene_path)) {
        LUNA_RUNTIME_WARN("Failed to load StartScene '{}'", start_scene_path.string());
        return false;
    }

    m_scene_file_path = start_scene_path;
    LUNA_RUNTIME_INFO("Loaded StartScene '{}' with {} entities",
                      m_scene_file_path.string(),
                      m_scene.entityCount());
    return true;
}

Application* createApplication(int argc, char** argv)
{
    const RuntimeStartupOptions options = parseStartupOptions(argc, argv);
    LUNA_RUNTIME_INFO("Starting LunaRuntime with backend '{}'", backendTypeToString(options.backend));

    if (!options.project_file_path.empty()) {
        LUNA_RUNTIME_INFO("Runtime project path: '{}'", options.project_file_path.string());
    } else {
        LUNA_RUNTIME_WARN("No runtime project path provided");
    }

    return new LunaRuntimeApplication(options.backend, options.project_file_path);
}

} // namespace luna
