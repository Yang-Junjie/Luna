#include "ScriptComponentInspector.h"

#include "Asset/AssetDatabase.h"
#include "EditorAssetDragDrop.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Entity.h"

#include <imgui.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr const char* kEntityDragPayload = "LUNA_ENTITY";

const char* scriptPropertyTypeToString(luna::ScriptPropertyType type)
{
    switch (type) {
        case luna::ScriptPropertyType::Bool:
            return "Bool";
        case luna::ScriptPropertyType::Int:
            return "Int";
        case luna::ScriptPropertyType::Float:
            return "Float";
        case luna::ScriptPropertyType::String:
            return "String";
        case luna::ScriptPropertyType::Vec3:
            return "Vec3";
        case luna::ScriptPropertyType::Entity:
            return "Entity";
        case luna::ScriptPropertyType::Asset:
            return "Asset";
    }

    return "Float";
}

std::string trimString(std::string value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (size_t index = 0; index < lhs.size(); ++index) {
        const unsigned char left = static_cast<unsigned char>(lhs[index]);
        const unsigned char right = static_cast<unsigned char>(rhs[index]);
        if (std::tolower(left) != std::tolower(right)) {
            return false;
        }
    }

    return true;
}

void resetScriptPropertyValue(luna::ScriptProperty& property)
{
    property.boolValue = false;
    property.intValue = 0;
    property.floatValue = 0.0f;
    property.stringValue.clear();
    property.vec3Value = glm::vec3(0.0f);
    property.entityValue = luna::UUID(0);
    property.assetValue = luna::AssetHandle(0);
}

std::string makeUniquePropertyName(const luna::ScriptEntry& script,
                                   size_t property_index,
                                   std::string desired_name)
{
    desired_name = trimString(std::move(desired_name));
    if (desired_name.empty()) {
        desired_name = "Property";
    }

    auto is_name_available = [&](std::string_view candidate) {
        for (size_t other_index = 0; other_index < script.properties.size(); ++other_index) {
            if (other_index == property_index) {
                continue;
            }

            if (equalsIgnoreCase(script.properties[other_index].name, candidate)) {
                return false;
            }
        }

        return true;
    };

    if (is_name_available(desired_name)) {
        return desired_name;
    }

    const std::string base_name = desired_name;
    for (uint32_t suffix = 1; suffix < 10000; ++suffix) {
        std::string candidate = base_name + std::to_string(suffix);
        if (is_name_available(candidate)) {
            return candidate;
        }
    }

    return base_name;
}

bool normalizeScriptPropertyNames(luna::ScriptEntry& script)
{
    bool changed = false;
    for (size_t property_index = 0; property_index < script.properties.size(); ++property_index) {
        luna::ScriptProperty& property = script.properties[property_index];
        const std::string normalized_name = makeUniquePropertyName(script, property_index, property.name);
        if (property.name != normalized_name) {
            property.name = normalized_name;
            changed = true;
        }
    }

    return changed;
}

std::string getAssetDisplayLabel(luna::AssetHandle handle)
{
    if (!handle.isValid()) {
        return "None";
    }

    if (!luna::AssetDatabase::exists(handle)) {
        return "Unknown Asset";
    }

    const auto& metadata = luna::AssetDatabase::getAssetMetadata(handle);
    if (!metadata.Name.empty()) {
        return metadata.Name;
    }

    if (!metadata.FilePath.empty()) {
        return metadata.FilePath.generic_string();
    }

    return "Unnamed Asset";
}

bool drawAssetHandleEditor(const char* label,
                           luna::AssetHandle& handle,
                           std::initializer_list<luna::AssetType> accepted_types = {})
{
    bool changed = false;
    unsigned long long raw_handle = static_cast<unsigned long long>(static_cast<uint64_t>(handle));
    if (ImGui::InputScalar(label, ImGuiDataType_U64, &raw_handle)) {
        handle = luna::AssetHandle(static_cast<uint64_t>(raw_handle));
        changed = true;
    }

    if (ImGui::BeginDragDropTarget()) {
        luna::editor::AssetDragDropData payload{};
        if (luna::editor::acceptAssetDragDropPayload(payload, accepted_types)) {
            handle = luna::editor::getAssetHandle(payload);
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    return changed;
}

bool drawScriptAssetEditor(luna::ScriptEntry& script)
{
    bool changed = drawAssetHandleEditor("Script Asset", script.scriptAsset, {luna::AssetType::Script});

    if (!script.scriptAsset.isValid()) {
        ImGui::TextDisabled("Script Asset: None");
        return changed;
    }

    if (!luna::AssetDatabase::exists(script.scriptAsset)) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Script asset handle does not exist.");
        return changed;
    }

    const auto& metadata = luna::AssetDatabase::getAssetMetadata(script.scriptAsset);
    ImGui::TextDisabled("Script Asset: %s", getAssetDisplayLabel(script.scriptAsset).c_str());
    ImGui::TextDisabled("Script Type: %s", luna::AssetUtils::AssetTypeToString(metadata.Type));
    if (metadata.Type != luna::AssetType::Script) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Selected asset is not a Script asset.");
    }

    return changed;
}

