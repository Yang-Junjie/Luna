#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"
#include "SwapchainLabPipeline.h"
#include "SwapchainLabState.h"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string_view>

namespace {

enum class SelfTestMode : uint8_t {
    None = 0,
    Recreate,
    TwoWindows
};

struct CommandLineOptions {
    luna::SwapchainDesc swapchainDesc{};
    bool twoWindows = false;
    SelfTestMode selfTestMode = SelfTestMode::None;
};

struct SelfTestResult {
    bool passed = false;
};

bool parse_uint32(std::string_view text, uint32_t* outValue)
{
    if (outValue == nullptr || text.empty()) {
        return false;
    }

    uint32_t value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end) {
        return false;
    }

    *outValue = value;
    return true;
}

bool parse_vsync(std::string_view text, bool* outValue)
{
    if (outValue == nullptr) {
        return false;
    }

    if (luna::iequals_ascii(text, "on") || luna::iequals_ascii(text, "true") || text == "1") {
        *outValue = true;
        return true;
    }

    if (luna::iequals_ascii(text, "off") || luna::iequals_ascii(text, "false") || text == "0") {
        *outValue = false;
        return true;
    }

    return false;
}

bool parse_format(std::string_view text, luna::PixelFormat* outFormat)
{
    if (outFormat == nullptr) {
        return false;
    }

    if (luna::iequals_ascii(text, "bgra8unorm")) {
        *outFormat = luna::PixelFormat::BGRA8Unorm;
        return true;
    }

    if (luna::iequals_ascii(text, "rgba8unorm")) {
        *outFormat = luna::PixelFormat::RGBA8Unorm;
        return true;
    }

    if (luna::iequals_ascii(text, "rgba8srgb")) {
        *outFormat = luna::PixelFormat::RGBA8Srgb;
        return true;
    }

    return false;
}

bool parse_arguments(int argc, char** argv, CommandLineOptions* options)
{
    if (options == nullptr) {
        return false;
    }

    options->swapchainDesc.bufferCount = 2;
    options->swapchainDesc.format = luna::PixelFormat::BGRA8Unorm;
    options->swapchainDesc.vsync = true;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--buffer-count") {
            if (index + 1 >= argc || !parse_uint32(argv[index + 1], &options->swapchainDesc.bufferCount)) {
                LUNA_CORE_ERROR("Invalid --buffer-count value");
                return false;
            }
            ++index;
            continue;
        }

        if (argument == "--vsync") {
            if (index + 1 >= argc || !parse_vsync(argv[index + 1], &options->swapchainDesc.vsync)) {
                LUNA_CORE_ERROR("Invalid --vsync value");
                return false;
            }
            ++index;
            continue;
        }

        if (argument == "--format") {
            if (index + 1 >= argc || !parse_format(argv[index + 1], &options->swapchainDesc.format)) {
                LUNA_CORE_ERROR("Invalid --format value");
                return false;
            }
            ++index;
            continue;
        }

        if (argument == "--two-windows") {
            options->twoWindows = true;
            continue;
        }

        constexpr std::string_view selfTestPrefix = "--self-test=";
        if (argument.substr(0, selfTestPrefix.size()) == selfTestPrefix) {
            const std::string_view mode = argument.substr(selfTestPrefix.size());
            if (mode == "recreate") {
                options->selfTestMode = SelfTestMode::Recreate;
                continue;
            }

            if (mode == "two_windows") {
                options->selfTestMode = SelfTestMode::TwoWindows;
                options->twoWindows = true;
                continue;
            }

            LUNA_CORE_ERROR("Unknown --self-test mode '{}'", mode);
            return false;
        }

        LUNA_CORE_ERROR("Unknown argument '{}'", argument);
        return false;
    }

    options->swapchainDesc.bufferCount = std::max(1u, options->swapchainDesc.bufferCount);
    return true;
}

class RhiSwapchainLabLayer final : public luna::Layer {
public:
    explicit RhiSwapchainLabLayer(std::shared_ptr<swapchain_lab::State> state)
        : luna::Layer("RhiSwapchainLabLayer"),
          m_state(std::move(state))
    {}

