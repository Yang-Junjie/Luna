#include "Core/Log.h"
#include "Imgui/ImGuiContext.h"
#include "LunaEditorApp.h"
#include "LunaEditorLayer.h"
#include "Platform/Common/FileDialogs.h"
#include "Project/ProjectManager.h"

#include <algorithm>
#include <filesystem>
#include <imgui.h>
#include <Project/ProjectInfo.h>

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

} // namespace

namespace luna {

LunaEditorLayer::LunaEditorLayer(LunaEditorApplication& application)
    : Layer("LunaEditorLayer"),
      m_application(&application),
      m_scene_hierarchy_panel(application),
      m_inspector_panel(application)
{}

void LunaEditorLayer::onAttach()
{
    if (m_application == nullptr) {
        return;
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
    ImGui::Text("Scene Source: %s", application.getAssetLabel().c_str());
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
        if (ImGui::MenuItem("Create New Project")) {
            std::filesystem::path project_root_path = FileDialogs::selectDirectory(".");
            LUNA_EDITOR_DEBUG("project root path {0}", project_root_path.string());
            ProjectInfo project_info{.Name = "Sample Project",
                                     .Version = "0.1.0",
                                     .Author = "Junjie Yang",
                                     .Description = "A simple Luna project.",
                                     .StartScene = "./Assets/Scenes/SampleScene.lunascene",
                                     .AssetsPath = "./Assets/"};
            ProjectManager::instance().createProject(project_root_path, project_info);
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

} // namespace luna