bool drawEntityReferenceEditor(luna::Entity owner_entity, luna::UUID& entity_value)
{
    bool changed = false;

    unsigned long long raw_entity_id = static_cast<unsigned long long>(static_cast<uint64_t>(entity_value));
    if (ImGui::InputScalar("Entity", ImGuiDataType_U64, &raw_entity_id)) {
        entity_value = luna::UUID(static_cast<uint64_t>(raw_entity_id));
        changed = true;
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityDragPayload);
            payload != nullptr && payload->Data != nullptr && payload->DataSize == sizeof(uint64_t)) {
            entity_value = luna::UUID(*static_cast<const uint64_t*>(payload->Data));
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    if (owner_entity) {
        if (ImGui::Button("Use Self")) {
            entity_value = owner_entity.getUUID();
            changed = true;
        }
        ImGui::SameLine();
    }
    if (ImGui::Button("Clear")) {
        entity_value = luna::UUID(0);
        changed = true;
    }

    if (!entity_value.isValid()) {
        ImGui::TextDisabled("Resolved Entity: None");
        return changed;
    }

    if (!owner_entity || owner_entity.getEntityManager() == nullptr) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Unable to validate entity reference.");
        return changed;
    }

    const luna::Entity target_entity = owner_entity.getEntityManager()->findEntityByUUID(entity_value);
    if (!target_entity) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Referenced entity does not exist in this scene.");
        return changed;
    }

    ImGui::TextDisabled("Resolved Entity: %s", target_entity.getName().c_str());
    return changed;
}

bool drawScriptPropertyValueEditor(luna::Entity owner_entity, luna::ScriptProperty& property)
{
    bool changed = false;

    switch (property.type) {
        case luna::ScriptPropertyType::Bool:
            changed |= ImGui::Checkbox("Value", &property.boolValue);
            break;
        case luna::ScriptPropertyType::Int:
            changed |= ImGui::InputInt("Value", &property.intValue);
            break;
        case luna::ScriptPropertyType::Float:
            changed |= ImGui::DragFloat("Value", &property.floatValue, 0.05f);
            break;
        case luna::ScriptPropertyType::String: {
            char string_buffer[512] = {};
            strncpy_s(string_buffer, property.stringValue.c_str(), _TRUNCATE);
            if (ImGui::InputTextMultiline("Value",
                                          string_buffer,
                                          sizeof(string_buffer),
                                          ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4.0f))) {
                property.stringValue = string_buffer;
                changed = true;
            }
            break;
        }
        case luna::ScriptPropertyType::Vec3:
            changed |= ImGui::DragFloat3("Value", &property.vec3Value.x, 0.05f);
            break;
        case luna::ScriptPropertyType::Entity:
            changed |= drawEntityReferenceEditor(owner_entity, property.entityValue);
            break;
        case luna::ScriptPropertyType::Asset:
            changed |= drawAssetHandleEditor("Asset", property.assetValue);
            if (!property.assetValue.isValid()) {
                ImGui::TextDisabled("Resolved Asset: None");
            } else if (!luna::AssetDatabase::exists(property.assetValue)) {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Referenced asset does not exist.");
            } else {
                const auto& metadata = luna::AssetDatabase::getAssetMetadata(property.assetValue);
                ImGui::TextDisabled("Resolved Asset: %s", getAssetDisplayLabel(property.assetValue).c_str());
                ImGui::TextDisabled("Asset Type: %s", luna::AssetUtils::AssetTypeToString(metadata.Type));
            }
            break;
    }

    return changed;
}

