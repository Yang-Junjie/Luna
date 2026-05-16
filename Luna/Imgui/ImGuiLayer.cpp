#include "Core/Log.h"
#include "Events/ApplicationEvent.h"
#include "Events/Event.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Imgui/ImGuiContext.h"
#include "Imgui/ImGuiLayer.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

#include <imgui.h>

namespace luna {

namespace {

ImGuiMouseButton toImGuiMouseButton(MouseCode button)
{
    switch (button) {
        case MouseCode::Left:
            return ImGuiMouseButton_Left;
        case MouseCode::Right:
            return ImGuiMouseButton_Right;
        case MouseCode::Middle:
            return ImGuiMouseButton_Middle;
        case MouseCode::XButton1:
            return 3;
        case MouseCode::XButton2:
            return 4;
        case MouseCode::None:
            break;
    }

    return ImGuiMouseButton_COUNT;
}

ImGuiKey toImGuiKey(KeyCode key)
{
    switch (key) {
        case KeyCode::A:
            return ImGuiKey_A;
        case KeyCode::B:
            return ImGuiKey_B;
        case KeyCode::C:
            return ImGuiKey_C;
        case KeyCode::D:
            return ImGuiKey_D;
        case KeyCode::E:
            return ImGuiKey_E;
        case KeyCode::F:
            return ImGuiKey_F;
        case KeyCode::G:
            return ImGuiKey_G;
        case KeyCode::H:
            return ImGuiKey_H;
        case KeyCode::I:
            return ImGuiKey_I;
        case KeyCode::J:
            return ImGuiKey_J;
        case KeyCode::K:
            return ImGuiKey_K;
        case KeyCode::L:
            return ImGuiKey_L;
        case KeyCode::M:
            return ImGuiKey_M;
        case KeyCode::N:
            return ImGuiKey_N;
        case KeyCode::O:
            return ImGuiKey_O;
        case KeyCode::P:
            return ImGuiKey_P;
        case KeyCode::Q:
            return ImGuiKey_Q;
        case KeyCode::R:
            return ImGuiKey_R;
        case KeyCode::S:
            return ImGuiKey_S;
        case KeyCode::T:
            return ImGuiKey_T;
        case KeyCode::U:
            return ImGuiKey_U;
        case KeyCode::V:
            return ImGuiKey_V;
        case KeyCode::W:
            return ImGuiKey_W;
        case KeyCode::X:
            return ImGuiKey_X;
        case KeyCode::Y:
            return ImGuiKey_Y;
        case KeyCode::Z:
            return ImGuiKey_Z;
        case KeyCode::Space:
            return ImGuiKey_Space;
        case KeyCode::Enter:
            return ImGuiKey_Enter;
        case KeyCode::Delete:
            return ImGuiKey_Delete;
        case KeyCode::Escape:
            return ImGuiKey_Escape;
        case KeyCode::Up:
            return ImGuiKey_UpArrow;
        case KeyCode::Down:
            return ImGuiKey_DownArrow;
        case KeyCode::Left:
            return ImGuiKey_LeftArrow;
        case KeyCode::Right:
            return ImGuiKey_RightArrow;
        case KeyCode::LeftShift:
            return ImGuiKey_LeftShift;
        case KeyCode::RightShift:
            return ImGuiKey_RightShift;
        case KeyCode::LeftControl:
            return ImGuiKey_LeftCtrl;
        case KeyCode::RightControl:
            return ImGuiKey_RightCtrl;
        case KeyCode::LeftAlt:
            return ImGuiKey_LeftAlt;
        case KeyCode::RightAlt:
            return ImGuiKey_RightAlt;
        case KeyCode::None:
            break;
    }

    return ImGuiKey_None;
}

struct ModifierKeyState {
    bool left_ctrl{false};
    bool right_ctrl{false};
    bool left_shift{false};
    bool right_shift{false};
    bool left_alt{false};
    bool right_alt{false};
};

ModifierKeyState g_modifier_key_state{};

float sanitizeFontSize(float size_pixels) noexcept
{
    if (!std::isfinite(size_pixels) || size_pixels <= 0.0f) {
        return 16.0f;
    }
    return std::clamp(size_pixels, 8.0f, 48.0f);
}

bool loadConfiguredFont(ImGuiIO& io, const ImGuiFontConfig& config)
{
    ImFont* font = nullptr;

    if (config.font_path.empty()) {
        font = io.Fonts->AddFontDefault();
        LUNA_IMGUI_INFO("Loaded default ImGui font");
    } else {
        std::error_code exists_ec;
        if (std::filesystem::exists(config.font_path, exists_ec) && !exists_ec) {
            const float size_pixels = sanitizeFontSize(config.size_pixels);
            font = io.Fonts->AddFontFromFileTTF(config.font_path.string().c_str(), size_pixels);
            if (font != nullptr) {
                LUNA_IMGUI_INFO("Loaded ImGui font '{}' at {}px", config.font_path.string(), size_pixels);
            } else {
                LUNA_IMGUI_WARN("Failed to load configured ImGui font '{}' at {}px; using default font",
                                config.font_path.string(),
                                size_pixels);
            }
        }
    }

    const bool loaded_configured_font = font != nullptr;
    if (font == nullptr) {
        LUNA_IMGUI_WARN("Configured ImGui font '{}' is missing; using default font", config.font_path.string());
        font = io.Fonts->AddFontDefault();
    }

    io.FontDefault = font;

    return loaded_configured_font;
}

void updateModifierKeys(ImGuiIO& io, KeyCode key, bool down)
{
    switch (key) {
        case KeyCode::LeftControl:
            g_modifier_key_state.left_ctrl = down;
            io.AddKeyEvent(ImGuiMod_Ctrl, g_modifier_key_state.left_ctrl || g_modifier_key_state.right_ctrl);
            break;
        case KeyCode::RightControl:
            g_modifier_key_state.right_ctrl = down;
            io.AddKeyEvent(ImGuiMod_Ctrl, g_modifier_key_state.left_ctrl || g_modifier_key_state.right_ctrl);
            break;
        case KeyCode::LeftShift:
            g_modifier_key_state.left_shift = down;
            io.AddKeyEvent(ImGuiMod_Shift, g_modifier_key_state.left_shift || g_modifier_key_state.right_shift);
            break;
        case KeyCode::RightShift:
            g_modifier_key_state.right_shift = down;
            io.AddKeyEvent(ImGuiMod_Shift, g_modifier_key_state.left_shift || g_modifier_key_state.right_shift);
            break;
        case KeyCode::LeftAlt:
            g_modifier_key_state.left_alt = down;
            io.AddKeyEvent(ImGuiMod_Alt, g_modifier_key_state.left_alt || g_modifier_key_state.right_alt);
            break;
        case KeyCode::RightAlt:
            g_modifier_key_state.right_alt = down;
            io.AddKeyEvent(ImGuiMod_Alt, g_modifier_key_state.left_alt || g_modifier_key_state.right_alt);
            break;
        default:
            break;
    }
}

} // namespace

ImGuiLayer::ImGuiLayer(Renderer& renderer, bool enable_multi_viewport, ImGuiFontConfig font_config)
    : Layer("ImGuiLayer"),
      m_enable_multi_viewport(enable_multi_viewport),
      m_font_config(std::move(font_config)),
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
    loadConfiguredFont(io, m_font_config);

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
    g_modifier_key_state = {};
    m_attached = false;
}

