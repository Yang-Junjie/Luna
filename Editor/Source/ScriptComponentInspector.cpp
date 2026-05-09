#include "ScriptComponentInspector.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "EditorUI.h"
#include "Project/ProjectManager.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshComponent.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Entity.h"
#include "Script/ScriptAsset.h"
#include "Script/ScriptPluginManager.h"
#include "Script/ScriptPropertySchema.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

void clearScriptPropertyMetadata(luna::ScriptProperty& property)
{
    property.metadata = {};
}

bool sameScriptPropertyOption(const luna::ScriptPropertyOption& lhs, const luna::ScriptPropertyOption& rhs)
{
    return lhs.label == rhs.label &&
           lhs.intValue == rhs.intValue &&
           lhs.stringValue == rhs.stringValue;
}

bool sameScriptPropertyMetadata(const luna::ScriptPropertyMetadata& lhs, const luna::ScriptPropertyMetadata& rhs)
{
    return lhs.displayName == rhs.displayName &&
           lhs.description == rhs.description &&
           lhs.category == rhs.category &&
           lhs.hasMinValue == rhs.hasMinValue &&
           lhs.hasMaxValue == rhs.hasMaxValue &&
           lhs.hasStepValue == rhs.hasStepValue &&
           lhs.minValue == rhs.minValue &&
           lhs.maxValue == rhs.maxValue &&
           lhs.stepValue == rhs.stepValue &&
           lhs.assetType == rhs.assetType &&
           lhs.entityFilter == rhs.entityFilter &&
           lhs.options.size() == rhs.options.size() &&
           std::equal(lhs.options.begin(), lhs.options.end(), rhs.options.begin(), sameScriptPropertyOption);
}

