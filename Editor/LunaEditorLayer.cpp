#include "Core/Log.h"
#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/Editor/ImporterManager.h"
#include "Asset/Editor/MeshLoader.h"
#include "Imgui/ImGuiContext.h"
#include "LunaEditorApp.h"
#include "LunaEditorLayer.h"
#include "Platform/Common/FileDialogs.h"
#include "Project/ProjectManager.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Scene/Components.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <glm/common.hpp>
#include <imgui.h>
#include <limits>
#include <optional>
#include <Project/ProjectInfo.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

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

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::filesystem::path projectDialogDefaultPath()
{
    if (const auto project_root = luna::ProjectManager::instance().getProjectRootPath()) {
        return *project_root;
    }
    return std::filesystem::current_path();
}

std::optional<std::filesystem::path> getAssetsRootPath()
{
    const auto project_root = luna::ProjectManager::instance().getProjectRootPath();
    const auto project_info = luna::ProjectManager::instance().getProjectInfo();
    if (!project_root || !project_info) {
        return std::nullopt;
    }

    return (*project_root / project_info->AssetsPath).lexically_normal();
}

luna::AssetHandle registerMemoryAsset(const std::shared_ptr<luna::Asset>& asset)
{
    if (!asset) {
        return luna::AssetHandle(0);
    }

    luna::AssetManager::get().registerMemoryAsset(asset->handle, asset);
    return asset->handle;
}

std::shared_ptr<luna::Mesh> createNormalizedMesh(const luna::Mesh& mesh)
{
    if (!mesh.isValid()) {
        return {};
    }

    std::vector<luna::SubMesh> sub_meshes = mesh.getSubMeshes();

    glm::vec3 bounds_min(std::numeric_limits<float>::max());
    glm::vec3 bounds_max(std::numeric_limits<float>::lowest());
    bool has_vertex = false;

    for (const auto& sub_mesh : sub_meshes) {
        for (const auto& vertex : sub_mesh.Vertices) {
            bounds_min = glm::min(bounds_min, vertex.Position);
            bounds_max = glm::max(bounds_max, vertex.Position);
            has_vertex = true;
        }
    }

    if (!has_vertex) {
        return {};
    }

    const glm::vec3 center = (bounds_min + bounds_max) * 0.5f;
    const glm::vec3 extent = bounds_max - bounds_min;
    const float max_extent = (std::max) ((std::max) (extent.x, extent.y), (std::max) (extent.z, 0.0001f));
    const float scale = 2.0f / max_extent;

    for (auto& sub_mesh : sub_meshes) {
        for (auto& vertex : sub_mesh.Vertices) {
            vertex.Position = (vertex.Position - center) * scale;
        }
    }

    return luna::Mesh::create(mesh.getName().empty() ? "DemoAssetMesh" : mesh.getName(), std::move(sub_meshes));
}