    void onImGuiRender() override
    {
        if (m_state == nullptr) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(480.0f, 320.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RhiSwapchainLab")) {
            ImGui::End();
            return;
        }

        ImGui::Text("Requested: vsync=%s", m_state->requestedDesc.vsync ? "true" : "false");
        ImGui::Text("Requested: bufferCount=%u", m_state->requestedDesc.bufferCount);
        ImGui::Text("Requested: format=%s", luna::to_string(m_state->requestedDesc.format).data());
        ImGui::Separator();
        ImGui::Text("Observed: Device Id=%llu", static_cast<unsigned long long>(m_state->observedState.deviceId));
        ImGui::Text("Observed: Swapchain Id=%llu", static_cast<unsigned long long>(m_state->observedState.swapchainId));
        ImGui::Text("Observed: valid=%s", m_state->observedState.valid ? "true" : "false");
        ImGui::Text("Observed: imageCount=%u", m_state->observedState.imageCount);
        ImGui::Text("Observed: format=%s", luna::to_string(m_state->observedState.currentFormat).data());
        ImGui::Text("Observed: presentMode=%s", m_state->observedState.presentModeName.data());
        ImGui::Text("Observed: vsyncActive=%s", m_state->observedState.vsyncActive ? "true" : "false");
        ImGui::Text("Observed: extent=%ux%u", m_state->observedState.width, m_state->observedState.height);
        ImGui::Text("Observed: frameCounter=%llu", static_cast<unsigned long long>(m_state->frameCounter));
        if (ImGui::Button("Recreate Swapchain")) {
            m_state->recreateSwapchainRequested = true;
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", m_state->status.empty() ? "Waiting for swapchain state..." : m_state->status.c_str());

        ImGui::End();

        if (!m_state->twoWindows) {
            return;
        }

        ImGuiWindowClass secondaryWindowClass;
        secondaryWindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        secondaryWindowClass.DockingAllowUnclassed = false;
        ImGui::SetNextWindowClass(&secondaryWindowClass);
        if (const ImGuiViewport* mainViewport = ImGui::GetMainViewport(); mainViewport != nullptr) {
            ImGui::SetNextWindowPos(
                ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 32.0f, mainViewport->Pos.y + 40.0f),
                ImGuiCond_FirstUseEver);
        }
        ImGui::SetNextWindowSize(ImVec2(360.0f, 220.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RhiSwapchainLab Secondary", nullptr, ImGuiWindowFlags_NoDocking)) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Secondary platform window");
        ImGui::Text("Device Id=%llu", static_cast<unsigned long long>(m_state->observedState.deviceId));
        ImGui::Text("Swapchain Id=%llu", static_cast<unsigned long long>(m_state->observedState.swapchainId));
        ImGui::Text("Frames=%llu", static_cast<unsigned long long>(m_state->frameCounter));
        ImGui::Separator();
        ImGui::TextWrapped("Resize either window and verify the other keeps refreshing.");
        ImGui::TextWrapped("%s", m_state->status.c_str());
        ImGui::End();
    }

private:
    std::shared_ptr<swapchain_lab::State> m_state;
};

class RhiSwapchainLabSelfTestLayer final : public luna::Layer {
public:
    RhiSwapchainLabSelfTestLayer(std::shared_ptr<swapchain_lab::State> state,
                                 std::shared_ptr<SelfTestResult> result,
                                 SelfTestMode selfTestMode)
        : luna::Layer("RhiSwapchainLabSelfTestLayer"),
          m_state(std::move(state)),
          m_result(std::move(result)),
          m_selfTestMode(selfTestMode)
    {}

    void onAttach() override
    {
        LUNA_CORE_INFO("RhiSwapchainLab self-test begin: {}",
                       m_selfTestMode == SelfTestMode::Recreate ? "recreate" : "two_windows");
    }

    void onUpdate(luna::Timestep) override
    {
        if (m_state == nullptr || m_result == nullptr) {
            return;
        }

        ++m_tick;
        switch (m_selfTestMode) {
            case SelfTestMode::Recreate:
                update_recreate_test();
                break;
            case SelfTestMode::TwoWindows:
                update_two_window_test();
                break;
            case SelfTestMode::None:
            default:
                break;
        }
    }