bool hasScriptPropertyMetadata(const luna::ScriptPropertyMetadata& metadata)
{
    return !metadata.displayName.empty() ||
           !metadata.description.empty() ||
           !metadata.category.empty() ||
           metadata.hasMinValue ||
           metadata.hasMaxValue ||
           metadata.hasStepValue ||
           !metadata.assetType.empty() ||
           !metadata.entityFilter.empty() ||
           !metadata.options.empty();
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

void copyScriptPropertyValue(luna::ScriptProperty& destination, const luna::ScriptProperty& source)
{
    destination.boolValue = source.boolValue;
    destination.intValue = source.intValue;
    destination.floatValue = source.floatValue;
    destination.stringValue = source.stringValue;
    destination.vec3Value = source.vec3Value;
    destination.entityValue = source.entityValue;
    destination.assetValue = source.assetValue;
}

struct ProjectScriptLanguageState {
    bool available = false;
    std::string language;
    std::string statusMessage;
};

ProjectScriptLanguageState getProjectScriptLanguageState()
{
    ProjectScriptLanguageState state{};

    const auto project_info = luna::ProjectManager::instance().getProjectInfo();
    const luna::ScriptPluginSelectionResult selection =
        luna::ScriptPluginManager::instance().resolveProjectSelection(project_info ? &*project_info : nullptr);
    if (!selection.isResolved() || selection.Candidate == nullptr) {
        state.statusMessage = selection.StatusMessage.empty() ? "No usable script plugin is selected for this project."
                                                              : selection.StatusMessage;
        return state;
    }

    state.language = selection.Candidate->Manifest.Language;
    state.available = !state.language.empty();
    if (!state.available) {
        state.statusMessage = "Selected script plugin does not declare a script language.";
    }
    return state;
}

std::string getScriptAssetLanguage(const luna::AssetMetadata& metadata)
{
    return metadata.GetConfig<std::string>("Language", "");
}

bool isScriptAssetLanguageAccepted(const luna::AssetMetadata& metadata,
                                   const ProjectScriptLanguageState& language_state,
                                   std::string* rejection_message = nullptr)
{
    if (metadata.Type != luna::AssetType::Script) {
        if (rejection_message != nullptr) {
            *rejection_message = "Selected asset is not a Script asset.";
        }
        return false;
    }

    if (!language_state.available) {
        if (rejection_message != nullptr) {
            *rejection_message = language_state.statusMessage.empty()
                                     ? "No usable script plugin is selected for this project."
                                     : language_state.statusMessage;
        }
        return false;
    }

    const std::string metadata_language = getScriptAssetLanguage(metadata);
    if (metadata_language.empty()) {
        if (rejection_message != nullptr) {
            *rejection_message = "Script asset metadata does not declare a script language.";
        }
        return false;
    }

    if (!equalsIgnoreCase(metadata_language, language_state.language)) {
        if (rejection_message != nullptr) {
            *rejection_message = "Script asset language '" + metadata_language +
                                 "' does not match selected project script language '" + language_state.language + "'.";
        }
        return false;
    }

    return true;
}

const luna::ScriptProperty* findMatchingProperty(const luna::ScriptEntry& script,
                                                 const std::string& name,
                                                 luna::ScriptPropertyType type)
{
    for (const luna::ScriptProperty& property : script.properties) {
        if (property.type == type && equalsIgnoreCase(property.name, name)) {
            return &property;
        }
    }

    return nullptr;
}

bool applyScriptPropertySchema(luna::ScriptEntry& script, const std::vector<luna::ScriptPropertySchema>& schemas)
{
    std::vector<luna::ScriptProperty> synced_properties;
    synced_properties.reserve(schemas.size());

    for (const luna::ScriptPropertySchema& schema : schemas) {
        if (schema.name.empty()) {
            continue;
        }

        luna::ScriptProperty property = schema.defaultValue;
        property.name = schema.name;
        property.type = schema.type;
        property.metadata = schema.metadata;

        if (const luna::ScriptProperty* existing = findMatchingProperty(script, schema.name, schema.type)) {
            copyScriptPropertyValue(property, *existing);
        }

        synced_properties.push_back(std::move(property));
    }

    const bool changed = script.properties.size() != synced_properties.size() ||
                         !std::equal(script.properties.begin(),
                                     script.properties.end(),
                                     synced_properties.begin(),
                                     synced_properties.end(),
                                     [](const luna::ScriptProperty& lhs, const luna::ScriptProperty& rhs) {
                                         return equalsIgnoreCase(lhs.name, rhs.name) && lhs.type == rhs.type &&
                                                lhs.boolValue == rhs.boolValue && lhs.intValue == rhs.intValue &&
                                                lhs.floatValue == rhs.floatValue && lhs.stringValue == rhs.stringValue &&
                                                lhs.vec3Value == rhs.vec3Value && lhs.entityValue == rhs.entityValue &&
                                                lhs.assetValue == rhs.assetValue &&
                                                sameScriptPropertyMetadata(lhs.metadata, rhs.metadata);
                                     });

    if (changed) {
        script.properties = std::move(synced_properties);
    }

    return changed;
}

bool drawScriptAssetEditor(luna::ScriptEntry& script)
{
    const ProjectScriptLanguageState language_state = getProjectScriptLanguageState();
    std::optional<std::string> rejected_selection_message;

    auto accepts_script_handle = [&](luna::AssetHandle handle) {
        if (!handle.isValid()) {
            rejected_selection_message.reset();
            return true;
        }

        if (!luna::AssetDatabase::exists(handle)) {
            rejected_selection_message = "Script asset does not exist.";
            return false;
        }

        std::string rejection_message;
        if (!isScriptAssetLanguageAccepted(
                luna::AssetDatabase::getAssetMetadata(handle), language_state, &rejection_message)) {
            rejected_selection_message = std::move(rejection_message);
            return false;
        }

        rejected_selection_message.reset();
        return true;
    };

    bool changed = luna::editor::ui::drawAssetHandleSelector(
        "Script Asset", script.scriptAsset, {luna::AssetType::Script}, accepts_script_handle);
    if (rejected_selection_message.has_value()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", rejected_selection_message->c_str());
    }

    if (!script.scriptAsset.isValid()) {
        ImGui::TextDisabled("Script Asset: None");
        if (!language_state.available && !language_state.statusMessage.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", language_state.statusMessage.c_str());
        }
        return changed;
    }

    if (!luna::AssetDatabase::exists(script.scriptAsset)) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Script asset does not exist.");
        return changed;
    }

    const auto& metadata = luna::AssetDatabase::getAssetMetadata(script.scriptAsset);
    ImGui::TextDisabled("Script Asset: %s", luna::editor::ui::assetDisplayLabel(script.scriptAsset).c_str());
    ImGui::TextDisabled("Script Type: %s", luna::AssetUtils::AssetTypeToString(metadata.Type));
    if (metadata.Type == luna::AssetType::Script) {
        const std::string metadata_language = getScriptAssetLanguage(metadata);
        ImGui::TextDisabled("Script Language: %s", metadata_language.empty() ? "<none>" : metadata_language.c_str());
    }
    if (metadata.Type != luna::AssetType::Script) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Selected asset is not a Script asset.");
        return changed;
    }

    std::string rejection_message;
    if (!isScriptAssetLanguageAccepted(metadata, language_state, &rejection_message)) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", rejection_message.c_str());
    }

    return changed;
}

