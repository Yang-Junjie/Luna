#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"
#include "ImageViewLabPipeline.h"
#include "ImageViewLabState.h"

#include <imgui.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

namespace {

struct CommandLineOptions {
    bool runSelfTest = false;
    std::string selfTestName = "phase3_deferred_destroy";
};

struct SelfTestResult {
    bool passed = false;
};

bool parse_arguments(int argc, char** argv, CommandLineOptions* options)
{
    if (options == nullptr) {
        return false;
    }

    constexpr std::array<std::string_view, 2> kSelfTestNames = {
        "phase3_deferred_destroy",
        "phase5_subresource_preview",
    };
    constexpr std::string_view kSelfTestPrefix = "--self-test=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--self-test") {
            options->runSelfTest = true;
            continue;
        }

        if (argument.substr(0, kSelfTestPrefix.size()) == kSelfTestPrefix) {
            const std::string_view selfTestName = argument.substr(kSelfTestPrefix.size());
            if (std::find(kSelfTestNames.begin(), kSelfTestNames.end(), selfTestName) == kSelfTestNames.end()) {
                LUNA_CORE_ERROR("Unknown self-test '{}'", argument.substr(kSelfTestPrefix.size()));
                return false;
            }

            options->runSelfTest = true;
            options->selfTestName = std::string(selfTestName);
            continue;
        }

        LUNA_CORE_ERROR("Unknown argument '{}'", argument);
        return false;
    }

    return true;
}

const char* page_label(image_view_lab::Page page)
{
    switch (page) {
        case image_view_lab::Page::MipView:
            return "Mip View";
        case image_view_lab::Page::ArrayLayerView:
            return "Array Layer View";
        case image_view_lab::Page::Slice3DView:
            return "3D Slice View";
        default:
            return "Unknown";
    }
}

const char* view_type_label(luna::ImageViewType type)
{
    return luna::to_string(type).data();
}

