#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/Editor/ImporterManager.h"
#include "Core/Log.h"
#include "LunaRuntime.h"
#include "Project/BuiltinMaterialOverrides.h"
#include "Project/ProjectManager.h"
#include "Scene/SceneSerializer.h"
#include "Script/ScriptPluginManager.h"

#include <Backend.h>
#include <exception>
#include <filesystem>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr luna::RHI::PresentMode kRequestedPresentMode = luna::RHI::PresentMode::Immediate;

struct RuntimeStartupOptions {
    luna::RHI::BackendType backend = luna::RHI::BackendType::Auto;
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

void logRuntimeAssetSyncStats(const luna::ImporterManager::ImportStats& stats)
{
    LUNA_RUNTIME_INFO(
        "Project asset sync: discovered={}, imported_missing={}, loaded_existing={}, rebuilt={}, unsupported={}, "
        "failed={}, missing_after_sync={}",
        stats.discoveredAssets,
        stats.importedMissingAssets,
        stats.loadedExistingMetadata,
        stats.rebuiltMetadata,
        stats.unsupportedFilesSkipped,
        stats.failedAssets,
        stats.missingMetadataAfterSync);
}

std::filesystem::path resolveProjectFilePath(const std::filesystem::path& input_path)
{
    if (input_path.empty()) {
        return {};
    }

    std::error_code ec;
    if (std::filesystem::is_directory(input_path, ec) && !ec) {
        std::filesystem::directory_iterator directory_it(input_path, ec);
        if (ec) {
            return {};
        }

        for (const std::filesystem::directory_iterator directory_end; directory_it != directory_end;
             directory_it.increment(ec)) {
            if (ec) {
                return {};
            }

            const auto& entry = *directory_it;
            std::error_code entry_ec;
            if (entry.is_regular_file(entry_ec) && !entry_ec && entry.path().extension() == ".lunaproj") {
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
                                  luna::RHI::BackendTypeToString(options.backend));
                continue;
            }

            option_value = std::string_view(argv[++i]);
            if (const auto parsed_backend = luna::RHI::ParseBackendType(option_value)) {
                options.backend = *parsed_backend;
            } else {
                LUNA_RUNTIME_WARN("Unsupported backend '{}' requested via command line; defaulting to '{}'",
                                  std::string(option_value),
                                  luna::RHI::BackendTypeToString(options.backend));
            }
            continue;
        }

        if (argument.starts_with("--backend=")) {
            option_value = argument.substr(std::string_view("--backend=").size());
            if (const auto parsed_backend = luna::RHI::ParseBackendType(option_value)) {
                options.backend = *parsed_backend;
            } else {
                LUNA_RUNTIME_WARN("Unsupported backend '{}' requested via command line; defaulting to '{}'",
                                  std::string(option_value),
                                  luna::RHI::BackendTypeToString(options.backend));
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

void logBackendStartupSelection(luna::RHI::BackendType requested_backend)
{
    const std::vector<luna::RHI::BackendType> compiled_backends = luna::RHI::Instance::GetCompiledBackends();
    const std::string compiled_backend_names = luna::RHI::DescribeBackendTypes(compiled_backends);

    LUNA_RUNTIME_INFO("Compiled RHI backends: {}", compiled_backend_names);
    if (requested_backend == luna::RHI::BackendType::Auto) {
        try {
            const luna::RHI::BackendType default_backend = luna::RHI::Instance::GetDefaultBackend();
            LUNA_RUNTIME_INFO("Auto RHI backend will resolve to '{}'", luna::RHI::BackendTypeToString(default_backend));
        } catch (const std::exception& error) {
            LUNA_RUNTIME_WARN("Failed to resolve default RHI backend: {}", error.what());
        }
        return;
    }

    if (!luna::RHI::Instance::IsBackendCompiled(requested_backend)) {
        LUNA_RUNTIME_WARN(
            "Requested RHI backend '{}' is not compiled into this build; renderer initialization will fail. "
            "Compiled backends: {}",
            luna::RHI::BackendTypeToString(requested_backend),
            compiled_backend_names);
    }
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
                      luna::RHI::BackendTypeToString(m_backend),
                      presentModeToString(kRequestedPresentMode));
    return Renderer::InitializationOptions{m_backend, kRequestedPresentMode};
}

void LunaRuntimeApplication::onInit()
{
    auto& renderer = getRenderer();
    renderer.setSceneOutputMode(Renderer::SceneOutputMode::Swapchain);
    renderer.getClearColor() = glm::vec4(0.08f, 0.09f, 0.11f, 1.0f);
    m_scene.setName("Untitled");

    if (!loadStartupScene()) {
        LUNA_RUNTIME_WARN("Runtime started without a loaded scene. Pass '--project <path-to-.lunaproj>' to load one.");
    }
}

void LunaRuntimeApplication::onUpdate(Timestep timestep)
{
    if (m_scene_runtime) {
        m_scene_runtime->update(timestep);
    }
}

void LunaRuntimeApplication::onShutdown()
{
    if (!m_scene_runtime) {
        return;
    }

    m_scene_runtime->stop();
    m_scene_runtime.reset();
}

bool LunaRuntimeApplication::loadStartupScene()
{
    if (m_scene_runtime) {
        m_scene_runtime->stop();
        m_scene_runtime.reset();
    }

    if (m_project_file_path.empty()) {
        return false;
    }

    if (!ProjectManager::instance().loadProject(m_project_file_path)) {
        LUNA_RUNTIME_WARN("Failed to load project '{}'", m_project_file_path.string());
        return false;
    }

    AssetManager::get().clear();
    AssetDatabase::clear();
    const ImporterManager::ImportStats sync_stats = ImporterManager::syncProjectAssets(&getTaskSystem());
    logRuntimeAssetSyncStats(sync_stats);
    AssetManager::get().init();
    BuiltinMaterialOverrides::load();

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
    std::error_code scene_exists_ec;
    const bool start_scene_exists = std::filesystem::exists(start_scene_path, scene_exists_ec);
    if (scene_exists_ec || !start_scene_exists) {
        LUNA_RUNTIME_WARN("Configured StartScene '{}' does not exist", start_scene_path.string());
        return false;
    }

    if (!SceneSerializer::deserialize(m_scene, start_scene_path)) {
        LUNA_RUNTIME_WARN("Failed to load StartScene '{}'", start_scene_path.string());
        return false;
    }

    m_scene_file_path = start_scene_path;
    m_scene_runtime = std::make_unique<SceneRuntime>(m_scene);
    m_scene_runtime->setScriptRuntime(ScriptPluginManager::instance().createRuntimeForProject(&*project_info));
    if (!m_scene_runtime->start()) {
        LUNA_RUNTIME_WARN("Failed to start runtime scene '{}'", m_scene_file_path.string());
        m_scene_runtime.reset();
        return false;
    }

    LUNA_RUNTIME_INFO(
        "Loaded StartScene '{}' with {} entities", m_scene_file_path.string(), m_scene.entityManager().entityCount());
    return true;
}

Application* createApplication(int argc, char** argv)
{
    const RuntimeStartupOptions options = parseStartupOptions(argc, argv);
    LUNA_RUNTIME_INFO("Starting LunaRuntime with requested backend '{}'",
                      luna::RHI::BackendTypeToString(options.backend));
    logBackendStartupSelection(options.backend);

    if (!options.project_file_path.empty()) {
        LUNA_RUNTIME_INFO("Runtime project path: '{}'", options.project_file_path.string());
    } else {
        LUNA_RUNTIME_WARN("No runtime project path provided");
    }

    return new LunaRuntimeApplication(options.backend, options.project_file_path);
}

} // namespace luna