std::vector<luna::ScriptPropertySchema> loadScriptPropertySchema(const luna::ScriptEntry& script)
{
    if (!script.scriptAsset.isValid() || !luna::AssetDatabase::exists(script.scriptAsset)) {
        return {};
    }

    const luna::AssetMetadata& metadata = luna::AssetDatabase::getAssetMetadata(script.scriptAsset);
    if (metadata.Type != luna::AssetType::Script) {
        return {};
    }

    const ProjectScriptLanguageState language_state = getProjectScriptLanguageState();
    std::string rejection_message;
    if (!isScriptAssetLanguageAccepted(metadata, language_state, &rejection_message)) {
        return {};
    }

    const std::shared_ptr<luna::ScriptAsset> script_asset =
        luna::AssetManager::get().loadAssetAs<luna::ScriptAsset>(script.scriptAsset);
    if (!script_asset) {
        return {};
    }

    luna::ScriptSchemaRequest request{};
    request.assetName = !metadata.Name.empty() ? metadata.Name : metadata.FilePath.filename().string();
    request.typeName = script.typeName;
    request.language = script_asset->language;
    request.source = script_asset->source;

    const auto& project_info = luna::ProjectManager::instance().getProjectInfo();
    return luna::ScriptPluginManager::instance().getPropertySchemaForProject(
        project_info ? &*project_info : nullptr, request);
}

bool drawSchemaSyncControls(luna::ScriptEntry& script)
{
    bool changed = false;
    bool open_no_schema_popup = false;
    if (ImGui::BeginPopupContextItem("ScriptPropertiesContext")) {
        if (ImGui::MenuItem("Sync Properties From Script")) {
            const std::vector<luna::ScriptPropertySchema> schemas = loadScriptPropertySchema(script);
            if (schemas.empty()) {
                open_no_schema_popup = true;
            } else {
                changed |= applyScriptPropertySchema(script, schemas);
            }
        }
        ImGui::EndPopup();
    }

    if (open_no_schema_popup) {
        ImGui::OpenPopup("NoScriptSchemaPopup");
    }
    if (ImGui::BeginPopup("NoScriptSchemaPopup")) {
        ImGui::TextUnformatted("The selected script did not expose a Properties schema.");
        ImGui::EndPopup();
    }

    return changed;
}

luna::AssetType parseAssetTypeFilter(std::string_view asset_type)
{
    if (asset_type.empty() || equalsIgnoreCase(asset_type, "Any") || equalsIgnoreCase(asset_type, "None")) {
        return luna::AssetType::None;
    }

    constexpr luna::AssetType kAssetTypes[] = {
        luna::AssetType::Texture,
        luna::AssetType::Mesh,
        luna::AssetType::Material,
        luna::AssetType::Model,
        luna::AssetType::Scene,
        luna::AssetType::Script,
    };

    for (const luna::AssetType type : kAssetTypes) {
        if (equalsIgnoreCase(asset_type, luna::AssetUtils::AssetTypeToString(type))) {
            return type;
        }
    }

    return luna::AssetType::None;
}