bool image_view_type_combo(const char* label, luna::ImageViewType* value, bool include3D)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, view_type_label(*value))) {
        const luna::ImageViewType kTypes[] = {
            luna::ImageViewType::Image2D,
            luna::ImageViewType::Image2DArray,
            luna::ImageViewType::Image3D,
        };
        const int typeCount = include3D ? 3 : 2;
        for (int index = 0; index < typeCount; ++index) {
            const luna::ImageViewType type = kTypes[index];
            const bool selected = type == *value;
            if (ImGui::Selectable(view_type_label(type), selected)) {
                *value = type;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

class RhiImageViewLabLayer final : public luna::Layer {
public:
    explicit RhiImageViewLabLayer(std::shared_ptr<image_view_lab::State> state)
        : luna::Layer("RhiImageViewLabLayer"),
          m_state(std::move(state))
    {}

    void onImGuiRender() override
    {
        if (m_state == nullptr) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(520.0f, 760.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RhiImageViewLab")) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Week 3 image view / subresource validation");
        ImGui::Text("Current Page: %s", page_label(m_state->page));
        ImGui::Separator();

        draw_page_button(image_view_lab::Page::MipView);
        ImGui::SameLine();
        draw_page_button(image_view_lab::Page::ArrayLayerView);
        ImGui::SameLine();
        draw_page_button(image_view_lab::Page::Slice3DView);

        ImGui::Separator();
        switch (m_state->page) {
            case image_view_lab::Page::MipView:
                draw_mip_view();
                break;
            case image_view_lab::Page::ArrayLayerView:
                draw_array_layer_view();
                break;
            case image_view_lab::Page::Slice3DView:
                draw_slice_3d_view();
                break;
        }

        ImGui::End();
    }

private:
    void draw_page_button(image_view_lab::Page page)
    {
        const bool selected = m_state->page == page;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.26f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.52f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.37f, 0.22f, 1.0f));
        }

        if (ImGui::Button(page_label(page), ImVec2(150.0f, 0.0f))) {
            m_state->page = page;
        }

        if (selected) {
            ImGui::PopStyleColor(3);
        }
    }

    void draw_view_list(std::vector<image_view_lab::ViewRecord>& views, int* selectedView, int* deleteView)
    {
        ImGui::Separator();
        ImGui::Text("Views (%d)", static_cast<int>(views.size()));

        for (int index = 0; index < static_cast<int>(views.size()); ++index) {
            const bool selected = *selectedView == index;
            if (ImGui::Selectable(views[static_cast<size_t>(index)].label.c_str(), selected)) {
                *selectedView = index;
            }
        }

        const bool canDelete = *selectedView >= 0 && *selectedView < static_cast<int>(views.size());
        ImGui::BeginDisabled(!canDelete);
        if (ImGui::Button("Delete Selected View")) {
            *deleteView = *selectedView;
        }
        ImGui::EndDisabled();
    }

    void draw_mip_view()
    {
        auto& mip = m_state->mip;
        ImGui::Text("Source Image: %ux%u, total mips=%u", mip.width, mip.height, mip.mipLevels);

        int baseMip = static_cast<int>(mip.createBaseMip);
        int mipCount = static_cast<int>(mip.createMipCount);
        ImGui::SliderInt("Base Mip", &baseMip, 0, static_cast<int>(std::max(1u, mip.mipLevels) - 1u));
        ImGui::SliderInt("Mip Count", &mipCount, 1, static_cast<int>(mip.mipLevels - static_cast<uint32_t>(baseMip)));
        mip.createBaseMip = static_cast<uint32_t>(std::max(baseMip, 0));
        mip.createMipCount = static_cast<uint32_t>(std::max(mipCount, 1));

        if (ImGui::Button("Recreate Source Image")) {
            mip.recreateImageRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Create View")) {
            mip.createViewRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Render To Selected View")) {
            mip.renderSelectedViewRequested = true;
        }

        const int selectedView = std::clamp(mip.selectedView, 0, std::max(0, static_cast<int>(mip.views.size()) - 1));
        if (!mip.views.empty()) {
            const auto& view = mip.views[static_cast<size_t>(selectedView)];
            ImGui::SliderFloat("View Local LOD", &mip.previewLod, 0.0f, static_cast<float>(std::max(1u, view.desc.mipCount) - 1u));
            ImGui::Text("Selected View Range: mip %u .. %u",
                        view.desc.baseMipLevel,
                        view.desc.baseMipLevel + view.desc.mipCount - 1);
        }

        draw_view_list(mip.views, &mip.selectedView, &mip.deleteView);
        ImGui::TextWrapped("%s", mip.status.empty() ? "Create single mip or mip range views from one source image, then render into the selected view."
                                                    : mip.status.c_str());
    }

    void draw_array_layer_view()
    {
        auto& array = m_state->array;
        ImGui::Text("Source Array Image: %ux%u, layers=%u, total mips=%u",
                    array.width,
                    array.height,
                    array.arrayLayers,
                    array.mipLevels);

        image_view_type_combo("View Type", &array.createType, false);

        int baseMip = static_cast<int>(array.createBaseMip);
        int mipCount = static_cast<int>(array.createMipCount);
        int baseLayer = static_cast<int>(array.createBaseLayer);
        int layerCount = static_cast<int>(array.createLayerCount);
        ImGui::SliderInt("Base Mip", &baseMip, 0, static_cast<int>(std::max(1u, array.mipLevels) - 1u));
        ImGui::SliderInt("Mip Count", &mipCount, 1, static_cast<int>(array.mipLevels - static_cast<uint32_t>(baseMip)));
        ImGui::SliderInt("Base Layer", &baseLayer, 0, static_cast<int>(std::max(1u, array.arrayLayers) - 1u));
        ImGui::SliderInt("Layer Count",
                         &layerCount,
                         1,
                         static_cast<int>(array.arrayLayers - static_cast<uint32_t>(baseLayer)));

        array.createBaseMip = static_cast<uint32_t>(std::max(baseMip, 0));
        array.createMipCount = static_cast<uint32_t>(std::max(mipCount, 1));
        array.createBaseLayer = static_cast<uint32_t>(std::max(baseLayer, 0));
        array.createLayerCount =
            array.createType == luna::ImageViewType::Image2D ? 1u : static_cast<uint32_t>(std::max(layerCount, 1));

        if (ImGui::Button("Recreate Source Image")) {
            array.recreateImageRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Create View")) {
            array.createViewRequested = true;
        }

        const int selectedView = std::clamp(array.selectedView, 0, std::max(0, static_cast<int>(array.views.size()) - 1));
        if (!array.views.empty()) {
            const auto& view = array.views[static_cast<size_t>(selectedView)];
            if (view.desc.type == luna::ImageViewType::Image2DArray) {
                ImGui::SliderInt("Local Layer", &array.previewLayer, 0, static_cast<int>(std::max(1u, view.desc.layerCount) - 1u));
            } else {
                array.previewLayer = 0;
            }
            ImGui::SliderFloat("View Local LOD", &array.previewLod, 0.0f, static_cast<float>(std::max(1u, view.desc.mipCount) - 1u));
            ImGui::Text("Selected View: type=%s, layers=%u .. %u",
                        luna::to_string(view.desc.type).data(),
                        view.desc.baseArrayLayer,
                        view.desc.baseArrayLayer + view.desc.layerCount - 1);
        }

        draw_view_list(array.views, &array.selectedView, &array.deleteView);
        ImGui::TextWrapped("%s", array.status.empty() ? "Create single-layer 2D views or layer-range 2D-array views."
                                                      : array.status.c_str());
    }

    void draw_slice_3d_view()
    {
        auto& volume = m_state->volume;
        ImGui::Text("Source 3D Image: %ux%ux%u, total mips=%u",
                    volume.width,
                    volume.height,
                    volume.depth,
                    volume.mipLevels);

        int baseMip = static_cast<int>(volume.createBaseMip);
        int mipCount = static_cast<int>(volume.createMipCount);
        ImGui::SliderInt("Base Mip", &baseMip, 0, static_cast<int>(std::max(1u, volume.mipLevels) - 1u));
        ImGui::SliderInt("Mip Count", &mipCount, 1, static_cast<int>(volume.mipLevels - static_cast<uint32_t>(baseMip)));
        volume.createBaseMip = static_cast<uint32_t>(std::max(baseMip, 0));
        volume.createMipCount = static_cast<uint32_t>(std::max(mipCount, 1));

        if (ImGui::Button("Recreate Source Image")) {
            volume.recreateImageRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Create View")) {
            volume.createViewRequested = true;
        }

        const int selectedView = std::clamp(volume.selectedView, 0, std::max(0, static_cast<int>(volume.views.size()) - 1));
        if (!volume.views.empty()) {
            const auto& view = volume.views[static_cast<size_t>(selectedView)];
            ImGui::SliderInt("Slice Index", &volume.previewSlice, 0, static_cast<int>(std::max(1u, volume.depth) - 1u));
            ImGui::SliderFloat("View Local LOD", &volume.previewLod, 0.0f, static_cast<float>(std::max(1u, view.desc.mipCount) - 1u));
            ImGui::Text("Selected View Range: mip %u .. %u",
                        view.desc.baseMipLevel,
                        view.desc.baseMipLevel + view.desc.mipCount - 1);
        }

        draw_view_list(volume.views, &volume.selectedView, &volume.deleteView);
        ImGui::TextWrapped("%s", volume.status.empty() ? "Use 3D views plus a slice index to debug volume subresources."
                                                       : volume.status.c_str());
    }

private:
    std::shared_ptr<image_view_lab::State> m_state;
};

