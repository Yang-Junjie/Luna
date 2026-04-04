#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"
#include "ImageViewLabPipeline.h"
#include "ImageViewLabState.h"

#include <imgui.h>

#include <algorithm>
#include <memory>
#include <string>

namespace {

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

        const int selectedView = std::clamp(mip.selectedView, 0, std::max(0, static_cast<int>(mip.views.size()) - 1));
        if (!mip.views.empty()) {
            const auto& view = mip.views[static_cast<size_t>(selectedView)];
            ImGui::SliderFloat("View Local LOD", &mip.previewLod, 0.0f, static_cast<float>(std::max(1u, view.desc.mipCount) - 1u));
            ImGui::Text("Selected View Range: mip %u .. %u",
                        view.desc.baseMipLevel,
                        view.desc.baseMipLevel + view.desc.mipCount - 1);
        }

        draw_view_list(mip.views, &mip.selectedView, &mip.deleteView);
        ImGui::TextWrapped("%s", mip.status.empty() ? "Create single mip or mip range views from one source image."
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

} // namespace

int main()
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    std::shared_ptr<image_view_lab::State> state = std::make_shared<image_view_lab::State>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline =
        std::make_shared<image_view_lab::RhiImageViewLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiImageViewLab",
        .windowWidth = 1440,
        .windowHeight = 900,
        .maximized = false,
        .enableImGui = true,
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

    app->pushLayer(std::make_unique<RhiImageViewLabLayer>(state));
    app->run();
    app.reset();
    luna::Logger::shutdown();
    return 0;
}