bool entityMatchesFilter(luna::Entity entity, std::string_view entity_filter)
{
    if (entity_filter.empty() || equalsIgnoreCase(entity_filter, "Any")) {
        return true;
    }
    if (!entity) {
        return false;
    }

    if (equalsIgnoreCase(entity_filter, "Camera") || equalsIgnoreCase(entity_filter, "CameraComponent")) {
        return entity.hasComponent<luna::CameraComponent>();
    }
    if (equalsIgnoreCase(entity_filter, "Light") || equalsIgnoreCase(entity_filter, "LightComponent")) {
        return entity.hasComponent<luna::LightComponent>();
    }
    if (equalsIgnoreCase(entity_filter, "Mesh") || equalsIgnoreCase(entity_filter, "MeshComponent")) {
        return entity.hasComponent<luna::MeshComponent>();
    }
    if (equalsIgnoreCase(entity_filter, "Script") || equalsIgnoreCase(entity_filter, "ScriptComponent")) {
        return entity.hasComponent<luna::ScriptComponent>();
    }

    return true;
}

float clampedNumericValue(float value, const luna::ScriptPropertyMetadata& metadata)
{
    if (metadata.hasMinValue) {
        value = (std::max)(value, metadata.minValue);
    }
    if (metadata.hasMaxValue) {
        value = (std::min)(value, metadata.maxValue);
    }
    return value;
}

int clampedNumericValue(int value, const luna::ScriptPropertyMetadata& metadata)
{
    if (metadata.hasMinValue) {
        value = (std::max)(value, static_cast<int>(std::ceil(metadata.minValue)));
    }
    if (metadata.hasMaxValue) {
        value = (std::min)(value, static_cast<int>(std::floor(metadata.maxValue)));
    }
    return value;
}

int intStepFromMetadata(const luna::ScriptPropertyMetadata& metadata)
{
    if (!metadata.hasStepValue || metadata.stepValue <= 0.0f) {
        return 1;
    }

    return (std::max)(1, static_cast<int>(std::round(metadata.stepValue)));
}

float floatStepFromMetadata(const luna::ScriptPropertyMetadata& metadata, float fallback)
{
    return metadata.hasStepValue && metadata.stepValue > 0.0f ? metadata.stepValue : fallback;
}

