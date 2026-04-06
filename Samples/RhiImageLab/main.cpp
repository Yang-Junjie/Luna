#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"
#include "ImageLabPipeline.h"
#include "ImageLabState.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace {

struct CommandLineOptions {
    bool runSelfTest = false;
    std::string_view selfTestName = "phase5_smoke";
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
            if (selfTestName != "phase5_smoke") {
                LUNA_CORE_ERROR("Unknown self-test '{}'", selfTestName);
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

const char* page_label(image_lab::Page page)
{
    switch (page) {
        case image_lab::Page::FormatProbe:
            return "Format Probe";
        case image_lab::Page::AttachmentOps:
            return "Attachment Ops";
        case image_lab::Page::MRTPreview:
            return "MRT Preview";
        case image_lab::Page::MipPreview:
            return "Mip Preview";
        case image_lab::Page::Array3DPreview:
            return "Array/3D Preview";
        default:
            return "Unknown";
    }
}

bool pixel_format_combo(const char* label, luna::PixelFormat* value, std::span<const luna::PixelFormat> formats)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, luna::to_string(*value).data())) {
        for (const luna::PixelFormat format : formats) {
            const bool selected = format == *value;
            if (ImGui::Selectable(luna::to_string(format).data(), selected)) {
                *value = format;
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

bool image_type_combo(const char* label, luna::ImageType* value)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, luna::to_string(*value).data())) {
        constexpr std::array<luna::ImageType, 3> kTypes = {
            luna::ImageType::Image2D,
            luna::ImageType::Image2DArray,
            luna::ImageType::Image3D,
        };
        for (const luna::ImageType type : kTypes) {
            const bool selected = type == *value;
            if (ImGui::Selectable(luna::to_string(type).data(), selected)) {
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

bool attachment_mode_combo(const char* label, image_lab::AttachmentMode* value)
{
    bool changed = false;
    const char* currentLabel = *value == image_lab::AttachmentMode::DepthOnly ? "Depth Only" : "Color Only";
    if (ImGui::BeginCombo(label, currentLabel)) {
        constexpr std::array<image_lab::AttachmentMode, 2> kModes = {
            image_lab::AttachmentMode::ColorOnly,
            image_lab::AttachmentMode::DepthOnly,
        };
        for (const image_lab::AttachmentMode mode : kModes) {
            const char* labelText = mode == image_lab::AttachmentMode::DepthOnly ? "Depth Only" : "Color Only";
            const bool selected = mode == *value;
            if (ImGui::Selectable(labelText, selected)) {
                *value = mode;
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

bool attachment_load_op_combo(const char* label, luna::AttachmentLoadOp* value)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, luna::to_string(*value).data())) {
        constexpr std::array<luna::AttachmentLoadOp, 3> kOps = {
            luna::AttachmentLoadOp::Load,
            luna::AttachmentLoadOp::Clear,
            luna::AttachmentLoadOp::Discard,
        };
        for (const luna::AttachmentLoadOp op : kOps) {
            const bool selected = op == *value;
            if (ImGui::Selectable(luna::to_string(op).data(), selected)) {
                *value = op;
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

bool attachment_store_op_combo(const char* label, luna::AttachmentStoreOp* value)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, luna::to_string(*value).data())) {
        constexpr std::array<luna::AttachmentStoreOp, 2> kOps = {
            luna::AttachmentStoreOp::Store,
            luna::AttachmentStoreOp::Discard,
        };
        for (const luna::AttachmentStoreOp op : kOps) {
            const bool selected = op == *value;
            if (ImGui::Selectable(luna::to_string(op).data(), selected)) {
                *value = op;
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

class RhiImageLabLayer final : public luna::Layer {
public:
    explicit RhiImageLabLayer(std::shared_ptr<image_lab::State> state)
        : luna::Layer("RhiImageLabLayer"),
          m_state(std::move(state))
    {}

    void onImGuiRender() override
    {
        if (m_state == nullptr) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(480.0f, 680.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RhiImageLab")) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Week 2 unified validation sample");
        ImGui::Text("Current Page: %s", page_label(m_state->page));
        ImGui::Separator();

        draw_page_button(image_lab::Page::FormatProbe);
        ImGui::SameLine();
        draw_page_button(image_lab::Page::AttachmentOps);
        ImGui::SameLine();
        draw_page_button(image_lab::Page::MRTPreview);
        ImGui::NewLine();
        draw_page_button(image_lab::Page::MipPreview);
        ImGui::SameLine();
        draw_page_button(image_lab::Page::Array3DPreview);

        ImGui::Separator();
        switch (m_state->page) {
            case image_lab::Page::FormatProbe:
                draw_format_probe();
                break;
            case image_lab::Page::AttachmentOps:
                draw_attachment_ops();
                break;
            case image_lab::Page::MRTPreview:
                draw_mrt_preview();
                break;
            case image_lab::Page::MipPreview:
                draw_mip_preview();
                break;
            case image_lab::Page::Array3DPreview:
                draw_array3d_preview();
                break;
        }

        ImGui::End();
    }

private:
    void draw_page_button(image_lab::Page page)
    {
        const bool selected = m_state->page == page;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.37f, 0.75f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.44f, 0.84f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.31f, 0.66f, 1.0f));
        }

        if (ImGui::Button(page_label(page), ImVec2(144.0f, 0.0f))) {
            m_state->page = page;
        }

        if (selected) {
            ImGui::PopStyleColor(3);
        }
    }

    void draw_format_probe()
    {
        auto& probe = m_state->formatProbe;
        if (pixel_format_combo("Pixel Format", &probe.selectedFormat, image_lab::kFormatProbeFormats)) {
            probe.probeRequested = true;
        }

        ImGui::Text("Bits Per Pixel: %u", luna::pixel_format_bits_per_pixel(probe.selectedFormat));
        ImGui::TextWrapped("Backend Mapping: %s", probe.backendMapping.empty() ? "Pending probe" : probe.backendMapping.c_str());

        if (ImGui::Button("Probe Format")) {
            probe.probeRequested = true;
        }

        if (!probe.details.empty()) {
            ImVec4 color = probe.accepted ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f) : ImVec4(0.95f, 0.45f, 0.35f, 1.0f);
            ImGui::TextColored(color, "%s", probe.accepted ? "Accepted" : "Rejected");
            ImGui::TextWrapped("%s", probe.details.c_str());
        } else {
            ImGui::TextWrapped("Choose a format and click Probe Format to check backend acceptance.");
        }
    }

    void draw_attachment_ops()
    {
        auto& ops = m_state->attachmentOps;
        attachment_mode_combo("Pass Mode", &ops.mode);

        if (ops.mode == image_lab::AttachmentMode::ColorOnly) {
            attachment_load_op_combo("Color Load", &ops.colorLoadOp);
            attachment_store_op_combo("Color Store", &ops.colorStoreOp);
        } else {
            attachment_load_op_combo("Depth Load", &ops.depthLoadOp);
            attachment_store_op_combo("Depth Store", &ops.depthStoreOp);
        }

        ImGui::SliderFloat("Viewport Scale", &ops.viewportScale, 0.2f, 1.0f, "%.2f");
        ImGui::SliderInt("Scissor Inset", &ops.scissorInset, 0, 256);
        ImGui::TextWrapped("The sample renders every frame into a persistent offscreen attachment. "
                           "Use Load to keep previous triangles, then switch to Clear or Discard to reset the history.");
        ImGui::TextWrapped("%s", ops.status.empty() ? "Attachment ops state will appear here once rendering starts."
                                                    : ops.status.c_str());
    }

    void draw_mrt_preview()
    {
        auto& mrt = m_state->mrt;
        if (ImGui::SliderInt("Color Attachments", &mrt.attachmentCount, 1, 4)) {
            mrt.rebuildRequested = true;
            mrt.previewAttachment = std::clamp(mrt.previewAttachment, 0, mrt.attachmentCount - 1);
        }

        for (int index = 0; index < mrt.attachmentCount; ++index) {
            const std::string label = "Attachment " + std::to_string(index) + " Format";
            if (pixel_format_combo(label.c_str(),
                                   &mrt.formats[static_cast<size_t>(index)],
                                   image_lab::kMrtColorFormats)) {
                mrt.rebuildRequested = true;
            }
        }

        if (ImGui::Button("Build MRT Pipeline")) {
            mrt.rebuildRequested = true;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Show 4-up View", &mrt.showFourUpView);

        ImGui::SliderInt("Preview Attachment", &mrt.previewAttachment, 0, std::max(0, mrt.attachmentCount - 1));
        ImGui::TextWrapped("%s", mrt.status.empty() ? "Build the MRT pipeline to preview the attachments." : mrt.status.c_str());
    }

    void draw_mip_preview()
    {
        auto& mip = m_state->mip;

        int width = static_cast<int>(mip.width);
        int height = static_cast<int>(mip.height);
        const bool widthChanged = ImGui::SliderInt("Width", &width, 32, 1024);
        const bool heightChanged = ImGui::SliderInt("Height", &height, 32, 1024);
        if (widthChanged || heightChanged) {
            mip.width = static_cast<uint32_t>(width);
            mip.height = static_cast<uint32_t>(height);
            mip.mipLevels = image_lab::calculate_theoretical_mip_count(mip.width, mip.height);
        }

        ImGui::Text("Theoretical Mip Count: %u", image_lab::calculate_theoretical_mip_count(mip.width, mip.height));
        if (ImGui::Button("Create Mip Texture")) {
            mip.createRequested = true;
        }

        const float maxLod = static_cast<float>(std::max(1u, image_lab::calculate_theoretical_mip_count(mip.width, mip.height)) - 1u);
        ImGui::SliderFloat("LOD", &mip.lod, 0.0f, maxLod);
        ImGui::TextWrapped("%s", mip.status.empty() ? "Create a mip texture and use the LOD slider to inspect it." : mip.status.c_str());
    }

    void draw_array3d_preview()
    {
        auto& preview = m_state->array3d;

        if (ImGui::Button("Array Mode")) {
            preview.type = luna::ImageType::Image2DArray;
            preview.previewMode = image_lab::ArrayPreviewMode::Array;
        }
        ImGui::SameLine();
        if (ImGui::Button("3D Mode")) {
            preview.type = luna::ImageType::Image3D;
            preview.previewMode = image_lab::ArrayPreviewMode::Volume;
        }

        image_type_combo("Resource Type", &preview.type);

        int width = static_cast<int>(preview.width);
        int height = static_cast<int>(preview.height);
        int depth = static_cast<int>(preview.depth);
        int mipLevels = static_cast<int>(preview.mipLevels);
        int arrayLayers = static_cast<int>(preview.arrayLayers);

        ImGui::SliderInt("Width", &width, 16, 256);
        ImGui::SliderInt("Height", &height, 16, 256);

        const bool is3D = preview.type == luna::ImageType::Image3D;
        const bool isArray = preview.type == luna::ImageType::Image2DArray;

        ImGui::BeginDisabled(!is3D);
        ImGui::SliderInt("Depth", &depth, 1, 64);
        ImGui::EndDisabled();

        ImGui::SliderInt("Mip Levels", &mipLevels, 1, static_cast<int>(image_lab::calculate_theoretical_mip_count(
                                                                    static_cast<uint32_t>(width),
                                                                    static_cast<uint32_t>(height),
                                                                    is3D ? static_cast<uint32_t>(depth) : 1u)));

        ImGui::BeginDisabled(!isArray);
        ImGui::SliderInt("Array Layers", &arrayLayers, 1, 8);
        ImGui::EndDisabled();

        preview.width = static_cast<uint32_t>(width);
        preview.height = static_cast<uint32_t>(height);
        preview.depth = static_cast<uint32_t>(std::max(depth, 1));
        preview.mipLevels = static_cast<uint32_t>(std::max(mipLevels, 1));
        preview.arrayLayers = static_cast<uint32_t>(std::max(arrayLayers, 1));

        ImGui::TextWrapped("Current Desc: type=%s, size=%ux%u%s, mipLevels=%u, arrayLayers=%u",
                           luna::to_string(preview.type).data(),
                           preview.width,
                           preview.height,
                           preview.type == luna::ImageType::Image3D
                               ? ("x" + std::to_string(preview.depth)).c_str()
                               : "",
                           preview.mipLevels,
                           preview.type == luna::ImageType::Image2DArray ? preview.arrayLayers : 1u);

        if (ImGui::Button("Create Image")) {
            preview.createRequested = true;
        }

        if (preview.type == luna::ImageType::Image2DArray) {
            ImGui::SliderInt("Layer", &preview.layer, 0, std::max(0, static_cast<int>(preview.arrayLayers) - 1));
        } else if (preview.type == luna::ImageType::Image3D) {
            ImGui::SliderFloat("Slice", &preview.slice, 0.0f, 1.0f);
        }

        ImGui::TextWrapped("%s", preview.status.empty() ? "Use Create Image to validate 2D / 2D Array / 3D creation."
                                                        : preview.status.c_str());
    }

private:
    std::shared_ptr<image_lab::State> m_state;
};

class RhiImageLabSelfTestLayer final : public luna::Layer {
public:
    RhiImageLabSelfTestLayer(std::shared_ptr<image_lab::State> state, std::shared_ptr<SelfTestResult> result)
        : luna::Layer("RhiImageLabSelfTestLayer"),
          m_state(std::move(state)),
          m_result(std::move(result))
    {}

    void onAttach() override
    {
        LUNA_CORE_INFO("RhiImageLab self-test begin: phase5_smoke");
    }

    void onUpdate(luna::Timestep) override
    {
        if (m_state == nullptr || m_result == nullptr) {
            return;
        }

        ++m_frame;
        if (m_frame == 2) {
            m_state->page = image_lab::Page::FormatProbe;
            m_state->formatProbe.probeRequested = true;
        } else if (m_frame == 12) {
            m_state->page = image_lab::Page::MRTPreview;
            m_state->mrt.rebuildRequested = true;
        } else if (m_frame == 24) {
            m_state->page = image_lab::Page::MipPreview;
            m_state->mip.createRequested = true;
        } else if (m_frame == 36) {
            m_state->page = image_lab::Page::Array3DPreview;
            m_state->array3d.createRequested = true;
        }

        const bool passed = !m_state->formatProbe.details.empty() && !m_state->mrt.status.empty() &&
                            !m_state->mip.status.empty() && !m_state->array3d.status.empty() &&
                            !m_state->attachmentOps.status.empty();
        if (passed || m_frame >= 90) {
            m_result->passed = passed;
            if (passed) {
                LUNA_CORE_INFO("RhiImageLab self-test PASS format='{}' mrt='{}' mip='{}' array='{}'",
                               m_state->formatProbe.details,
                               m_state->mrt.status,
                               m_state->mip.status,
                               m_state->array3d.status);
            } else {
                LUNA_CORE_ERROR("RhiImageLab self-test FAIL format='{}' mrt='{}' mip='{}' array='{}' attachment='{}'",
                                m_state->formatProbe.details,
                                m_state->mrt.status,
                                m_state->mip.status,
                                m_state->array3d.status,
                                m_state->attachmentOps.status);
            }
            luna::Application::get().close();
        }
    }

private:
    std::shared_ptr<image_lab::State> m_state;
    std::shared_ptr<SelfTestResult> m_result;
    uint32_t m_frame = 0;
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

    std::shared_ptr<image_lab::State> state = std::make_shared<image_lab::State>();
    std::shared_ptr<SelfTestResult> selfTestResult = std::make_shared<SelfTestResult>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline = std::make_shared<image_lab::RhiImageLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiImageLab",
        .windowWidth = 1440,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = !options.runSelfTest,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "RhiImageLab",
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
        app->pushLayer(std::make_unique<RhiImageLabSelfTestLayer>(state, selfTestResult));
    } else {
        app->pushLayer(std::make_unique<RhiImageLabLayer>(state));
    }
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return options.runSelfTest && !selfTestResult->passed ? 1 : 0;
}