class RhiImageViewLabSelfTestLayer final : public luna::Layer {
public:
    RhiImageViewLabSelfTestLayer(std::shared_ptr<image_view_lab::State> state,
                                 std::shared_ptr<SelfTestResult> result,
                                 std::string selfTestName)
        : luna::Layer("RhiImageViewLabSelfTestLayer"),
          m_state(std::move(state)),
          m_result(std::move(result)),
          m_selfTestName(std::move(selfTestName))
    {}

    void onAttach() override
    {
        LUNA_CORE_INFO("RhiImageViewLab self-test begin: {}", m_selfTestName);
    }

    void onUpdate(luna::Timestep) override
    {
        if (m_state == nullptr || m_result == nullptr) {
            return;
        }

        if (m_selfTestName == "phase5_subresource_preview") {
            update_phase5_subresource_preview();
            return;
        }

        update_phase3_deferred_destroy();
    }

private:
    void update_phase3_deferred_destroy()
    {
        m_state->page = image_view_lab::Page::MipView;
        auto& mip = m_state->mip;

        if (!m_hasPreviousViewCount) {
            m_previousViewCount = mip.views.size();
            m_hasPreviousViewCount = true;
        } else {
            if (mip.views.size() > m_previousViewCount) {
                m_createdViews += static_cast<int>(mip.views.size() - m_previousViewCount);
            } else if (mip.views.size() < m_previousViewCount) {
                m_deletedViews += static_cast<int>(m_previousViewCount - mip.views.size());
            }
            m_previousViewCount = mip.views.size();
        }

        ++m_frame;
        switch (m_frame) {
            case 2:
                request_create(2, 1);
                break;
            case 4:
                request_delete_selected();
                break;
            case 6:
                request_create(3, 1);
                break;
            case 8:
                request_delete_selected();
                break;
            case 10:
                mip.recreateImageRequested = true;
                m_recreateIssued = true;
                break;
            case 12:
                request_create(1, 2);
                break;
            case 14:
                request_delete_selected();
                break;
            case 16:
                request_create(0, 1);
                break;
            default:
                break;
        }

        if (m_frame >= 40) {
            finish_phase3();
        }
    }

