#include "BindingLabPipeline.h"
#include "BindingLabState.h"
#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"

#include <imgui.h>

#include <algorithm>
#include <memory>
#include <string_view>

namespace {

struct CommandLineOptions {
    bool runSelfTest = false;
    std::string_view selfTestName = "phase3_descriptor_recycle";
};

struct SelfTestResult {
    bool passed = false;
};

bool parse_arguments(int argc, char** argv, CommandLineOptions* options)
{
    if (options == nullptr) {
        return false;
    }

    constexpr std::string_view kSelfTestPrefix = "--self-test=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--self-test") {
            options->runSelfTest = true;
            continue;
        }

        if (argument.substr(0, kSelfTestPrefix.size()) == kSelfTestPrefix) {
            const std::string_view selfTestName = argument.substr(kSelfTestPrefix.size());
            if (selfTestName != "phase3_descriptor_recycle" && selfTestName != "phase5_layout_validation") {
                LUNA_CORE_ERROR("Unknown self-test '{}'", argument.substr(kSelfTestPrefix.size()));
                return false;
            }

            options->runSelfTest = true;
            options->selfTestName = selfTestName;
            continue;
        }

        LUNA_CORE_ERROR("Unknown argument '{}'", argument);
        return false;
    }

    return true;
}

const char* page_label(binding_lab::Page page)
{
    switch (page) {
        case binding_lab::Page::MultiSet:
            return "Multi-Set";
        case binding_lab::Page::DescriptorArray:
            return "Descriptor Array";
        case binding_lab::Page::DynamicUniform:
            return "Dynamic Uniform";
        default:
            return "Unknown";
    }
}

class RhiBindingLabLayer final : public luna::Layer {
public:
    explicit RhiBindingLabLayer(std::shared_ptr<binding_lab::State> state)
        : luna::Layer("RhiBindingLabLayer"),
          m_state(std::move(state))
    {}