std::optional<std::filesystem::path> findPreferredMeshAssetPath()
{
    const auto assets_root = getAssetsRootPath();
    if (!assets_root || !std::filesystem::exists(*assets_root)) {
        return std::nullopt;
    }

    std::optional<std::filesystem::path> best_match;
    int best_score = std::numeric_limits<int>::max();

    for (const auto& entry : std::filesystem::recursive_directory_iterator(*assets_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto path = entry.path();
        const std::string extension = toLower(path.extension().string());
        int score = std::numeric_limits<int>::max();

        if (extension == ".gltf") {
            score = 20;
        } else if (extension == ".glb") {
            score = 30;
        } else if (extension == ".fbx") {
            score = 40;
        } else if (extension == ".obj") {
            score = 50;
        } else {
            continue;
        }

        const std::string stem = toLower(path.stem().string());
        if (stem == "damagedhelmet") {
            score = 0;
        } else if (stem.find("helmet") != std::string::npos) {
            score -= 5;
        }

        if (score < best_score) {
            best_score = score;
            best_match = path;
        }
    }

    return best_match;
}

luna::AssetHandle findCompanionMaterialHandle(const std::filesystem::path& mesh_path)
{
    const std::array<std::filesystem::path, 4> candidates = {
        mesh_path.parent_path() / (mesh_path.stem().string() + ".lunamat"),
        mesh_path.parent_path() / (mesh_path.stem().string() + ".material"),
        mesh_path.parent_path() / (mesh_path.stem().string() + ".lmat"),
        mesh_path.parent_path() / (mesh_path.stem().string() + ".mtl"),
    };

    for (const auto& candidate : candidates) {
        if (!std::filesystem::exists(candidate)) {
            continue;
        }

        const luna::AssetHandle handle = luna::AssetDatabase::findHandleByFilePath(candidate);
        if (handle.isValid()) {
            return handle;
        }
    }

    return luna::AssetHandle(0);
}

std::shared_ptr<luna::Material> createFallbackMaterial()
{
    luna::Material::SurfaceProperties surface;
    surface.BaseColorFactor = glm::vec4(0.96f, 0.52f, 0.18f, 1.0f);
    return luna::Material::create("FallbackMaterial", {}, surface);
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
    ImGui::Text("Scene Source: %s", m_asset_label.c_str());
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
    if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem("Open Project")) {
            const std::filesystem::path project_file_path =
                FileDialogs::openFile("Luna Project (*.lunaproj)\0*.lunaproj\0", projectDialogDefaultPath().string());
            if (!project_file_path.empty()) {
                openProject(project_file_path);
            }
        }

        if (ImGui::MenuItem("Create New Project")) {
            const std::filesystem::path project_root_path =
                FileDialogs::selectDirectory(projectDialogDefaultPath().string());
            LUNA_EDITOR_DEBUG("project root path {0}", project_root_path.string());
            ProjectInfo project_info{.Name = "Sample Project",
                                     .Version = "0.1.0",
                                     .Author = "Junjie Yang",
                                     .Description = "A simple Luna project.",
                                     .StartScene = "./Assets/Scenes/SampleScene.lunascene",
                                     .AssetsPath = "./Assets/"};
            if (!project_root_path.empty()) {
                ProjectManager::instance().createProject(project_root_path, project_info);
            }
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
    m_scene.registry().clear();
    m_selected_entity = {};
    m_demo_mesh.reset();
    m_demo_material.reset();
    m_demo_mesh_handle = AssetHandle(0);
    m_demo_material_handle = AssetHandle(0);
    m_asset_label = "No asset loaded";
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
    ImporterManager::init();
    ImporterManager::import();
    AssetManager::get().init();

    buildScene();
    LUNA_EDITOR_INFO("Loaded project '{}'", project_file_path.string());
    return true;
}

void LunaEditorLayer::buildScene()
{
    if (!tryLoadDefaultAsset()) {
        m_selected_entity = {};
        m_asset_label = "No asset loaded";
        LUNA_EDITOR_WARN("No preview asset could be loaded for current project");
        return;
    }

    Entity demo_entity = m_scene.createEntity("Demo Mesh");
    auto& mesh_component = demo_entity.addComponent<MeshComponent>();
    if (!m_demo_mesh_handle.isValid()) {
        m_demo_mesh_handle = registerMemoryAsset(m_demo_mesh);
    }
    mesh_component.meshHandle = m_demo_mesh_handle;

    if (m_demo_mesh) {
        mesh_component.resizeSubmeshMaterials(m_demo_mesh->getSubMeshes().size());
    }

    if (!m_demo_material_handle.isValid()) {
        m_demo_material_handle = registerMemoryAsset(m_demo_material);
    }

    const AssetHandle material_handle = m_demo_material_handle;
    for (uint32_t submesh_index = 0; submesh_index < mesh_component.getSubmeshMaterialCount(); ++submesh_index) {
        mesh_component.setSubmeshMaterial(submesh_index, material_handle);
    }

    auto& transform = demo_entity.getComponent<TransformComponent>();
    transform.translation = glm::vec3(0.0f);
    transform.rotation = glm::vec3(-0.35f, 0.0f, 0.0f);
    transform.scale = glm::vec3(1.0f);

    m_selected_entity = demo_entity;
}

bool LunaEditorLayer::tryLoadDefaultAsset()
{
    const auto mesh_path = findPreferredMeshAssetPath();
    if (!mesh_path) {
        return false;
    }

    try {
        const auto loaded_mesh = MeshLoader::loadFromFile(*mesh_path);
        if (!loaded_mesh || !loaded_mesh->isValid()) {
            return false;
        }

        m_demo_mesh = createNormalizedMesh(*loaded_mesh);
        if (!m_demo_mesh || !m_demo_mesh->isValid()) {
            return false;
        }

        m_demo_mesh_handle = AssetHandle(0);
        m_demo_material_handle = findCompanionMaterialHandle(*mesh_path);
        if (m_demo_material_handle.isValid()) {
            m_demo_material.reset();
        } else {
            m_demo_material = createFallbackMaterial();
        }

        m_asset_label = mesh_path->filename().string();
        LUNA_EDITOR_INFO("Loaded project preview asset '{}'", mesh_path->string());
        return true;
    } catch (const std::exception& error) {
        LUNA_EDITOR_WARN("Failed to load project preview asset '{}': {}", mesh_path->string(), error.what());
        return false;
    }
}

} // namespace luna
