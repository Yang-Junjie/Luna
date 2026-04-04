#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"
#include "SyncLabPipeline.h"
#include "SyncLabState.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <memory>

namespace {

const char* page_label(sync_lab::Page page)
{
    switch (page) {
        case sync_lab::Page::HistoryCopy:
            return "History Copy";
        case sync_lab::Page::Readback:
            return "Readback";
        case sync_lab::Page::Indirect:
            return "Indirect";
        default:
            return "Unknown";
    }
}

const char* layout_label(luna::ImageLayout layout)
{
    switch (layout) {
        case luna::ImageLayout::Undefined:
            return "Undefined";
        case luna::ImageLayout::General:
            return "General";
        case luna::ImageLayout::ColorAttachment:
            return "ColorAttachment";
        case luna::ImageLayout::DepthStencilAttachment:
            return "DepthStencilAttachment";
        case luna::ImageLayout::TransferSrc:
            return "TransferSrc";
        case luna::ImageLayout::TransferDst:
            return "TransferDst";
        case luna::ImageLayout::ShaderReadOnly:
            return "ShaderReadOnly";
        case luna::ImageLayout::Present:
            return "Present";
        default:
            return "Unknown";
    }
}

const char* stage_label(luna::PipelineStage stage)
{
    switch (stage) {
        case luna::PipelineStage::None:
            return "None";
        case luna::PipelineStage::Transfer:
            return "Transfer";
        case luna::PipelineStage::FragmentShader:
            return "FragmentShader";
        case luna::PipelineStage::ComputeShader:
            return "ComputeShader";
        case luna::PipelineStage::ColorAttachmentOutput:
            return "ColorAttachmentOutput";
        case luna::PipelineStage::Host:
            return "Host";
        case luna::PipelineStage::DrawIndirect:
            return "DrawIndirect";
        case luna::PipelineStage::AllCommands:
            return "AllCommands";
        default:
            return "Other";
    }
}

const char* access_label(luna::ResourceAccess access)
{
    switch (access) {
        case luna::ResourceAccess::None:
            return "None";
        case luna::ResourceAccess::ShaderRead:
            return "ShaderRead";
        case luna::ResourceAccess::ShaderWrite:
            return "ShaderWrite";
        case luna::ResourceAccess::TransferRead:
            return "TransferRead";
        case luna::ResourceAccess::TransferWrite:
            return "TransferWrite";
        case luna::ResourceAccess::HostRead:
            return "HostRead";
        case luna::ResourceAccess::HostWrite:
            return "HostWrite";
        case luna::ResourceAccess::IndirectCommandRead:
            return "IndirectRead";
        default:
            return "Other";
    }
}

template <typename Enum, size_t N>
bool draw_enum_combo(const char* label,
                     Enum* value,
                     const std::array<Enum, N>& values,
                     const char* (*toLabel)(Enum))
{
    int currentIndex = 0;
    for (size_t index = 0; index < values.size(); ++index) {
        if (values[index] == *value) {
            currentIndex = static_cast<int>(index);
            break;
        }
    }

    bool changed = false;
    if (ImGui::BeginCombo(label, toLabel(values[static_cast<size_t>(currentIndex)]))) {
        for (size_t index = 0; index < values.size(); ++index) {
            const bool selected = currentIndex == static_cast<int>(index);
            if (ImGui::Selectable(toLabel(values[index]), selected)) {
                *value = values[index];
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

class RhiSyncLabLayer final : public luna::Layer {
public:
    explicit RhiSyncLabLayer(std::shared_ptr<sync_lab::State> state)
        : luna::Layer("RhiSyncLabLayer"),
          m_state(std::move(state))
    {}

    void onImGuiRender() override
    {
        if (m_state == nullptr) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(620.0f, 820.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RhiSyncLab")) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Week 5 sync / copy / readback / indirect");
        ImGui::Text("Current Page: %s", page_label(m_state->page));
        ImGui::Separator();

        draw_page_button(sync_lab::Page::HistoryCopy);
        ImGui::SameLine();
        draw_page_button(sync_lab::Page::Readback);
        ImGui::SameLine();
        draw_page_button(sync_lab::Page::Indirect);

        ImGui::Separator();
        switch (m_state->page) {
            case sync_lab::Page::HistoryCopy:
                draw_history_copy();
                break;
            case sync_lab::Page::Readback:
                draw_readback();
                break;
            case sync_lab::Page::Indirect:
                draw_indirect();
                break;
        }

        ImGui::Separator();
        draw_timeline();

        ImGui::End();
    }

private:
    void draw_page_button(sync_lab::Page page)
    {
        const bool selected = m_state->page == page;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.46f, 0.66f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.52f, 0.74f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.38f, 0.58f, 1.0f));
        }

        if (ImGui::Button(page_label(page), ImVec2(186.0f, 0.0f))) {
            if (m_state->page != page) {
                LUNA_CORE_INFO("RhiSyncLab page switched to {}", page_label(page));
            }
            m_state->page = page;
        }

        if (selected) {
            ImGui::PopStyleColor(3);
        }
    }

    void draw_history_copy()
    {
        auto& history = m_state->history;
        static constexpr std::array<luna::ImageLayout, 5> kLayouts = {
            luna::ImageLayout::ShaderReadOnly,
            luna::ImageLayout::TransferSrc,
            luna::ImageLayout::TransferDst,
            luna::ImageLayout::General,
            luna::ImageLayout::Undefined,
        };
        static constexpr std::array<luna::PipelineStage, 6> kStages = {
            luna::PipelineStage::None,
            luna::PipelineStage::Transfer,
            luna::PipelineStage::FragmentShader,
            luna::PipelineStage::ComputeShader,
            luna::PipelineStage::DrawIndirect,
            luna::PipelineStage::Host,
        };
        static constexpr std::array<luna::ResourceAccess, 7> kAccess = {
            luna::ResourceAccess::None,
            luna::ResourceAccess::ShaderRead,
            luna::ResourceAccess::ShaderWrite,
            luna::ResourceAccess::TransferRead,
            luna::ResourceAccess::TransferWrite,
            luna::ResourceAccess::HostRead,
            luna::ResourceAccess::IndirectCommandRead,
        };

        ImGui::Text("Sample Frame: %d", history.sampleFrame);
        if (ImGui::Button("Advance Frame")) {
            history.advanceFrameRequested = true;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto Advance", &history.autoAdvance);
        ImGui::SameLine();
        ImGui::Checkbox("Pause Copy", &history.pauseCopy);

        ImGui::Separator();
        ImGui::TextUnformatted("Barrier Debug Panel");
        draw_enum_combo("Old Layout", &history.barrierOldLayout, kLayouts, layout_label);
        draw_enum_combo("New Layout", &history.barrierNewLayout, kLayouts, layout_label);
        draw_enum_combo("Src Stage", &history.barrierSrcStage, kStages, stage_label);
        draw_enum_combo("Dst Stage", &history.barrierDstStage, kStages, stage_label);
        draw_enum_combo("Src Access", &history.barrierSrcAccess, kAccess, access_label);
        draw_enum_combo("Dst Access", &history.barrierDstAccess, kAccess, access_label);
        if (ImGui::Button("Run Barrier Only")) {
            history.runBarrierOnlyRequested = true;
        }

        ImGui::TextWrapped("%s", history.barrierSummary.empty() ? "Barrier summary will appear here." : history.barrierSummary.c_str());
        ImGui::TextWrapped("%s", history.status.empty() ? "Current and History previews are shown side by side in the render output."
                                                        : history.status.c_str());
    }

    void draw_readback()
    {
        auto& readback = m_state->readback;
        ImGui::SliderInt("Region X", &readback.regionX, 0, 12);
        ImGui::SliderInt("Region Y", &readback.regionY, 0, 12);
        if (ImGui::Button("Copy Buffer To Image")) {
            readback.copyBufferToImageRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy Image To Buffer")) {
            readback.copyImageToBufferRequested = true;
        }

        ImGui::TextWrapped("%s", readback.status.empty() ? "Upload a pattern into the image, then read back a 4x4 block."
                                                         : readback.status.c_str());
        ImGui::Separator();
        ImGui::TextUnformatted("Readback 4x4 Block");
        if (!readback.hasReadbackData) {
            ImGui::TextUnformatted("No readback data yet.");
            return;
        }

        for (int row = 0; row < 4; ++row) {
            ImGui::Text("%08X  %08X  %08X  %08X",
                        readback.pixels[static_cast<size_t>(row * 4 + 0)],
                        readback.pixels[static_cast<size_t>(row * 4 + 1)],
                        readback.pixels[static_cast<size_t>(row * 4 + 2)],
                        readback.pixels[static_cast<size_t>(row * 4 + 3)]);
        }
    }

    void draw_indirect()
    {
        auto& indirect = m_state->indirect;
        ImGui::SliderInt("Group Count X", &indirect.desiredGroupCountX, 1, 32);
        ImGui::SliderInt("Group Count Y", &indirect.desiredGroupCountY, 1, 32);
        ImGui::Checkbox("Use Indirect Dispatch", &indirect.useIndirect);
        if (ImGui::Button("Generate Args On GPU")) {
            indirect.generateArgsRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Run Indirect")) {
            indirect.runRequested = true;
        }

        ImGui::Text("GPU Args: (%u, %u, %u)", indirect.gpuArgs[0], indirect.gpuArgs[1], indirect.gpuArgs[2]);
        ImGui::TextWrapped("%s", indirect.status.empty() ? "Generate indirect args on GPU, then compare indirect dispatch with CPU direct dispatch."
                                                         : indirect.status.c_str());
    }

    void draw_timeline()
    {
        ImGui::TextUnformatted("Resource Timeline");
        if (ImGui::Button("Clear Timeline")) {
            m_state->timeline.clear();
        }

        if (m_state->timeline.empty()) {
            ImGui::TextUnformatted("No operations recorded yet.");
            return;
        }

        for (auto it = m_state->timeline.rbegin(); it != m_state->timeline.rend(); ++it) {
            ImGui::BulletText("#%llu %s",
                              static_cast<unsigned long long>(it->serial),
                              it->label.c_str());
        }
    }

private:
    std::shared_ptr<sync_lab::State> m_state;
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

    std::shared_ptr<sync_lab::State> state = std::make_shared<sync_lab::State>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline = std::make_shared<sync_lab::RhiSyncLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiSyncLab",
        .windowWidth = 1440,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = true,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "RhiSyncLab",
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

    app->pushLayer(std::make_unique<RhiSyncLabLayer>(state));
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return 0;
}
