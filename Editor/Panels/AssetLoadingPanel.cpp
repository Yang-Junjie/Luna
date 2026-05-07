#include "Asset/AssetManager.h"
#include "Asset/AssetTypes.h"
#include "AssetLoadingPanel.h"
#include "EditorUI.h"

#include <algorithm>
#include <imgui.h>

namespace luna {

void AssetLoadingPanel::onImGuiRender()
{
    ImGui::SetNextWindowSize(editor::ui::scaled(420.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Asset Loading")) {
        ImGui::End();
        return;
    }

    const std::vector<AssetManager::LoadingAssetInfo> loading_assets = AssetManager::get().getLoadingAssetsSnapshot();

    ImGui::Text("Loading Assets: %zu", loading_assets.size());
    ImGui::Separator();

    if (loading_assets.empty()) {
        ImGui::TextUnformatted("No assets are loading.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("##AssetLoadingTable",
                          4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(90.0f));
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableHeadersRow();

        for (const AssetManager::LoadingAssetInfo& info : loading_assets) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(AssetUtils::AssetTypeToString(info.Type));

            ImGui::TableNextColumn();
            const char* name = info.Name.empty() ? "Unnamed Asset" : info.Name.c_str();
            ImGui::TextUnformatted(name);

            ImGui::TableNextColumn();
            const std::string display_path =
                info.FilePath.empty() ? std::string("Unknown Path") : info.FilePath.generic_string();
            ImGui::TextUnformatted(display_path.c_str());

            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", info.Handle.toString().c_str());
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace luna