bool drawScriptPropertyEditor(luna::Entity owner_entity,
                              luna::ScriptEntry& script,
                              size_t property_index,
                              bool& removed_property)
{
    auto& property = script.properties[property_index];
    bool changed = false;

    std::string property_label = property.name.empty() ? "<unnamed>" : property.name;
    property_label += " (";
    property_label += scriptPropertyTypeToString(property.type);
    property_label += ")";

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
    const bool open = ImGui::TreeNodeEx("##Property", flags, "%s", property_label.c_str());

    bool remove_requested = false;
    if (ImGui::BeginPopupContextItem("PropertyContext")) {
        if (ImGui::MenuItem("Remove Property")) {
            remove_requested = true;
        }
        ImGui::EndPopup();
    }

    if (open) {
        char name_buffer[256] = {};
        strncpy_s(name_buffer, property.name.c_str(), _TRUNCATE);
        if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
            property.name = name_buffer;
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            const std::string normalized_name = makeUniquePropertyName(script, property_index, property.name);
            if (property.name != normalized_name) {
                property.name = normalized_name;
                changed = true;
            }
        }

        const char* current_type_label = scriptPropertyTypeToString(property.type);
        if (ImGui::BeginCombo("Type", current_type_label)) {
            constexpr luna::ScriptPropertyType kPropertyTypes[] = {
                luna::ScriptPropertyType::Bool,
                luna::ScriptPropertyType::Int,
                luna::ScriptPropertyType::Float,
                luna::ScriptPropertyType::String,
                luna::ScriptPropertyType::Vec3,
                luna::ScriptPropertyType::Entity,
                luna::ScriptPropertyType::Asset,
            };

            for (const luna::ScriptPropertyType type : kPropertyTypes) {
                const bool selected = property.type == type;
                if (ImGui::Selectable(scriptPropertyTypeToString(type), selected)) {
                    if (property.type != type) {
                        property.type = type;
                        resetScriptPropertyValue(property);
                        changed = true;
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        changed |= drawScriptPropertyValueEditor(owner_entity, property);

        if (ImGui::Button("Remove Property", ImVec2(-1.0f, 0.0f))) {
            remove_requested = true;
        }

        ImGui::TreePop();
    }

    if (remove_requested) {
        script.properties.erase(script.properties.begin() + static_cast<std::ptrdiff_t>(property_index));
        removed_property = true;
        changed = true;
    }

    return changed;
}

} // namespace

namespace luna {

bool drawScriptComponentInspector(Entity owner_entity, ScriptComponent& script_component)
{
    bool changed = false;

    changed |= ImGui::Checkbox("Enabled", &script_component.enabled);
    ImGui::Text("Scripts: %zu", script_component.scripts.size());

    if (ImGui::Button("Add Script", ImVec2(-1.0f, 0.0f))) {
        ScriptEntry entry{};
        entry.id = UUID{};
        script_component.scripts.push_back(std::move(entry));
        changed = true;
    }

    for (size_t script_index = 0; script_index < script_component.scripts.size(); ++script_index) {
        auto& script = script_component.scripts[script_index];
        ImGui::PushID(static_cast<int>(script_index));
        ImGui::Separator();
        changed |= ImGui::Checkbox("Script Enabled", &script.enabled);

        char type_buffer[256] = {};
        strncpy_s(type_buffer, script.typeName.c_str(), _TRUNCATE);
        if (ImGui::InputText("Type Name", type_buffer, sizeof(type_buffer))) {
            script.typeName = type_buffer;
            changed = true;
        }

        changed |= drawScriptAssetEditor(script);
        changed |= ImGui::InputInt("Execution Order", &script.executionOrder);

        ImGui::SeparatorText("Properties");
        changed |= normalizeScriptPropertyNames(script);
        ImGui::TextDisabled("Property Count: %zu", script.properties.size());
        if (ImGui::Button("Add Property", ImVec2(-1.0f, 0.0f))) {
            ScriptProperty property{};
            property.name = makeUniquePropertyName(script, script.properties.size(), "Property");
            script.properties.push_back(std::move(property));
            changed = true;
        }

        if (script.properties.empty()) {
            ImGui::TextDisabled("No properties.");
        } else {
            for (size_t property_index = 0; property_index < script.properties.size(); ++property_index) {
                ImGui::PushID(static_cast<int>(property_index));
                bool removed_property = false;
                changed |= drawScriptPropertyEditor(owner_entity, script, property_index, removed_property);
                ImGui::PopID();

                if (removed_property) {
                    break;
                }
            }
        }

        if (ImGui::Button("Remove Script", ImVec2(-1.0f, 0.0f))) {
            script_component.scripts.erase(script_component.scripts.begin() + static_cast<std::ptrdiff_t>(script_index));
            changed = true;
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    return changed;
}

} // namespace luna