bool drawIntOptionEditor(luna::ScriptProperty& property)
{
    const luna::ScriptPropertyOption* selected_option = nullptr;
    for (const luna::ScriptPropertyOption& option : property.metadata.options) {
        if (option.intValue == property.intValue) {
            selected_option = &option;
            break;
        }
    }

    const std::string fallback = std::to_string(property.intValue);
    const char* preview = selected_option != nullptr && !selected_option->label.empty()
                              ? selected_option->label.c_str()
                              : fallback.c_str();

    return luna::editor::ui::drawCombo("Value", preview, [&]() {
        bool changed = false;
        for (const luna::ScriptPropertyOption& option : property.metadata.options) {
            const std::string label = option.label.empty() ? std::to_string(option.intValue) : option.label;
            const bool selected = property.intValue == option.intValue;
            if (ImGui::Selectable(label.c_str(), selected)) {
                if (property.intValue != option.intValue) {
                    property.intValue = option.intValue;
                    changed = true;
                }
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        return changed;
    });
}

bool drawStringOptionEditor(luna::ScriptProperty& property)
{
    const luna::ScriptPropertyOption* selected_option = nullptr;
    for (const luna::ScriptPropertyOption& option : property.metadata.options) {
        if (option.stringValue == property.stringValue) {
            selected_option = &option;
            break;
        }
    }

    const char* preview = selected_option != nullptr && !selected_option->label.empty()
                              ? selected_option->label.c_str()
                              : property.stringValue.c_str();

    return luna::editor::ui::drawCombo("Value", preview, [&]() {
        bool changed = false;
        for (const luna::ScriptPropertyOption& option : property.metadata.options) {
            const std::string label = option.label.empty() ? option.stringValue : option.label;
            const bool selected = property.stringValue == option.stringValue;
            if (ImGui::Selectable(label.c_str(), selected)) {
                if (property.stringValue != option.stringValue) {
                    property.stringValue = option.stringValue;
                    changed = true;
                }
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        return changed;
    });
}

bool drawEntityReferenceEditor(luna::Entity owner_entity, luna::UUID& entity_value, std::string_view entity_filter)
{
    bool changed = false;

    unsigned long long raw_entity_id = static_cast<unsigned long long>(static_cast<uint64_t>(entity_value));
    bool use_self_requested = false;
    bool clear_requested = false;
    if (luna::editor::ui::beginPropertyRow("Entity")) {
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputScalar("##value", ImGuiDataType_U64, &raw_entity_id)) {
            entity_value = luna::UUID(static_cast<uint64_t>(raw_entity_id));
            changed = true;
        }
        if (ImGui::BeginPopupContextItem("EntityReferenceContext")) {
            const bool can_use_self =
                static_cast<bool>(owner_entity) && entityMatchesFilter(owner_entity, entity_filter);
            if (ImGui::MenuItem("Use Self", nullptr, false, can_use_self)) {
                use_self_requested = true;
            }
            if (ImGui::MenuItem("Clear", nullptr, false, entity_value.isValid())) {
                clear_requested = true;
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityDragPayload);
                payload != nullptr && payload->Data != nullptr && payload->DataSize == sizeof(uint64_t)) {
                const luna::UUID candidate_id{*static_cast<const uint64_t*>(payload->Data)};
                if (owner_entity && owner_entity.getEntityManager() != nullptr) {
                    const luna::Entity candidate = owner_entity.getEntityManager()->findEntityByUUID(candidate_id);
                    if (entityMatchesFilter(candidate, entity_filter)) {
                        entity_value = candidate_id;
                        changed = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        luna::editor::ui::endPropertyRow();
    }

    if (use_self_requested && owner_entity) {
        entity_value = owner_entity.getUUID();
        changed = true;
    }
    if (clear_requested) {
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
    if (!entityMatchesFilter(target_entity, entity_filter)) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                           "Referenced entity does not match required filter '%s'.",
                           std::string(entity_filter).c_str());
        return changed;
    }

    ImGui::TextDisabled("Resolved Entity: %s", target_entity.getName().c_str());
    return changed;
}

bool drawScriptPropertyValueEditor(luna::Entity owner_entity, luna::ScriptProperty& property)
{
    bool changed = false;
    const luna::ScriptPropertyMetadata& metadata = property.metadata;

    switch (property.type) {
        case luna::ScriptPropertyType::Bool:
            changed |= luna::editor::ui::drawBool("Value", property.boolValue);
            break;
        case luna::ScriptPropertyType::Int:
            if (!metadata.options.empty()) {
                changed |= drawIntOptionEditor(property);
            } else {
                changed |= luna::editor::ui::drawInt("Value", property.intValue, intStepFromMetadata(metadata));
                if (changed) {
                    property.intValue = clampedNumericValue(property.intValue, metadata);
                }
            }
            break;
        case luna::ScriptPropertyType::Float:
            changed |= luna::editor::ui::drawFloat("Value",
                                                   property.floatValue,
                                                   floatStepFromMetadata(metadata, 0.05f),
                                                   metadata.hasMinValue && metadata.hasMaxValue ? metadata.minValue
                                                                                                : 0.0f,
                                                   metadata.hasMinValue && metadata.hasMaxValue ? metadata.maxValue
                                                                                                : 0.0f);
            if (changed) {
                property.floatValue = clampedNumericValue(property.floatValue, metadata);
            }
            break;
        case luna::ScriptPropertyType::String:
            if (!metadata.options.empty()) {
                changed |= drawStringOptionEditor(property);
            } else {
                changed |= luna::editor::ui::drawTextMultiline("Value", property.stringValue);
            }
            break;
        case luna::ScriptPropertyType::Vec3:
            changed |= luna::editor::ui::drawVec3Control(
                "Value", property.vec3Value, 0.0f, floatStepFromMetadata(metadata, 0.05f));
            if (changed) {
                property.vec3Value.x = clampedNumericValue(property.vec3Value.x, metadata);
                property.vec3Value.y = clampedNumericValue(property.vec3Value.y, metadata);
                property.vec3Value.z = clampedNumericValue(property.vec3Value.z, metadata);
            }
            break;
        case luna::ScriptPropertyType::Entity:
            changed |= drawEntityReferenceEditor(owner_entity, property.entityValue, metadata.entityFilter);
            break;
        case luna::ScriptPropertyType::Asset:
            {
                std::optional<std::string> rejected_selection_message;
                const luna::AssetType required_type = parseAssetTypeFilter(metadata.assetType);
                auto accepts_asset_handle = [&](luna::AssetHandle handle) {
                    if (required_type == luna::AssetType::None || !handle.isValid()) {
                        rejected_selection_message.reset();
                        return true;
                    }
                    if (!luna::AssetDatabase::exists(handle)) {
                        rejected_selection_message = "Referenced asset does not exist.";
                        return false;
                    }

                    const luna::AssetMetadata& asset_metadata = luna::AssetDatabase::getAssetMetadata(handle);
                    if (asset_metadata.Type != required_type) {
                        rejected_selection_message =
                            "Selected asset must be " + std::string(luna::AssetUtils::AssetTypeToString(required_type)) +
                            ".";
                        return false;
                    }

                    rejected_selection_message.reset();
                    return true;
                };

                changed |= luna::editor::ui::drawAssetHandleSelector(
                    "Asset", property.assetValue, {}, accepts_asset_handle);
                if (rejected_selection_message.has_value()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                                       "%s",
                                       rejected_selection_message->c_str());
                }
            }
            if (!property.assetValue.isValid()) {
                ImGui::TextDisabled("Resolved Asset: None");
            } else if (!luna::AssetDatabase::exists(property.assetValue)) {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Referenced asset does not exist.");
            } else {
                const auto& asset_metadata = luna::AssetDatabase::getAssetMetadata(property.assetValue);
                ImGui::TextDisabled("Resolved Asset: %s",
                                    luna::editor::ui::assetDisplayLabel(property.assetValue).c_str());
                ImGui::TextDisabled("Asset Type: %s", luna::AssetUtils::AssetTypeToString(asset_metadata.Type));
                const luna::AssetType required_type = parseAssetTypeFilter(property.metadata.assetType);
                if (required_type != luna::AssetType::None && asset_metadata.Type != required_type) {
                    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                                       "Referenced asset does not match required type '%s'.",
                                       luna::AssetUtils::AssetTypeToString(required_type));
                }
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

    const bool has_metadata = hasScriptPropertyMetadata(property.metadata);
    std::string property_label = !property.metadata.displayName.empty()
                                     ? property.metadata.displayName
                                     : (property.name.empty() ? "<unnamed>" : property.name);
    if (!property.metadata.category.empty()) {
        property_label = property.metadata.category + " / " + property_label;
    }
    property_label += " (";
    if (!property.metadata.displayName.empty() && !equalsIgnoreCase(property.metadata.displayName, property.name)) {
        property_label += property.name;
        property_label += ", ";
    }
    property_label += scriptPropertyTypeToString(property.type);
    property_label += ")";

    const bool open = luna::editor::ui::beginSection(property_label.c_str(), "##Property");

    bool remove_requested = false;
    if (ImGui::BeginPopupContextItem("PropertyContext")) {
        if (ImGui::MenuItem("Remove Property")) {
            remove_requested = true;
        }
        ImGui::EndPopup();
    }

    if (open) {
        bool name_deactivated = false;
        const std::string name_before_edit = property.name;
        if (luna::editor::ui::drawTextInput("Name", property.name, 256, {}, &name_deactivated)) {
            changed = true;
        }
        if (name_deactivated) {
            const std::string normalized_name = makeUniquePropertyName(script, property_index, property.name);
            if (property.name != normalized_name) {
                property.name = normalized_name;
                changed = true;
            }
            if (property.name != name_before_edit) {
                clearScriptPropertyMetadata(property);
            }
        }

        const char* current_type_label = scriptPropertyTypeToString(property.type);
        changed |= luna::editor::ui::drawCombo("Type", current_type_label, [&]() {
            bool type_changed = false;
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
                        clearScriptPropertyMetadata(property);
                        type_changed = true;
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            return type_changed;
        });

        if (has_metadata && !property.metadata.description.empty()) {
            ImGui::TextWrapped("%s", property.metadata.description.c_str());
        }
        if (has_metadata && !property.metadata.category.empty()) {
            luna::editor::ui::drawTextValue("Category", property.metadata.category);
        }

        changed |= drawScriptPropertyValueEditor(owner_entity, property);

        luna::editor::ui::endSection();
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

ScriptComponentInspectorChange drawScriptComponentInspector(Entity owner_entity, ScriptComponent& script_component)
{
    ScriptComponentInspectorChange change{};

    if (editor::ui::drawBool("Enabled", script_component.enabled)) {
        change.changed = true;
        change.script_structure_changed = true;
    }
    editor::ui::drawTextValue("Scripts", std::to_string(script_component.scripts.size()));

    if (editor::ui::drawButton("Add Script", editor::ui::ButtonVariant::Primary, ImVec2(-1.0f, 0.0f))) {
        ScriptEntry entry{};
        entry.id = UUID{};
        script_component.scripts.push_back(std::move(entry));
        change.changed = true;
        change.script_structure_changed = true;
    }

    for (size_t script_index = 0; script_index < script_component.scripts.size(); ++script_index) {
        auto& script = script_component.scripts[script_index];
        ImGui::PushID(static_cast<int>(script_index));
        const std::string script_header = "Script " + std::to_string(script_index);
        const bool script_open = editor::ui::beginSection(script_header.c_str(), "##ScriptEntry");
        bool remove_script_requested = false;
        if (ImGui::BeginPopupContextItem("ScriptEntryContext")) {
            if (ImGui::MenuItem("Remove Script")) {
                remove_script_requested = true;
            }
            ImGui::EndPopup();
        }
        if (script_open) {
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, editor::ui::scale(10.0f));
            if (editor::ui::drawBool("Script Enabled", script.enabled)) {
                change.changed = true;
                change.script_structure_changed = true;
            }

            if (editor::ui::drawTextInput("Type Name", script.typeName)) {
                change.changed = true;
                change.script_structure_changed = true;
            }

            if (drawScriptAssetEditor(script)) {
                change.changed = true;
                change.script_structure_changed = true;
            }
            if (editor::ui::drawInt("Execution Order", script.executionOrder)) {
                change.changed = true;
                change.script_structure_changed = true;
            }

            const bool properties_open = editor::ui::beginSection("Properties", "##ScriptProperties");
            if (drawSchemaSyncControls(script)) {
                change.changed = true;
                change.script_structure_changed = true;
            }
            if (properties_open) {
                ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, editor::ui::scale(10.0f));
                editor::ui::drawTextValue("Property Count", std::to_string(script.properties.size()));
                if (editor::ui::drawButton("Add Property", editor::ui::ButtonVariant::Primary, ImVec2(-1.0f, 0.0f))) {
                    ScriptProperty property{};
                    property.name = makeUniquePropertyName(script, script.properties.size(), "Property");
                    script.properties.push_back(std::move(property));
                    change.changed = true;
                    change.script_structure_changed = true;
                }
                if (normalizeScriptPropertyNames(script)) {
                    change.changed = true;
                    change.script_structure_changed = true;
                }

                if (script.properties.empty()) {
                    ImGui::TextDisabled("No properties.");
                } else {
                    for (size_t property_index = 0; property_index < script.properties.size(); ++property_index) {
                        ImGui::PushID(static_cast<int>(property_index));
                        bool removed_property = false;
                        if (drawScriptPropertyEditor(owner_entity, script, property_index, removed_property)) {
                            change.changed = true;
                            if (removed_property) {
                                change.script_structure_changed = true;
                            } else if (!change.script_structure_changed) {
                                change.property_value_changed = true;
                                change.script_index = script_index;
                                change.property_index = property_index;
                            }
                        }
                        ImGui::PopID();

                        if (removed_property) {
                            break;
                        }
                    }
                }

                ImGui::PopStyleVar();
                editor::ui::endSection();
            }

            ImGui::PopStyleVar();
            editor::ui::endSection();
        }

        if (remove_script_requested) {
            script_component.scripts.erase(script_component.scripts.begin() + static_cast<std::ptrdiff_t>(script_index));
            change.changed = true;
            change.script_structure_changed = true;
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    return change;
}

} // namespace luna
