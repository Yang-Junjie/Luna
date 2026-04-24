#include "Core/Log.h"
#include "Events/Event.h"
#include "Imgui/ImGuiContext.h"
#include "Imgui/ImGuiLayer.hpp"

#include <imgui.h>

namespace luna {

ImGuiLayer::ImGuiLayer(Renderer& renderer, bool enable_multi_viewport)
    : Layer("ImGuiLayer"),
      m_enable_multi_viewport(enable_multi_viewport),
      m_renderer(&renderer)
{}

void ImGuiLayer::onAttach()
{
    if (m_attached) {
        return;
    }

    if (m_renderer == nullptr || !m_renderer->isInitialized() || m_renderer->getNativeWindow() == nullptr) {
        LUNA_IMGUI_ERROR("Cannot initialize ImGui layer because renderer state is incomplete");
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (m_enable_multi_viewport) {
        LUNA_IMGUI_WARN("ImGui multi-viewport is disabled until the RHI path supports rendering platform windows");
    }
    io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    setDarkThemeColors();
    setImGuiWidgetStyle();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    if (!luna::ImGuiRhiContext::Init(*m_renderer)) {
        LUNA_IMGUI_ERROR("Failed to initialize ImGui RHI backend");
        ImGui::DestroyContext();
        return;
    }
    m_attached = true;
    LUNA_IMGUI_INFO("Initialized ImGui for luna");
}

void ImGuiLayer::onDetach()
{
    if (!m_attached) {
        return;
    }

    luna::ImGuiRhiContext::Destroy();
    m_attached = false;
}

void ImGuiLayer::onEvent(Event& event)
{
    if (!m_attached || !m_block_events) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    event.m_handled |= event.isInCategory(EventCategory::EventCategoryMouse) && io.WantCaptureMouse;
    event.m_handled |= event.isInCategory(EventCategory::EventCategoryKeyboard) && io.WantCaptureKeyboard;
}

void ImGuiLayer::begin()
{
    if (!m_attached) {
        return;
    }

    luna::ImGuiRhiContext::StartFrame();

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    }
    if (m_menu_bar_callback && ImGui::BeginMainMenuBar()) {
        m_menu_bar_callback();
        ImGui::EndMainMenuBar();
    }
}

void ImGuiLayer::end()
{
    if (!m_attached) {
        return;
    }
}

void ImGuiLayer::renderPlatformWindows()
{
    if (!m_attached || !viewportsEnabled()) {
        return;
    }

    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

bool ImGuiLayer::viewportsEnabled() const
{
    if (!m_attached) {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    return (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

void ImGuiLayer::setImGuiWidgetStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScrollbarRounding = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
}

void ImGuiLayer::setDarkThemeColors()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4{0.11f, 0.11f, 0.11f, 1.00f};
    colors[ImGuiCol_ChildBg] = ImVec4{0.11f, 0.11f, 0.11f, 1.00f};
    colors[ImGuiCol_PopupBg] = ImVec4{0.08f, 0.08f, 0.08f, 0.96f};
    colors[ImGuiCol_Border] = ImVec4{0.17f, 0.17f, 0.18f, 1.00f};

    colors[ImGuiCol_TitleBg] = ImVec4{0.07f, 0.07f, 0.07f, 1.00f};
    colors[ImGuiCol_TitleBgActive] = ImVec4{0.09f, 0.09f, 0.09f, 1.00f};
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.07f, 0.07f, 0.07f, 1.00f};

    colors[ImGuiCol_FrameBg] = ImVec4{0.16f, 0.16f, 0.17f, 1.00f};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.22f, 0.22f, 0.23f, 1.00f};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.13f, 0.13f, 0.14f, 1.00f};

    const ImVec4 orange_main = ImVec4{0.92f, 0.45f, 0.11f, 1.00f};
    const ImVec4 orange_hovered = ImVec4{1.00f, 0.55f, 0.20f, 1.00f};
    const ImVec4 orange_active = ImVec4{0.80f, 0.38f, 0.08f, 1.00f};

    colors[ImGuiCol_Button] = ImVec4{0.20f, 0.20f, 0.21f, 1.00f};
    colors[ImGuiCol_ButtonHovered] = orange_main;
    colors[ImGuiCol_ButtonActive] = orange_active;

    colors[ImGuiCol_Tab] = ImVec4{0.12f, 0.12f, 0.13f, 1.00f};
    colors[ImGuiCol_TabHovered] = orange_hovered;
    colors[ImGuiCol_TabActive] = orange_main;
    colors[ImGuiCol_TabUnfocused] = ImVec4{0.12f, 0.12f, 0.13f, 1.00f};
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.18f, 0.18f, 0.19f, 1.00f};

    colors[ImGuiCol_CheckMark] = orange_main;
    colors[ImGuiCol_SliderGrab] = orange_main;
    colors[ImGuiCol_SliderGrabActive] = orange_active;
    colors[ImGuiCol_Header] = ImVec4{0.35f, 0.20f, 0.08f, 0.50f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.92f, 0.45f, 0.11f, 0.30f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.92f, 0.45f, 0.11f, 0.50f};
    colors[ImGuiCol_TextSelectedBg] = ImVec4{0.92f, 0.45f, 0.11f, 0.35f};
    colors[ImGuiCol_SeparatorHovered] = orange_main;
    colors[ImGuiCol_SeparatorActive] = orange_active;
    colors[ImGuiCol_ResizeGrip] = ImVec4{0.92f, 0.45f, 0.11f, 0.20f};
    colors[ImGuiCol_ResizeGripHovered] = orange_main;
    colors[ImGuiCol_ResizeGripActive] = orange_active;

    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.TabRounding = 2.0f;
}

} // namespace luna
