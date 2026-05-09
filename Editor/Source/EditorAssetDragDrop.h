#pragma once

#include "Asset/AssetMetadata.h"

#include <algorithm>
#include <initializer_list>

#include <imgui.h>

namespace luna::editor {

constexpr const char* kAssetDragDropPayload = "LUNA_ASSET";

struct AssetDragDropData {
    uint64_t Handle = 0;
    uint32_t Type = static_cast<uint32_t>(AssetType::None);
};

inline AssetType getAssetType(const AssetDragDropData& payload)
{
    return static_cast<AssetType>(payload.Type);
}

inline AssetHandle getAssetHandle(const AssetDragDropData& payload)
{
    return AssetHandle(payload.Handle);
}

inline bool acceptsType(AssetType type, std::initializer_list<AssetType> accepted_types)
{
    return accepted_types.size() == 0 ||
           std::find(accepted_types.begin(), accepted_types.end(), type) != accepted_types.end();
}

inline bool beginAssetDragDropSource(const AssetMetadata& metadata, const char* label = nullptr)
{
    if (!metadata.Handle.isValid() || metadata.Type == AssetType::None) {
        return false;
    }

    if (!ImGui::BeginDragDropSource()) {
        return false;
    }

    const AssetDragDropData payload{
        static_cast<uint64_t>(metadata.Handle),
        static_cast<uint32_t>(metadata.Type),
    };
    ImGui::SetDragDropPayload(kAssetDragDropPayload, &payload, sizeof(payload));

    const char* display_label = label;
    if (display_label == nullptr || display_label[0] == '\0') {
        display_label = metadata.Name.empty() ? "Unnamed Asset" : metadata.Name.c_str();
    }

    ImGui::TextUnformatted(display_label);
    ImGui::TextDisabled("%s", AssetUtils::AssetTypeToString(metadata.Type));
    ImGui::EndDragDropSource();
    return true;
}

inline bool acceptAssetDragDropPayload(AssetDragDropData& out_payload,
                                       std::initializer_list<AssetType> accepted_types = {})
{
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragDropPayload);
    if (payload == nullptr || payload->Data == nullptr || payload->DataSize != sizeof(AssetDragDropData)) {
        return false;
    }

    out_payload = *static_cast<const AssetDragDropData*>(payload->Data);
    return getAssetHandle(out_payload).isValid() && acceptsType(getAssetType(out_payload), accepted_types);
}

} // namespace luna::editor
