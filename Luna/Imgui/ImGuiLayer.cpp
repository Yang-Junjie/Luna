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

void ImGuiLayer::startFrame()
{
    if (!m_attached) {
        return;
    }

    luna::ImGuiRhiContext::StartFrame();
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

} // namespace luna