    void onImGuiRender() override
    {
        if (m_state == nullptr) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(560.0f, 760.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RhiBindingLab")) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Week 4 binding model validation");
        ImGui::Text("Current Page: %s", page_label(m_state->page));
        ImGui::Separator();

        draw_page_button(binding_lab::Page::MultiSet);
        ImGui::SameLine();
        draw_page_button(binding_lab::Page::DescriptorArray);
        ImGui::SameLine();
        draw_page_button(binding_lab::Page::DynamicUniform);

        ImGui::Separator();
        switch (m_state->page) {
            case binding_lab::Page::MultiSet:
                draw_multi_set();
                break;
            case binding_lab::Page::DescriptorArray:
                draw_descriptor_array();
                break;
            case binding_lab::Page::DynamicUniform:
                draw_dynamic_uniform();
                break;
        }

        ImGui::End();
    }

private:
    void draw_page_button(binding_lab::Page page)
    {
        const bool selected = m_state->page == page;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.76f, 0.34f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.40f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.68f, 0.28f, 0.08f, 1.0f));
        }

        if (ImGui::Button(page_label(page), ImVec2(164.0f, 0.0f))) {
            m_state->page = page;
        }

        if (selected) {
            ImGui::PopStyleColor(3);
        }
    }

    void draw_color_editor(const char* label, std::array<float, 4>* color)
    {
        ImGui::ColorEdit4(label, color->data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB);
    }

    void draw_multi_set()
    {
        auto& multiSet = m_state->multiSet;

        ImGui::TextUnformatted("Layout Composition");
        ImGui::Checkbox("Include Set 0", &multiSet.includeSet0);
        ImGui::SameLine();
        ImGui::Checkbox("Include Set 1", &multiSet.includeSet1);
        ImGui::SameLine();
        ImGui::Checkbox("Include Set 2", &multiSet.includeSet2);

        if (ImGui::Button("Build Pipeline Layout")) {
            multiSet.buildLayoutRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Test Duplicate Set Conflict")) {
            multiSet.conflictTestRequested = true;
        }

        for (const std::string& summary : multiSet.layoutSummaries) {
            if (!summary.empty()) {
                ImGui::BulletText("%s", summary.c_str());
            }
        }
        ImGui::TextWrapped("%s", multiSet.layoutStatus.empty() ? "Build a probe pipeline layout from selected set layouts."
                                                               : multiSet.layoutStatus.c_str());

        ImGui::Separator();
        ImGui::TextUnformatted("Multi-Set Sample");
        draw_color_editor("Global Tint", &multiSet.globalTint);
        draw_color_editor("Material Color", &multiSet.materialColor);
        ImGui::SliderFloat2("Object Offset", multiSet.objectOffset.data(), -0.45f, 0.45f);

        ImGui::Checkbox("Bind Global Set", &multiSet.bindGlobal);
        ImGui::SameLine();
        ImGui::Checkbox("Bind Material Set", &multiSet.bindMaterial);
        ImGui::SameLine();
        ImGui::Checkbox("Bind Object Set", &multiSet.bindObject);

        ImGui::TextWrapped("%s", multiSet.sampleStatus.empty() ? "Global / Material / Object are backed by separate sets."
                                                               : multiSet.sampleStatus.c_str());
    }

    void draw_descriptor_array()
    {
        auto& descriptorArray = m_state->descriptorArray;
        ImGui::TextUnformatted("Binding 0 Array Count: 4");
        ImGui::SliderInt("Texture Index", &descriptorArray.textureIndex, 0, 3);
        if (ImGui::Button("Replace Slot 2")) {
            descriptorArray.replaceSlotRequested = true;
        }

        for (int index = 0; index < 4; ++index) {
            const std::string label = "Slot " + std::to_string(index) + ": " + descriptorArray.slotLabels[static_cast<size_t>(index)];
            ImGui::BulletText("%s", label.c_str());
        }

        ImGui::TextWrapped("%s", descriptorArray.status.empty() ? "Shader reads textures[Texture Index] from a descriptor array."
                                                                : descriptorArray.status.c_str());
    }

    void draw_dynamic_uniform()
    {
        auto& dynamicUniform = m_state->dynamicUniform;
        ImGui::SliderInt("Object Index", &dynamicUniform.objectIndex, 0, 3);
        ImGui::Text("Dynamic Offset: %u bytes", dynamicUniform.dynamicOffset);
        ImGui::Text("Min UBO Alignment: %u", dynamicUniform.minUniformBufferOffsetAlignment);
        ImGui::Text("Frames In Flight: %u", dynamicUniform.framesInFlight);
        if (!dynamicUniform.limitsSummary.empty()) {
            ImGui::TextWrapped("%s", dynamicUniform.limitsSummary.c_str());
        }
        ImGui::TextWrapped("%s", dynamicUniform.status.empty() ? "Same set layout, different object data via dynamic offset."
                                                               : dynamicUniform.status.c_str());
    }

private:
    std::shared_ptr<binding_lab::State> m_state;
};

class RhiBindingLabSelfTestLayer final : public luna::Layer {
public:
    RhiBindingLabSelfTestLayer(std::shared_ptr<binding_lab::State> state,
                               std::shared_ptr<SelfTestResult> result,
                               std::string_view selfTestName)
        : luna::Layer("RhiBindingLabSelfTestLayer"),
          m_state(std::move(state)),
          m_result(std::move(result)),
          m_selfTestName(selfTestName)
    {}

    void onAttach() override
    {
        LUNA_CORE_INFO("RhiBindingLab self-test begin: {}", m_selfTestName);
    }