    void update_phase5_subresource_preview()
    {
        ++m_frame;
        switch (m_phase5Step) {
            case 0:
                m_state->page = image_view_lab::Page::MipView;
                if (!m_state->mip.views.empty()) {
                    const uint32_t maxMip = std::max(1u, m_state->mip.mipLevels);
                    m_phase5MipTarget = std::min(2u, maxMip - 1u);
                    m_state->mip.createBaseMip = m_phase5MipTarget;
                    m_state->mip.createMipCount = 1;
                    m_state->mip.createViewRequested = true;
                    m_phase5Step = 1;
                }
                break;
            case 1:
                m_state->page = image_view_lab::Page::MipView;
                if (select_mip_view(m_phase5MipTarget, 1u)) {
                    reset_phase5_probe();
                    m_state->mip.phase5StampRequested = true;
                    m_state->phase5Probe.request = image_view_lab::Phase5ProbeKind::MipIsolation;
                    m_phase5Step = 2;
                }
                break;
            case 2:
                m_state->page = image_view_lab::Page::MipView;
                if (consume_phase5_probe(image_view_lab::Phase5ProbeKind::MipIsolation, &m_phase5MipSummary)) {
                    m_phase5MipPassed = m_state->phase5Probe.passed;
                    if (!m_phase5MipPassed) {
                        finish_phase5();
                        return;
                    }

                    m_state->page = image_view_lab::Page::ArrayLayerView;
                    m_phase5ArrayLayerTarget = std::min(2u, std::max(1u, m_state->array.arrayLayers) - 1u);
                    m_state->array.createType = luna::ImageViewType::Image2D;
                    m_state->array.createBaseMip = 0;
                    m_state->array.createMipCount = 1;
                    m_state->array.createBaseLayer = m_phase5ArrayLayerTarget;
                    m_state->array.createLayerCount = 1;
                    m_state->array.createViewRequested = true;
                    m_phase5Step = 3;
                }
                break;
            case 3:
                m_state->page = image_view_lab::Page::ArrayLayerView;
                if (select_array_view(m_phase5ArrayLayerTarget)) {
                    reset_phase5_probe();
                    m_state->phase5Probe.request = image_view_lab::Phase5ProbeKind::ArrayLayerIsolation;
                    m_phase5Step = 4;
                }
                break;
            case 4:
                m_state->page = image_view_lab::Page::ArrayLayerView;
                if (consume_phase5_probe(image_view_lab::Phase5ProbeKind::ArrayLayerIsolation, &m_phase5ArraySummary)) {
                    m_phase5ArrayPassed = m_state->phase5Probe.passed;
                    finish_phase5();
                }
                break;
            default:
                break;
        }

        if (m_frame >= 180) {
            m_phase5FailureReason = "timeout while waiting for subresource preview probe results";
            finish_phase5();
        }
    }

    void request_create(uint32_t baseMip, uint32_t mipCount)
    {
        if (m_state == nullptr) {
            return;
        }

        auto& mip = m_state->mip;
        const uint32_t maxMip = std::max(1u, mip.mipLevels);
        mip.createBaseMip = std::min(baseMip, maxMip - 1u);
        mip.createMipCount = std::max(1u, std::min(mipCount, maxMip - mip.createBaseMip));
        mip.createViewRequested = true;
    }

    void request_delete_selected()
    {
        if (m_state == nullptr) {
            return;
        }

        auto& mip = m_state->mip;
        if (mip.views.size() <= 1) {
            return;
        }

        mip.selectedView = static_cast<int>(mip.views.size() - 1);
        mip.deleteView = mip.selectedView;
    }

    bool select_mip_view(uint32_t baseMip, uint32_t mipCount)
    {
        auto& mip = m_state->mip;
        for (int index = 0; index < static_cast<int>(mip.views.size()); ++index) {
            const auto& view = mip.views[static_cast<size_t>(index)];
            if (view.desc.baseMipLevel == baseMip && view.desc.mipCount == mipCount) {
                mip.selectedView = index;
                mip.previewLod = 0.0f;
                return true;
            }
        }
        return false;
    }