    void onImGuiRender() override
    {
        if (m_selfTestMode == SelfTestMode::TwoWindows) {
            m_lastViewportCount = ImGui::GetPlatformIO().Viewports.Size;
        }
    }

private:
    void update_recreate_test()
    {
        if (!m_capturedInitialIds) {
            if (m_state->observedState.deviceId == 0 || m_state->observedState.swapchainId == 0) {
                fail_if_timeout("initial ids were not observed");
                return;
            }

            m_initialDeviceId = m_state->observedState.deviceId;
            m_initialSwapchainId = m_state->observedState.swapchainId;
            m_capturedInitialIds = true;
            return;
        }

        if (!m_recreateRequested && m_state->frameCounter >= 8) {
            m_state->recreateSwapchainRequested = true;
            m_recreateRequested = true;
            return;
        }

        if (m_recreateRequested && m_state->observedState.swapchainId != 0 &&
            m_state->observedState.swapchainId != m_initialSwapchainId) {
            const bool passed = m_state->observedState.deviceId == m_initialDeviceId;
            finish(passed,
                   passed ? "RhiSwapchainLab recreate PASS deviceId stable and swapchainId changed"
                          : "RhiSwapchainLab recreate FAIL deviceId changed unexpectedly");
            return;
        }

        fail_if_timeout("swapchainId did not change after recreate request");
    }

    void update_two_window_test()
    {
        if (m_lastViewportCount >= 2) {
            m_observedTwoViewports = true;
        }

        if (m_observedTwoViewports && !m_resizeIssued && m_state->frameCounter >= 20) {
            auto* nativeWindow = static_cast<GLFWwindow*>(luna::Application::get().getWindow().getNativeWindow());
            if (nativeWindow != nullptr) {
                glfwSetWindowSize(nativeWindow, 1024, 640);
                m_resizeIssued = true;
                m_resizeFrameCounter = m_state->frameCounter;
            }
        }

        if (m_resizeIssued && m_lastViewportCount >= 2 && m_state->frameCounter >= m_resizeFrameCounter + 20 &&
            m_state->observedState.valid) {
            finish(true, "RhiSwapchainLab two_windows PASS second viewport stayed alive after main window resize");
            return;
        }

        fail_if_timeout("second viewport did not stay alive after resize");
    }

    void fail_if_timeout(const char* reason)
    {
        if (m_tick < 600) {
            return;
        }

        finish(false, reason);
    }

    void finish(bool passed, const char* message)
    {
        m_result->passed = passed;
        if (passed) {
            LUNA_CORE_INFO("{}", message);
        } else {
            LUNA_CORE_ERROR("{}", message);
        }
        luna::Application::get().close();
    }

private:
    std::shared_ptr<swapchain_lab::State> m_state;
    std::shared_ptr<SelfTestResult> m_result;
    SelfTestMode m_selfTestMode = SelfTestMode::None;
    uint64_t m_initialDeviceId = 0;
    uint64_t m_initialSwapchainId = 0;
    uint64_t m_resizeFrameCounter = 0;
    int m_tick = 0;
    int m_lastViewportCount = 0;
    bool m_capturedInitialIds = false;
    bool m_recreateRequested = false;
    bool m_observedTwoViewports = false;
    bool m_resizeIssued = false;
};

} // namespace

int main(int argc, char** argv)
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    CommandLineOptions options;
    if (!parse_arguments(argc, argv, &options)) {
        luna::Logger::shutdown();
        return 1;
    }

    std::shared_ptr<swapchain_lab::State> state = std::make_shared<swapchain_lab::State>();
    std::shared_ptr<SelfTestResult> selfTestResult = std::make_shared<SelfTestResult>();
    state->requestedDesc = options.swapchainDesc;
    state->twoWindows = options.twoWindows;
    std::shared_ptr<luna::IRenderPipeline> renderPipeline =
        std::make_shared<swapchain_lab::RhiSwapchainLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiSwapchainLab",
        .windowWidth = 1280,
        .windowHeight = 720,
        .maximized = false,
        .enableImGui = true,
        .enableMultiViewport = options.twoWindows,
        .renderService =
            {
                .applicationName = "RhiSwapchainLab",
                .backend = luna::RHIBackend::Vulkan,
                .swapchain = options.swapchainDesc,
                .renderPipeline = renderPipeline,
            },
    };

    std::unique_ptr<luna::Application> app = std::make_unique<luna::Application>(specification);
    if (!app->isInitialized()) {
        app.reset();
        luna::Logger::shutdown();
        return 1;
    }

    app->pushLayer(std::make_unique<RhiSwapchainLabLayer>(state));
    if (options.selfTestMode != SelfTestMode::None) {
        app->pushLayer(std::make_unique<RhiSwapchainLabSelfTestLayer>(state, selfTestResult, options.selfTestMode));
    }
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return options.selfTestMode != SelfTestMode::None && !selfTestResult->passed ? 1 : 0;
}