    void onUpdate(luna::Timestep) override
    {
        if (m_state == nullptr || m_result == nullptr) {
            return;
        }

        if (m_selfTestName == "phase5_layout_validation") {
            update_phase5_layout_validation();
            return;
        }

        update_phase3_descriptor_recycle();
    }

private:
    void update_phase3_descriptor_recycle()
    {
        m_state->page = binding_lab::Page::DescriptorArray;
        auto& descriptorArray = m_state->descriptorArray;

        if (!m_hasLastAlternate) {
            m_lastAlternate = descriptorArray.slot2UsesAlternate;
            m_hasLastAlternate = true;
        } else if (descriptorArray.slot2UsesAlternate != m_lastAlternate) {
            ++m_toggleCount;
            m_lastAlternate = descriptorArray.slot2UsesAlternate;
        }

        ++m_frame;
        if (m_frame >= 2 && m_frame <= 48 && (m_frame % 2) == 0) {
            descriptorArray.textureIndex = (m_frame / 2) % 4;
            descriptorArray.replaceSlotRequested = true;
            ++m_requestedReplacements;
        }

        if (m_frame >= 60) {
            finish();
        }
    }

private:
    void finish()
    {
        const auto& descriptorArray = m_state->descriptorArray;
        const bool passed = m_requestedReplacements >= 20 && m_toggleCount >= 10 && !descriptorArray.status.empty();
        m_result->passed = passed;

        if (passed) {
            LUNA_CORE_INFO("RhiBindingLab self-test PASS replacements={} toggles={} slot2='{}'",
                           m_requestedReplacements,
                           m_toggleCount,
                           descriptorArray.slotLabels[2]);
        } else {
            LUNA_CORE_ERROR("RhiBindingLab self-test FAIL replacements={} toggles={} status='{}'",
                            m_requestedReplacements,
                            m_toggleCount,
                            descriptorArray.status);
        }

        luna::Application::get().close();
    }

    void update_phase5_layout_validation()
    {
        m_state->page = binding_lab::Page::MultiSet;
        auto& multiSet = m_state->multiSet;
        multiSet.buildLayoutRequested = m_buildRequestPending;

        ++m_frame;
        if (m_frame == 2) {
            m_buildRequestPending = true;
            multiSet.buildLayoutRequested = true;
        }

        if (!m_buildValidated &&
            multiSet.layoutStatus.find("reflection validation passed") != std::string::npos) {
            m_buildValidated = true;
            m_buildRequestPending = false;
            multiSet.buildLayoutRequested = false;
            if (!m_conflictRequestIssued) {
                multiSet.conflictTestRequested = true;
                m_conflictRequestIssued = true;
            }
        } else if (m_buildValidated &&
                   multiSet.layoutStatus.find("Conflict rejected") != std::string::npos) {
            m_conflictValidated = true;
        }

        if (m_frame >= 90 || (m_buildValidated && m_conflictValidated)) {
            const bool passed = m_buildValidated && m_conflictValidated;
            m_result->passed = passed;
            if (passed) {
                LUNA_CORE_INFO("RhiBindingLab phase 5 self-test PASS build='{}' conflict='{}'",
                               multiSet.layoutStatus,
                               multiSet.layoutStatus);
            } else {
                LUNA_CORE_ERROR("RhiBindingLab phase 5 self-test FAIL buildValidated={} conflictValidated={} status='{}'",
                                m_buildValidated ? "true" : "false",
                                m_conflictValidated ? "true" : "false",
                                multiSet.layoutStatus);
            }
            luna::Application::get().close();
        }
    }

private:
    std::shared_ptr<binding_lab::State> m_state;
    std::shared_ptr<SelfTestResult> m_result;
    std::string_view m_selfTestName;
    uint32_t m_frame = 0;
    int m_requestedReplacements = 0;
    int m_toggleCount = 0;
    bool m_lastAlternate = false;
    bool m_hasLastAlternate = false;
    bool m_buildValidated = false;
    bool m_conflictValidated = false;
    bool m_buildRequestPending = false;
    bool m_conflictRequestIssued = false;
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

    std::shared_ptr<binding_lab::State> state = std::make_shared<binding_lab::State>();
    std::shared_ptr<SelfTestResult> selfTestResult = std::make_shared<SelfTestResult>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline =
        std::make_shared<binding_lab::RhiBindingLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiBindingLab",
        .windowWidth = 1440,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = !options.runSelfTest,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "RhiBindingLab",
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
        app->pushLayer(std::make_unique<RhiBindingLabSelfTestLayer>(state, selfTestResult, options.selfTestName));
    } else {
        app->pushLayer(std::make_unique<RhiBindingLabLayer>(state));
    }
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return options.runSelfTest && !selfTestResult->passed ? 1 : 0;
}