void ImGuiLayer::onEvent(Event& event)
{
    if (!m_attached) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<WindowFocusEvent>([&io](WindowFocusEvent&) {
        io.AddFocusEvent(true);
        return false;
    });
    dispatcher.dispatch<WindowLostFocusEvent>([&io](WindowLostFocusEvent&) {
        io.AddFocusEvent(false);
        g_modifier_key_state = {};
        return false;
    });
    dispatcher.dispatch<MouseMovedEvent>([&io](MouseMovedEvent& mouse_event) {
        io.AddMousePosEvent(mouse_event.getX(), mouse_event.getY());
        return false;
    });
    dispatcher.dispatch<MouseScrolledEvent>([&io](MouseScrolledEvent& mouse_event) {
        io.AddMouseWheelEvent(mouse_event.getXOffset(), mouse_event.getYOffset());
        return false;
    });
    dispatcher.dispatch<MouseButtonPressedEvent>([&io](MouseButtonPressedEvent& mouse_event) {
        const ImGuiMouseButton button = toImGuiMouseButton(mouse_event.getMouseButton());
        if (button < ImGuiMouseButton_COUNT) {
            io.AddMouseButtonEvent(button, true);
        }
        return false;
    });
    dispatcher.dispatch<MouseButtonReleasedEvent>([&io](MouseButtonReleasedEvent& mouse_event) {
        const ImGuiMouseButton button = toImGuiMouseButton(mouse_event.getMouseButton());
        if (button < ImGuiMouseButton_COUNT) {
            io.AddMouseButtonEvent(button, false);
        }
        return false;
    });
    dispatcher.dispatch<KeyPressedEvent>([&io](KeyPressedEvent& key_event) {
        const KeyCode key = key_event.getKeyCode();
        updateModifierKeys(io, key, true);
        const ImGuiKey imgui_key = toImGuiKey(key);
        if (imgui_key != ImGuiKey_None) {
            io.AddKeyEvent(imgui_key, true);
        }
        return false;
    });
    dispatcher.dispatch<KeyReleasedEvent>([&io](KeyReleasedEvent& key_event) {
        const KeyCode key = key_event.getKeyCode();
        updateModifierKeys(io, key, false);
        const ImGuiKey imgui_key = toImGuiKey(key);
        if (imgui_key != ImGuiKey_None) {
            io.AddKeyEvent(imgui_key, false);
        }
        return false;
    });
    dispatcher.dispatch<KeyTypedEvent>([&io](KeyTypedEvent& key_event) {
        if (key_event.getCodepoint() != 0u) {
            io.AddInputCharacter(key_event.getCodepoint());
        }
        return false;
    });

    if (!m_block_events) {
        return;
    }

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
