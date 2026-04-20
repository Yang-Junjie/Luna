#include "LunaEditorLayer.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/Editor/ImporterManager.h"
#include "Core/Log.h"
#include "Imgui/ImGuiContext.h"
#include "LunaEditorApp.h"
#include "Platform/Common/FileDialogs.h"
#include "Project/ProjectInfo.h"
#include "Project/ProjectManager.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <filesystem>
#include <imgui.h>
#include <optional>
#include <string>
#include <system_error>

namespace {

constexpr const char* kProjectFileFilter = "Luna Project (*.lunaproj)\0*.lunaproj\0";
constexpr const char* kSceneFileFilter = "Luna Scene (*.lunascene)\0*.lunascene\0";

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

void logEditorAssetSyncStats(const luna::ImporterManager::ImportStats& stats)
{
    LUNA_EDITOR_INFO(
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

std::filesystem::path projectDialogDefaultPath()
{
    if (const auto project_root = luna::ProjectManager::instance().getProjectRootPath()) {
        return *project_root;
    }

    return std::filesystem::current_path();
}

std::optional<std::filesystem::path> makeScenePathRelativeToProject(const std::filesystem::path& scene_file_path)
{
    const auto project_root = luna::ProjectManager::instance().getProjectRootPath();
    if (!project_root || scene_file_path.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(scene_file_path, *project_root, ec);
    if (ec) {
        return std::nullopt;
    }

    relative_path = relative_path.lexically_normal();
    if (relative_path.empty() || relative_path.is_absolute()) {
        return std::nullopt;
    }

    const std::string relative_string = relative_path.generic_string();
    if (relative_string == "." || relative_string.starts_with("..")) {
        return std::nullopt;
    }

    return relative_path;
}

} // namespace

namespace luna {

LunaEditorLayer::LunaEditorLayer(LunaEditorApplication& application)
    : Layer("LunaEditorLayer"),
      m_application(&application),
      m_scene_hierarchy_panel(*this),
      m_inspector_panel(*this)
{}

void LunaEditorLayer::onAttach()
{
    if (m_application == nullptr) {
        return;
    }

    createScene();

    if (m_application->getImGuiLayer() != nullptr) {
        m_application->getRenderer().setSceneOutputMode(Renderer::SceneOutputMode::OffscreenTexture);
    } else {
        m_application->getRenderer().setSceneOutputMode(Renderer::SceneOutputMode::Swapchain);
        LUNA_EDITOR_INFO("ImGui overlay disabled for backend '{}'", backendTypeToString(m_application->getBackend()));
    }

    if (auto* imgui_layer = m_application->getImGuiLayer(); imgui_layer != nullptr) {
        imgui_layer->setMenuBarCallback([this]() {
            onImGuiMenuBar();
        });
    }
}

void LunaEditorLayer::onDetach()
{
    if (m_application == nullptr) {
        return;
    }

    if (auto* imgui_layer = m_application->getImGuiLayer(); imgui_layer != nullptr) {
        imgui_layer->setMenuBarCallback({});
    }
}

void LunaEditorLayer::onUpdate(Timestep)
{
    m_scene.onUpdateRuntime();
}

void LunaEditorLayer::onImGuiRender()
{
    if (m_application == nullptr) {
        return;
    }

    auto& application = *m_application;
    const float delta_seconds = Application::get().getTimestep().getSeconds();
    const float fps = 1.0f / (std::max)(delta_seconds, 0.0001f);

    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene");
    ImGui::Text("Backend: Luna RHI / %s", backendTypeToString(application.getBackend()));
    ImGui::Text("Frame: %.2f ms  |  %.1f FPS", delta_seconds * 1000.0f, fps);
    ImGui::Separator();
    ImGui::Text("Scene File: %s", m_asset_label.c_str());
    ImGui::Text("Entities: %zu", m_scene.entityManager().entityCount());
    ImGui::Separator();

    const auto viewport_extent = application.getRenderer().getSceneOutputSize();
    ImGui::Text("Viewport: %u x %u", viewport_extent.width, viewport_extent.height);
    ImGui::TextUnformatted(
        "Scene rendering now targets a persistent offscreen texture and is presented in the Viewport panel.");
    ImGui::End();

    m_scene_hierarchy_panel.onImGuiRender();
    m_inspector_panel.onImGuiRender();
    drawViewport();
}

void LunaEditorLayer::onImGuiMenuBar()
{
    const bool project_loaded = hasProjectLoaded();

    if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem("Open Project")) {
            const std::filesystem::path project_file_path =
                FileDialogs::openFile(kProjectFileFilter, projectDialogDefaultPath().string());
            if (!project_file_path.empty()) {
                openProject(project_file_path);
            }
        }

        if (ImGui::MenuItem("Create New Project")) {
            const std::filesystem::path project_root_path =
                FileDialogs::selectDirectory(projectDialogDefaultPath().string());
            if (!project_root_path.empty()) {
                ProjectInfo project_info{.Name = "New Project",
                                         .Version = "0.1.0",
                                         .Author = "Junjie Yang",
                                         .Description = "A simple Luna project.",
                                         .StartScene = "./Assets/Scenes/Main.lunascene",
                                         .AssetsPath = "./Assets/"};

                if (ProjectManager::instance().createProject(project_root_path, project_info)) {
                    std::error_code ec;
                    if (!project_info.AssetsPath.empty()) {
                        std::filesystem::create_directories(
                            (project_root_path / project_info.AssetsPath).lexically_normal(), ec);
                    }

                    ec.clear();
                    if (!project_info.StartScene.empty()) {
                        const auto scene_directory =
                            (project_root_path / project_info.StartScene).lexically_normal().parent_path();
                        if (!scene_directory.empty()) {
                            std::filesystem::create_directories(scene_directory, ec);
                        }
                    }

                    openProject(project_root_path / (project_info.Name + ".lunaproj"));
                }
            }
        }

        if (ImGui::MenuItem("Sync Assets", nullptr, false, project_loaded)) {
            syncProjectAssets();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene", project_loaded)) {
        if (ImGui::MenuItem("Create Scene")) {
            createScene();
        }

        if (ImGui::MenuItem("Open Scene")) {
            openScene();
        }

        if (ImGui::MenuItem("Save Scene")) {
            saveScene();
        }
        ImGui::EndMenu();
    }
}

void LunaEditorLayer::drawViewport()
{
    if (m_application == nullptr) {
        return;
    }

    auto& renderer = m_application->getRenderer();

    ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float dpi_scale = ImGui::GetWindowViewport() != nullptr ? ImGui::GetWindowViewport()->DpiScale : 1.0f;
    const uint32_t viewport_width = static_cast<uint32_t>((std::max)(available.x * dpi_scale, 0.0f));
    const uint32_t viewport_height = static_cast<uint32_t>((std::max)(available.y * dpi_scale, 0.0f));
    renderer.setSceneOutputSize(viewport_width, viewport_height);

    const auto& scene_texture = renderer.getSceneOutputTexture();
    const ImTextureID texture_id = rhi::ImGuiRhiContext::GetTextureId(scene_texture);
    if (texture_id != 0 && available.x > 0.0f && available.y > 0.0f) {
        ImGui::Image(texture_id, available);
    } else if (available.x > 0.0f && available.y > 0.0f) {
        ImGui::SetCursorPos(ImVec2(16.0f, 16.0f));
        ImGui::TextUnformatted("Viewport texture will appear after the first rendered frame.");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

const std::string& LunaEditorLayer::getAssetLabel() const
{
    return m_asset_label;
}

Scene& LunaEditorLayer::getScene()
{
    return m_scene;
}

Entity LunaEditorLayer::getSelectedEntity() const
{
    return m_selected_entity;
}

void LunaEditorLayer::setSelectedEntity(Entity entity)
{
    m_selected_entity = entity;
}

void LunaEditorLayer::resetEditorState()
{
    m_scene.entityManager().clear();
    m_scene.setName("Untitled");
    m_selected_entity = {};
    m_scene_file_path.clear();
    m_asset_label = "No scene loaded";
}

void LunaEditorLayer::createScene()
{
    resetEditorState();
    updateSceneLabel();
    LUNA_EDITOR_INFO("Created a new empty scene");
}

bool LunaEditorLayer::syncProjectAssets()
{
    if (!hasProjectLoaded()) {
        LUNA_EDITOR_WARN("Cannot sync assets because no project is currently loaded");
        return false;
    }

    const ImporterManager::ImportStats stats = ImporterManager::syncProjectAssets();
    logEditorAssetSyncStats(stats);
    return stats.failedAssets == 0 && stats.missingMetadataAfterSync == 0;
}

bool LunaEditorLayer::openProject(const std::filesystem::path& project_file_path)
{
    if (project_file_path.empty()) {
        return false;
    }

    if (!ProjectManager::instance().loadProject(project_file_path)) {
        LUNA_EDITOR_WARN("Failed to load project '{}'", project_file_path.string());
        return false;
    }

    resetEditorState();

    AssetManager::get().clear();
    AssetDatabase::clear();
    syncProjectAssets();
    AssetManager::get().init();

    createScene();

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (project_root && project_info && !project_info->StartScene.empty()) {
        const std::filesystem::path start_scene_path =
            SceneSerializer::normalizeScenePath((*project_root / project_info->StartScene).lexically_normal());
        if (std::filesystem::exists(start_scene_path)) {
            if (!openScene(start_scene_path, false)) {
                createScene();
                m_scene_file_path = start_scene_path;
                updateSceneLabel();
            }
        } else {
            m_scene_file_path = start_scene_path;
            updateSceneLabel();
            LUNA_EDITOR_WARN(
                "Configured StartScene '{}' does not exist. Saving will create it at that location.",
                start_scene_path.string());
        }
    } else {
        updateSceneLabel();
        LUNA_EDITOR_INFO("Project '{}' does not define a StartScene. Using an empty scene.",
                         project_file_path.string());
    }

    LUNA_EDITOR_INFO("Loaded project '{}' with {} scene entities",
                     project_file_path.string(),
                     m_scene.entityManager().entityCount());
    return true;
}

bool LunaEditorLayer::openScene()
{
    const std::filesystem::path scene_file_path =
        FileDialogs::openFile(kSceneFileFilter, sceneDialogDefaultPath().string());
    if (scene_file_path.empty()) {
        return false;
    }

    return openScene(scene_file_path, true);
}

bool LunaEditorLayer::openScene(const std::filesystem::path& scene_file_path, bool update_project_start_scene)
{
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        return false;
    }

    if (!SceneSerializer::deserialize(m_scene, normalized_scene_path)) {
        LUNA_EDITOR_WARN("Failed to open scene '{}'", normalized_scene_path.string());
        return false;
    }

    m_scene_file_path = normalized_scene_path;
    m_selected_entity = {};
    updateSceneLabel();

    if (update_project_start_scene) {
        syncProjectStartScene(normalized_scene_path);
    }

    LUNA_EDITOR_INFO(
        "Opened scene '{}' with {} entities", normalized_scene_path.string(), m_scene.entityManager().entityCount());
    return true;
}

bool LunaEditorLayer::saveScene()
{
    if (m_scene_file_path.empty()) {
        return saveSceneAs();
    }

    return saveSceneAs(m_scene_file_path);
}

bool LunaEditorLayer::saveSceneAs()
{
    const std::filesystem::path scene_file_path =
        FileDialogs::saveFile(kSceneFileFilter, sceneDialogDefaultPath().string());
    if (scene_file_path.empty()) {
        return false;
    }

    return saveSceneAs(scene_file_path);
}

bool LunaEditorLayer::saveSceneAs(const std::filesystem::path& scene_file_path)
{
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        return false;
    }

    if (m_scene.getName().empty() || m_scene.getName() == "Untitled") {
        m_scene.setName(normalized_scene_path.stem().string());
    }

    if (!SceneSerializer::serialize(m_scene, normalized_scene_path)) {
        LUNA_EDITOR_WARN("Failed to save scene '{}'", normalized_scene_path.string());
        return false;
    }

    m_scene_file_path = normalized_scene_path;
    updateSceneLabel();
    syncProjectStartScene(normalized_scene_path);

    LUNA_EDITOR_INFO("Saved scene '{}' to '{}'", m_scene.getName(), normalized_scene_path.string());
    return true;
}

std::filesystem::path LunaEditorLayer::sceneDialogDefaultPath() const
{
    if (!m_scene_file_path.empty()) {
        const std::filesystem::path parent_path = m_scene_file_path.parent_path();
        if (!parent_path.empty() && std::filesystem::exists(parent_path)) {
            return parent_path;
        }
    }

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (project_root && project_info) {
        const std::filesystem::path scenes_directory = (*project_root / project_info->AssetsPath / "Scenes").lexically_normal();
        if (std::filesystem::exists(scenes_directory)) {
            return scenes_directory;
        }

        const std::filesystem::path assets_directory = (*project_root / project_info->AssetsPath).lexically_normal();
        if (std::filesystem::exists(assets_directory)) {
            return assets_directory;
        }

        return *project_root;
    }

    return projectDialogDefaultPath();
}

void LunaEditorLayer::updateSceneLabel()
{
    if (!m_scene_file_path.empty()) {
        if (const auto relative_path = makeScenePathRelativeToProject(m_scene_file_path)) {
            m_asset_label = relative_path->generic_string();
            return;
        }

        m_asset_label = m_scene_file_path.lexically_normal().string();
        return;
    }

    const std::string scene_name = m_scene.getName().empty() ? "Untitled" : m_scene.getName();
    m_asset_label = scene_name + SceneSerializer::FileExtension + std::string(" (unsaved)");
}

void LunaEditorLayer::syncProjectStartScene(const std::filesystem::path& scene_file_path)
{
    const auto relative_scene_path = makeScenePathRelativeToProject(scene_file_path);
    if (!relative_scene_path) {
        LUNA_EDITOR_WARN("Scene '{}' is outside the current project root. StartScene was not updated.",
                         scene_file_path.string());
        return;
    }

    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_info) {
        return;
    }

    if (project_info->StartScene.lexically_normal() == relative_scene_path->lexically_normal()) {
        return;
    }

    ProjectInfo updated_project_info = *project_info;
    updated_project_info.StartScene = *relative_scene_path;
    ProjectManager::instance().setProjectInfo(updated_project_info);

    if (ProjectManager::instance().saveProject()) {
        LUNA_EDITOR_INFO("Updated project StartScene to '{}'", relative_scene_path->generic_string());
    } else {
        LUNA_EDITOR_WARN("Failed to persist updated StartScene '{}'", relative_scene_path->generic_string());
    }
}

bool LunaEditorLayer::hasProjectLoaded() const
{
    return ProjectManager::instance().getProjectRootPath().has_value() &&
           ProjectManager::instance().getProjectInfo().has_value();
}

} // namespace luna
