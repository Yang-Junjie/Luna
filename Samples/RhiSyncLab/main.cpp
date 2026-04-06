#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"
#include "SyncLabPipeline.h"
#include "SyncLabState.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>

namespace {

struct CommandLineOptions {
    bool runSelfTest = false;
    std::string_view selfTestName = "phase3_upload_ring";
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
            if (selfTestName != "phase3_upload_ring" && selfTestName != "phase5_subresource_barrier") {
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

const char* page_label(sync_lab::Page page)
{
    switch (page) {
        case sync_lab::Page::HistoryCopy:
            return "History Copy";
        case sync_lab::Page::Readback:
            return "Readback";
        case sync_lab::Page::Indirect:
            return "Indirect";
        case sync_lab::Page::Subresource:
            return "Subresource";
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
        ImGui::SameLine();
        draw_page_button(sync_lab::Page::Subresource);

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
            case sync_lab::Page::Subresource:
                draw_subresource();
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

        if (ImGui::Button(page_label(page), ImVec2(140.0f, 0.0f))) {
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
        ImGui::Checkbox("Use Indirect Draw", &indirect.useIndirect);
        if (ImGui::Button("Generate Args On GPU")) {
            indirect.generateArgsRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Run Indirect")) {
            indirect.runRequested = true;
        }

        ImGui::Text("GPU Args: (%u, %u, %u)", indirect.gpuArgs[0], indirect.gpuArgs[1], indirect.gpuArgs[2]);
        ImGui::TextWrapped("%s", indirect.status.empty() ? "Generate drawIndirect args on GPU, then compare GPU indirect draw with CPU direct draw."
                                                         : indirect.status.c_str());
    }

    void draw_subresource()
    {
        auto& subresource = m_state->subresource;
        ImGui::Text("Probe Image: mipLevels=%u, arrayLayers=%u",
                    subresource.availableMipLevels,
                    subresource.availableArrayLayers);

        const int maxBaseMip = std::max(0, static_cast<int>(subresource.availableMipLevels) - 1);
        const int maxBaseLayer = std::max(0, static_cast<int>(subresource.availableArrayLayers) - 1);
        ImGui::SliderInt("Base Mip", &subresource.baseMipLevel, 0, maxBaseMip);
        ImGui::SliderInt("Mip Count",
                         &subresource.mipCount,
                         1,
                         static_cast<int>(subresource.availableMipLevels) - subresource.baseMipLevel);
        ImGui::SliderInt("Base Layer", &subresource.baseArrayLayer, 0, maxBaseLayer);
        ImGui::SliderInt("Layer Count",
                         &subresource.layerCount,
                         1,
                         static_cast<int>(subresource.availableArrayLayers) - subresource.baseArrayLayer);
        draw_enum_combo("Old Layout",
                        &subresource.oldLayout,
                        std::array<luna::ImageLayout, 5>{
                            luna::ImageLayout::Undefined,
                            luna::ImageLayout::ShaderReadOnly,
                            luna::ImageLayout::TransferSrc,
                            luna::ImageLayout::TransferDst,
                            luna::ImageLayout::General,
                        },
                        layout_label);
        draw_enum_combo("New Layout",
                        &subresource.newLayout,
                        std::array<luna::ImageLayout, 4>{
                            luna::ImageLayout::TransferDst,
                            luna::ImageLayout::TransferSrc,
                            luna::ImageLayout::ShaderReadOnly,
                            luna::ImageLayout::General,
                        },
                        layout_label);
        if (ImGui::Button("Run Barrier Only")) {
            subresource.runBarrierOnlyRequested = true;
        }

        ImGui::TextWrapped("%s",
                           subresource.barrierSummary.empty() ? "Choose a non-zero mip/layer range to test true subresource tracking."
                                                              : subresource.barrierSummary.c_str());
        ImGui::TextWrapped("%s",
                           subresource.status.empty() ? "Expected result: the selected mip/layer range is Accepted with an exact range summary."
                                                       : subresource.status.c_str());
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

class RhiSyncLabSelfTestLayer final : public luna::Layer {
public:
    RhiSyncLabSelfTestLayer(std::shared_ptr<sync_lab::State> state,
                            std::shared_ptr<SelfTestResult> result,
                            std::string_view selfTestName)
        : luna::Layer("RhiSyncLabSelfTestLayer"),
          m_state(std::move(state)),
          m_result(std::move(result)),
          m_selfTestName(selfTestName)
    {}

    void onAttach() override
    {
        LUNA_CORE_INFO("RhiSyncLab self-test begin: {}", m_selfTestName);
    }

    void onUpdate(luna::Timestep) override
    {
        if (m_state == nullptr || m_result == nullptr) {
            return;
        }

        if (m_selfTestName == "phase5_subresource_barrier") {
            update_phase5_subresource_barrier();
            return;
        }

        update_phase3_upload_ring();
    }

private:
    void update_phase3_upload_ring()
    {
        m_state->page = sync_lab::Page::Readback;
        auto& readback = m_state->readback;

        if (readback.hasReadbackData) {
            if (!m_hasLastPixel || readback.pixels[0] != m_lastFirstPixel) {
                ++m_readbackUpdates;
                m_lastFirstPixel = readback.pixels[0];
                m_hasLastPixel = true;
            }
        }

        ++m_frame;
        const uint32_t cycle = (m_frame - 1) / 6;
        const uint32_t phase = (m_frame - 1) % 6;
        if (m_frame <= 48 && phase == 0) {
            static constexpr std::array<int, 6> kRegions = {0, 2, 4, 6, 8, 12};
            readback.regionX = kRegions[cycle % kRegions.size()];
            readback.regionY = kRegions[(cycle + 2) % kRegions.size()];
            readback.copyBufferToImageRequested = true;
            ++m_copyBufferToImageCount;
        } else if (m_frame <= 48 && phase == 2) {
            readback.copyImageToBufferRequested = true;
            ++m_copyImageToBufferCount;
        }

        if (m_frame >= 64) {
            finish();
        }
    }

private:
    void finish()
    {
        const auto& readback = m_state->readback;
        const bool passed =
            m_copyBufferToImageCount >= 8 && m_copyImageToBufferCount >= 8 && m_readbackUpdates >= 4 && readback.hasReadbackData;
        m_result->passed = passed;

        if (passed) {
            LUNA_CORE_INFO("RhiSyncLab self-test PASS copyBufferToImage={} copyImageToBuffer={} readbackUpdates={} firstPixel=0x{:08X}",
                           m_copyBufferToImageCount,
                           m_copyImageToBufferCount,
                           m_readbackUpdates,
                           readback.pixels[0]);
        } else {
            LUNA_CORE_ERROR("RhiSyncLab self-test FAIL copyBufferToImage={} copyImageToBuffer={} readbackUpdates={} hasReadback={}",
                            m_copyBufferToImageCount,
                            m_copyImageToBufferCount,
                            m_readbackUpdates,
                            readback.hasReadbackData ? "true" : "false");
        }

        luna::Application::get().close();
    }

    void update_phase5_subresource_barrier()
    {
        m_state->page = sync_lab::Page::Subresource;
        auto& subresource = m_state->subresource;
        subresource.baseMipLevel = 1;
        subresource.mipCount = 1;
        subresource.baseArrayLayer = 2;
        subresource.layerCount = 1;

        ++m_frame;
        if (m_frame == 2) {
            subresource.runBarrierOnlyRequested = true;
        }

        if (subresource.status.find("Accepted") != std::string::npos) {
            m_result->passed = true;
            LUNA_CORE_INFO("RhiSyncLab phase 5 self-test PASS summary='{}' status='{}'",
                           subresource.barrierSummary,
                           subresource.status);
            luna::Application::get().close();
            return;
        }

        if (m_frame >= 60) {
            m_result->passed = false;
            LUNA_CORE_ERROR("RhiSyncLab phase 5 self-test FAIL summary='{}' status='{}'",
                            subresource.barrierSummary,
                            subresource.status);
            luna::Application::get().close();
        }
    }

private:
    std::shared_ptr<sync_lab::State> m_state;
    std::shared_ptr<SelfTestResult> m_result;
    std::string_view m_selfTestName;
    uint32_t m_frame = 0;
    uint32_t m_lastFirstPixel = 0;
    int m_copyBufferToImageCount = 0;
    int m_copyImageToBufferCount = 0;
    int m_readbackUpdates = 0;
    bool m_hasLastPixel = false;
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

    std::shared_ptr<sync_lab::State> state = std::make_shared<sync_lab::State>();
    std::shared_ptr<SelfTestResult> selfTestResult = std::make_shared<SelfTestResult>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline = std::make_shared<sync_lab::RhiSyncLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiSyncLab",
        .windowWidth = 1440,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = !options.runSelfTest,
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

    if (options.runSelfTest) {
        app->pushLayer(std::make_unique<RhiSyncLabSelfTestLayer>(state, selfTestResult, options.selfTestName));
    } else {
        app->pushLayer(std::make_unique<RhiSyncLabLayer>(state));
    }
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return options.runSelfTest && !selfTestResult->passed ? 1 : 0;
}
