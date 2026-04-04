#include "BindingLabPipeline.h"
#include "BindingLabState.h"
#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"

#include <imgui.h>

#include <algorithm>
#include <memory>

namespace {

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
        ImGui::TextWrapped("%s", dynamicUniform.status.empty() ? "Same set layout, different object data via dynamic offset."
                                                               : dynamicUniform.status.c_str());
    }

private:
    std::shared_ptr<binding_lab::State> m_state;
};

} // namespace

int main()
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    std::shared_ptr<binding_lab::State> state = std::make_shared<binding_lab::State>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline =
        std::make_shared<binding_lab::RhiBindingLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiBindingLab",
        .windowWidth = 1440,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = true,
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

    app->pushLayer(std::make_unique<RhiBindingLabLayer>(state));
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return 0;
}
