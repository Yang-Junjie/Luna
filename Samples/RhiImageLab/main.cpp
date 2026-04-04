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

const char* page_label(image_lab::Page page)
{
    switch (page) {
        case image_lab::Page::FormatProbe:
            return "Format Probe";
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
        draw_page_button(image_lab::Page::MRTPreview);
        ImGui::SameLine();
        draw_page_button(image_lab::Page::MipPreview);
        ImGui::SameLine();
        draw_page_button(image_lab::Page::Array3DPreview);

        ImGui::Separator();
        switch (m_state->page) {
            case image_lab::Page::FormatProbe:
                draw_format_probe();
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

        if (ImGui::Button(page_label(page), ImVec2(112.0f, 0.0f))) {
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

} // namespace

int main()
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    std::shared_ptr<image_lab::State> state = std::make_shared<image_lab::State>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline = std::make_shared<image_lab::RhiImageLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiImageLab",
        .windowWidth = 1440,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = true,
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

    app->pushLayer(std::make_unique<RhiImageLabLayer>(state));
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return 0;
}