    bool select_array_view(uint32_t baseLayer)
    {
        auto& array = m_state->array;
        for (int index = 0; index < static_cast<int>(array.views.size()); ++index) {
            const auto& view = array.views[static_cast<size_t>(index)];
            if (view.desc.type == luna::ImageViewType::Image2D &&
                view.desc.baseArrayLayer == baseLayer &&
                view.desc.layerCount == 1 &&
                view.desc.baseMipLevel == 0 &&
                view.desc.mipCount == 1) {
                array.selectedView = index;
                array.previewLayer = 0;
                array.previewLod = 0.0f;
                return true;
            }
        }
        return false;
    }

    void reset_phase5_probe()
    {
        m_state->phase5Probe = {};
    }

    bool consume_phase5_probe(image_view_lab::Phase5ProbeKind expectedKind, std::string* summary)
    {
        const auto& probe = m_state->phase5Probe;
        if (!probe.ready || probe.completed != expectedKind) {
            return false;
        }

        if (summary != nullptr) {
            *summary = probe.summary;
        }
        return true;
    }

    void finish_phase3()
    {
        auto& mip = m_state->mip;
        const bool passed = m_recreateIssued && m_createdViews >= 4 && m_deletedViews >= 3 && !mip.views.empty();
        m_result->passed = passed;

        if (passed) {
            LUNA_CORE_INFO("RhiImageViewLab self-test PASS createdViews={} deletedViews={} finalViews={}",
                           m_createdViews,
                           m_deletedViews,
                           mip.views.size());
        } else {
            LUNA_CORE_ERROR("RhiImageViewLab self-test FAIL createdViews={} deletedViews={} recreated={} finalViews={}",
                            m_createdViews,
                            m_deletedViews,
                            m_recreateIssued ? "true" : "false",
                            mip.views.size());
        }

        luna::Application::get().close();
    }

    void finish_phase5()
    {
        if (!m_phase5FailureReason.empty()) {
            m_result->passed = false;
            LUNA_CORE_ERROR("RhiImageViewLab phase 5 self-test FAIL reason='{}' mip='{}' array='{}'",
                            m_phase5FailureReason,
                            m_phase5MipSummary,
                            m_phase5ArraySummary);
            luna::Application::get().close();
            return;
        }

        const bool passed = m_phase5MipPassed && m_phase5ArrayPassed;
        m_result->passed = passed;
        if (passed) {
            LUNA_CORE_INFO("RhiImageViewLab phase 5 self-test PASS mip='{}' array='{}'",
                           m_phase5MipSummary,
                           m_phase5ArraySummary);
        } else {
            LUNA_CORE_ERROR("RhiImageViewLab phase 5 self-test FAIL mipPassed={} arrayPassed={} mip='{}' array='{}'",
                            m_phase5MipPassed ? "true" : "false",
                            m_phase5ArrayPassed ? "true" : "false",
                            m_phase5MipSummary,
                            m_phase5ArraySummary);
        }

        luna::Application::get().close();
    }

private:
    std::shared_ptr<image_view_lab::State> m_state;
    std::shared_ptr<SelfTestResult> m_result;
    std::string m_selfTestName;
    uint32_t m_frame = 0;
    size_t m_previousViewCount = 0;
    int m_createdViews = 0;
    int m_deletedViews = 0;
    bool m_hasPreviousViewCount = false;
    bool m_recreateIssued = false;
    int m_phase5Step = 0;
    uint32_t m_phase5MipTarget = 0;
    uint32_t m_phase5ArrayLayerTarget = 0;
    bool m_phase5MipPassed = false;
    bool m_phase5ArrayPassed = false;
    std::string m_phase5MipSummary;
    std::string m_phase5ArraySummary;
    std::string m_phase5FailureReason;
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

    std::shared_ptr<image_view_lab::State> state = std::make_shared<image_view_lab::State>();
    std::shared_ptr<SelfTestResult> selfTestResult = std::make_shared<SelfTestResult>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline =
        std::make_shared<image_view_lab::RhiImageViewLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiImageViewLab",
        .windowWidth = 1440,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = !options.runSelfTest,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "RhiImageViewLab",
                .backend = luna::RHIBackend::Vulkan,
                .renderPipeline = renderPipeline,
            },
    };

    std::unique_ptr<luna::Application> app = std::make_unique<luna::Application>(specification);
    if (!app->isInitialized()) {
        app.reset();
        luna::Logger::shutdown();
        return 1;
    }

    if (options.runSelfTest) {
        app->pushLayer(std::make_unique<RhiImageViewLabSelfTestLayer>(state, selfTestResult, options.selfTestName));
    } else {
        app->pushLayer(std::make_unique<RhiImageViewLabLayer>(state));
    }
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return options.runSelfTest && !selfTestResult->passed ? 1 : 0;
}
