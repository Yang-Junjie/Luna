#include "InspectorPlugin.h"

#include "EditorApi/EditorApi.h"
#include "EditorUI.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.inspector";
constexpr const char* kWindowId = "luna.editor.inspector.window";
constexpr const char* kAddComponentPopupId = "AddComponentPopup";
constexpr const char* kEntityDragPayload = "LUNA_ENTITY";

bool sameVec3(const luna::editor::Vec3& lhs, const luna::editor::Vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameTransform(const luna::editor::SceneTransform& lhs, const luna::editor::SceneTransform& rhs)
{
    return sameVec3(lhs.translation, rhs.translation) && sameVec3(lhs.rotation_degrees, rhs.rotation_degrees) &&
           sameVec3(lhs.scale, rhs.scale);
}

bool sameCamera(const luna::editor::SceneCameraComponent& lhs, const luna::editor::SceneCameraComponent& rhs)
{
    return lhs.primary == rhs.primary && lhs.fixed_aspect_ratio == rhs.fixed_aspect_ratio &&
           lhs.projection == rhs.projection &&
           lhs.perspective_vertical_fov_degrees == rhs.perspective_vertical_fov_degrees &&
           lhs.perspective_near == rhs.perspective_near && lhs.perspective_far == rhs.perspective_far &&
           lhs.orthographic_size == rhs.orthographic_size && lhs.orthographic_near == rhs.orthographic_near &&
           lhs.orthographic_far == rhs.orthographic_far;
}

bool sameLight(const luna::editor::SceneLightComponent& lhs, const luna::editor::SceneLightComponent& rhs)
{
    return lhs.type == rhs.type && lhs.enabled == rhs.enabled && sameVec3(lhs.color, rhs.color) &&
           lhs.intensity == rhs.intensity && lhs.range == rhs.range &&
           lhs.inner_cone_angle_degrees == rhs.inner_cone_angle_degrees &&
           lhs.outer_cone_angle_degrees == rhs.outer_cone_angle_degrees;
}

bool sameMesh(const luna::editor::SceneMeshComponent& lhs, const luna::editor::SceneMeshComponent& rhs)
{
    return lhs.mesh_handle == rhs.mesh_handle && lhs.first_submesh == rhs.first_submesh &&
           lhs.submesh_count == rhs.submesh_count && lhs.submesh_materials == rhs.submesh_materials;
}

bool sameScriptPropertyOption(const luna::editor::SceneScriptPropertyOption& lhs,
                              const luna::editor::SceneScriptPropertyOption& rhs)
{
    return lhs.label == rhs.label && lhs.int_value == rhs.int_value && lhs.string_value == rhs.string_value;
}

bool sameScriptPropertyMetadata(const luna::editor::SceneScriptPropertyMetadata& lhs,
                                const luna::editor::SceneScriptPropertyMetadata& rhs)
{
    return lhs.display_name == rhs.display_name && lhs.description == rhs.description &&
           lhs.category == rhs.category && lhs.has_min_value == rhs.has_min_value &&
           lhs.has_max_value == rhs.has_max_value && lhs.has_step_value == rhs.has_step_value &&
           lhs.min_value == rhs.min_value && lhs.max_value == rhs.max_value && lhs.step_value == rhs.step_value &&
           lhs.asset_type == rhs.asset_type && lhs.entity_filter == rhs.entity_filter &&
           lhs.options.size() == rhs.options.size() &&
           std::equal(lhs.options.begin(), lhs.options.end(), rhs.options.begin(), sameScriptPropertyOption);
}

bool sameScriptProperty(const luna::editor::SceneScriptProperty& lhs,
                        const luna::editor::SceneScriptProperty& rhs)
{
    return lhs.name == rhs.name && lhs.type == rhs.type && lhs.bool_value == rhs.bool_value &&
           lhs.int_value == rhs.int_value && lhs.float_value == rhs.float_value &&
           lhs.string_value == rhs.string_value && sameVec3(lhs.vec3_value, rhs.vec3_value) &&
           lhs.entity_value == rhs.entity_value && lhs.asset_value == rhs.asset_value &&
           sameScriptPropertyMetadata(lhs.metadata, rhs.metadata);
}

bool sameScriptEntry(const luna::editor::SceneScriptEntry& lhs, const luna::editor::SceneScriptEntry& rhs)
{
    return lhs.id == rhs.id && lhs.enabled == rhs.enabled && lhs.script_asset == rhs.script_asset &&
           lhs.type_name == rhs.type_name && lhs.execution_order == rhs.execution_order &&
           lhs.properties.size() == rhs.properties.size() &&
           std::equal(lhs.properties.begin(), lhs.properties.end(), rhs.properties.begin(), sameScriptProperty);
}

bool sameScript(const luna::editor::SceneScriptComponent& lhs, const luna::editor::SceneScriptComponent& rhs)
{
    return lhs.enabled == rhs.enabled && lhs.scripts.size() == rhs.scripts.size() &&
           std::equal(lhs.scripts.begin(), lhs.scripts.end(), rhs.scripts.begin(), sameScriptEntry);
}

std::string entityLabel(const luna::editor::SceneEntityReference& entity)
{
    return entity.name.empty() ? entity.id.toString() : entity.name;
}

std::string scopedEntityLabel(const luna::editor::SceneEntityReference& entity, const char* scope)
{
    return entityLabel(entity) + "##" + scope + entity.id.toString();
}

const char* cameraProjectionLabel(luna::editor::SceneCameraProjection projection)
{
    switch (projection) {
        case luna::editor::SceneCameraProjection::Perspective:
            return "Perspective";
        case luna::editor::SceneCameraProjection::Orthographic:
            return "Orthographic";
    }

    return "Perspective";
}

const char* lightTypeLabel(luna::editor::SceneLightType type)
{
    switch (type) {
        case luna::editor::SceneLightType::Directional:
            return "Directional";
        case luna::editor::SceneLightType::Point:
            return "Point";
        case luna::editor::SceneLightType::Spot:
            return "Spot";
    }

    return "Directional";
}

const char* scriptPropertyTypeLabel(luna::editor::SceneScriptPropertyType type)
{
    switch (type) {
        case luna::editor::SceneScriptPropertyType::Bool:
            return "Bool";
        case luna::editor::SceneScriptPropertyType::Int:
            return "Int";
        case luna::editor::SceneScriptPropertyType::Float:
            return "Float";
        case luna::editor::SceneScriptPropertyType::String:
            return "String";
        case luna::editor::SceneScriptPropertyType::Vec3:
            return "Vec3";
        case luna::editor::SceneScriptPropertyType::Entity:
            return "Entity";
        case luna::editor::SceneScriptPropertyType::Asset:
            return "Asset";
    }

    return "Float";
}

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        const unsigned char left = static_cast<unsigned char>(lhs[index]);
        const unsigned char right = static_cast<unsigned char>(rhs[index]);
        if (std::tolower(left) != std::tolower(right)) {
            return false;
        }
    }

    return true;
}

std::string trimString(std::string value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool parseEntityId(std::string_view text, luna::editor::EntityId& out_entity_id)
{
    if (text.empty()) {
        out_entity_id = luna::editor::EntityId(0);
        return true;
    }

    uint64_t value = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return false;
    }

    out_entity_id = luna::editor::EntityId(value);
    return true;
}

bool entityMatchesFilter(const luna::editor::SceneEntityDetails& entity, std::string_view entity_filter)
{
    if (entity_filter.empty() || equalsIgnoreCase(entity_filter, "Any")) {
        return true;
    }

    if (equalsIgnoreCase(entity_filter, "Camera") || equalsIgnoreCase(entity_filter, "CameraComponent")) {
        return entity.components.camera;
    }
    if (equalsIgnoreCase(entity_filter, "Light") || equalsIgnoreCase(entity_filter, "LightComponent")) {
        return entity.components.light;
    }
    if (equalsIgnoreCase(entity_filter, "Mesh") || equalsIgnoreCase(entity_filter, "MeshComponent")) {
        return entity.components.mesh;
    }
    if (equalsIgnoreCase(entity_filter, "Script") || equalsIgnoreCase(entity_filter, "ScriptComponent")) {
        return entity.components.script;
    }

    return true;
}

bool entityMatchesFilter(luna::editor::Host& host,
                         luna::editor::EntityId entity_id,
                         std::string_view entity_filter)
{
    if (entity_filter.empty() || equalsIgnoreCase(entity_filter, "Any")) {
        return true;
    }
    const std::optional<luna::editor::SceneEntityDetails> entity = host.scene().entityDetails(entity_id);
    return entity && entityMatchesFilter(*entity, entity_filter);
}

float clampFloat(float value, const luna::editor::SceneScriptPropertyMetadata& metadata)
{
    if (metadata.has_min_value) {
        value = std::max(value, metadata.min_value);
    }
    if (metadata.has_max_value) {
        value = std::min(value, metadata.max_value);
    }
    return value;
}

int clampInt(int value, const luna::editor::SceneScriptPropertyMetadata& metadata)
{
    if (metadata.has_min_value) {
        value = std::max(value, static_cast<int>(metadata.min_value));
    }
    if (metadata.has_max_value) {
        value = std::min(value, static_cast<int>(metadata.max_value));
    }
    return value;
}

float scriptFloatStep(const luna::editor::SceneScriptPropertyMetadata& metadata, float fallback)
{
    return metadata.has_step_value && metadata.step_value > 0.0f ? metadata.step_value : fallback;
}

int scriptIntStep(const luna::editor::SceneScriptPropertyMetadata& metadata)
{
    return metadata.has_step_value && metadata.step_value > 0.0f ? std::max(1, static_cast<int>(metadata.step_value))
                                                                 : 1;
}

luna::AssetType parseAssetTypeFilter(std::string_view asset_type)
{
    if (asset_type.empty() || equalsIgnoreCase(asset_type, "Any") || equalsIgnoreCase(asset_type, "None")) {
        return luna::AssetType::None;
    }

    const luna::AssetType asset_types[]{
        luna::AssetType::Texture,
        luna::AssetType::Mesh,
        luna::AssetType::Material,
        luna::AssetType::Model,
        luna::AssetType::Scene,
        luna::AssetType::Script,
    };
    for (const luna::AssetType type : asset_types) {
        if (equalsIgnoreCase(asset_type, luna::AssetUtils::AssetTypeToString(type))) {
            return type;
        }
    }

    return luna::AssetType::None;
}

std::string assetTypeLabel(luna::AssetType type)
{
    return type == luna::AssetType::None ? std::string("None") : std::string(luna::AssetUtils::AssetTypeToString(type));
}

void resetScriptPropertyValue(luna::editor::SceneScriptProperty& property)
{
    property.bool_value = false;
    property.int_value = 0;
    property.float_value = 0.0f;
    property.string_value.clear();
    property.vec3_value = {};
    property.entity_value = luna::editor::EntityId(0);
    property.asset_value = luna::AssetHandle(0);
}

void clearScriptPropertyMetadata(luna::editor::SceneScriptProperty& property)
{
    property.metadata = {};
}

std::string makeUniquePropertyName(const luna::editor::SceneScriptEntry& script,
                                   std::size_t property_index,
                                   std::string desired_name)
{
    desired_name = trimString(std::move(desired_name));
    if (desired_name.empty()) {
        desired_name = "Property";
    }

    const auto is_name_available = [&](std::string_view candidate) {
        for (std::size_t other_index = 0; other_index < script.properties.size(); ++other_index) {
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

bool normalizeScriptPropertyNames(luna::editor::SceneScriptEntry& script)
{
    bool changed = false;
    for (std::size_t property_index = 0; property_index < script.properties.size(); ++property_index) {
        luna::editor::SceneScriptProperty& property = script.properties[property_index];
        const std::string normalized_name = makeUniquePropertyName(script, property_index, property.name);
        if (property.name != normalized_name) {
            property.name = normalized_name;
            changed = true;
        }
    }
    return changed;
}

std::size_t resolveSubmeshCount(const luna::editor::SceneMeshComponent& mesh, std::size_t total_submeshes)
{
    if (mesh.first_submesh >= total_submeshes) {
        return 0;
    }

    const std::size_t remaining = total_submeshes - mesh.first_submesh;
    if (mesh.submesh_count == luna::editor::SceneMeshComponent::AllSubmeshes) {
        return remaining;
    }

    return std::min(remaining, static_cast<std::size_t>(mesh.submesh_count));
}

luna::AssetHandle defaultBuiltinMaterial(luna::editor::Host& host)
{
    const std::vector<luna::editor::AssetInfo> materials = host.assets().builtinAssets(luna::AssetType::Material);
    return materials.empty() ? luna::AssetHandle(0) : materials.front().handle;
}

void syncMaterialSlotsToMesh(luna::editor::Host& host, luna::editor::SceneMeshComponent& mesh)
{
    const std::optional<std::size_t> total_submeshes = host.assets().meshSubmeshCount(mesh.mesh_handle);
    if (!total_submeshes) {
        return;
    }

    const std::size_t slot_count = resolveSubmeshCount(mesh, *total_submeshes);
    mesh.submesh_materials.resize(slot_count, defaultBuiltinMaterial(host));
    const luna::AssetHandle default_material = defaultBuiltinMaterial(host);
    for (luna::AssetHandle& material : mesh.submesh_materials) {
        if (!material.isValid()) {
            material = default_material;
        }
    }
}

constexpr float kPropertyLabelWidth = 108.0f;

std::string hiddenLabel(std::string_view id)
{
    return "##" + std::string(id);
}

std::string hiddenLabel(std::string_view id, std::string_view suffix)
{
    return "##" + std::string(id) + std::string(suffix);
}

bool fullWidthButton(luna::editor::Ui& ui, std::string_view label)
{
    return ui.button(label,
                     luna::editor::Vec2{.x = -1.0f, .y = 0.0f},
                     luna::editor::ButtonVariant::Subtle);
}

bool beginPropertyTable(luna::editor::Ui& ui, std::string_view id, float label_width = kPropertyLabelWidth)
{
    const luna::editor::TableFlags flags = luna::editor::TableFlag::RowBg |
                                           luna::editor::TableFlag::BordersInnerH |
                                           luna::editor::TableFlag::SizingStretchProp;
    if (!ui.beginTable(id, 2, flags)) {
        return false;
    }

    ui.tableSetupColumn("Property",
                        static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                        label_width);
    ui.tableSetupColumn("Value",
                        static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthStretch));
    return true;
}

void beginPropertyRow(luna::editor::Ui& ui, std::string_view label)
{
    ui.tableNextRow();
    (void) ui.tableNextColumn();
    ui.textDisabled(label);
    (void) ui.tableNextColumn();
    ui.setNextItemWidth(-1.0f);
}

void drawTextProperty(luna::editor::Ui& ui, std::string_view label, std::string_view value)
{
    beginPropertyRow(ui, label);
    ui.textDisabled(value);
}

bool drawInputTextProperty(luna::editor::Ui& ui,
                           std::string_view label,
                           std::string_view id,
                           std::string& value,
                           std::size_t buffer_size)
{
    beginPropertyRow(ui, label);
    return ui.inputText(hiddenLabel(id), value, buffer_size);
}

bool drawCheckboxProperty(luna::editor::Ui& ui, std::string_view label, std::string_view id, bool& value)
{
    beginPropertyRow(ui, label);
    return ui.checkbox(hiddenLabel(id), value);
}

bool drawDragFloatProperty(luna::editor::Ui& ui,
                           std::string_view label,
                           std::string_view id,
                           float& value,
                           float speed,
                           float min_value,
                           float max_value,
                           std::string_view format)
{
    beginPropertyRow(ui, label);
    return ui.dragFloat(hiddenLabel(id), value, speed, min_value, max_value, format);
}

bool drawDragIntProperty(luna::editor::Ui& ui,
                         std::string_view label,
                         std::string_view id,
                         int& value,
                         float speed,
                         int min_value,
                         int max_value)
{
    beginPropertyRow(ui, label);
    return ui.dragInt(hiddenLabel(id), value, speed, min_value, max_value);
}

bool drawDragFloat3Property(luna::editor::Ui& ui,
                            std::string_view label,
                            std::string_view id,
                            luna::editor::Vec3& value,
                            float speed,
                            float min_value,
                            float max_value,
                            std::string_view format)
{
    beginPropertyRow(ui, label);
    return ui.dragFloat3(hiddenLabel(id), value, speed, min_value, max_value, format);
}

bool drawColor3Property(luna::editor::Ui& ui,
                        std::string_view label,
                        std::string_view id,
                        luna::editor::Vec3& value)
{
    beginPropertyRow(ui, label);
    return ui.colorEdit3(hiddenLabel(id), value);
}

struct InspectorSection {
    bool open{false};
    bool remove_requested{false};
};

struct CompactInspectorStyleGuard {
    CompactInspectorStyleGuard()
    {
        luna::editor::ui::pushCompactInspectorStyle();
    }

    ~CompactInspectorStyleGuard()
    {
        luna::editor::ui::popCompactInspectorStyle();
    }
};

InspectorSection beginInspectorSection(luna::editor::Ui& ui,
                                       std::string_view id,
                                       std::string_view label,
                                       bool allow_remove = false,
                                       std::string_view remove_label = "Remove Component")
{
    InspectorSection section{
        .open = ui.beginSection(id, label, true),
    };

    if (allow_remove && ui.beginPopupContextItem(std::string(id) + "Context")) {
        section.remove_requested = ui.menuItem(remove_label);
        ui.endPopup();
    }

    return section;
}

void endInspectorSection(luna::editor::Ui& ui, const InspectorSection& section)
{
    if (section.open) {
        ui.endSection();
    }
}

bool drawCameraProjectionCombo(luna::editor::Ui& ui,
                               luna::editor::SceneCameraProjection& projection,
                               std::string_view label)
{
    if (!ui.beginCombo(label, cameraProjectionLabel(projection))) {
        return false;
    }

    bool changed = false;
    const luna::editor::SceneCameraProjection projections[]{
        luna::editor::SceneCameraProjection::Perspective,
        luna::editor::SceneCameraProjection::Orthographic,
    };
    for (const luna::editor::SceneCameraProjection candidate : projections) {
        const bool selected = projection == candidate;
        if (ui.selectable(cameraProjectionLabel(candidate), selected)) {
            projection = candidate;
            changed = true;
        }
        if (selected) {
            ui.setItemDefaultFocus();
        }
    }
    ui.endCombo();
    return changed;
}

bool drawLightTypeCombo(luna::editor::Ui& ui, luna::editor::SceneLightType& type, std::string_view label)
{
    if (!ui.beginCombo(label, lightTypeLabel(type))) {
        return false;
    }

    bool changed = false;
    const luna::editor::SceneLightType types[]{
        luna::editor::SceneLightType::Directional,
        luna::editor::SceneLightType::Point,
        luna::editor::SceneLightType::Spot,
    };
    for (const luna::editor::SceneLightType candidate : types) {
        const bool selected = type == candidate;
        if (ui.selectable(lightTypeLabel(candidate), selected)) {
            type = candidate;
            changed = true;
        }
        if (selected) {
            ui.setItemDefaultFocus();
        }
    }
    ui.endCombo();
    return changed;
}

bool drawScriptPropertyTypeCombo(luna::editor::Ui& ui,
                                 luna::editor::SceneScriptPropertyType& type,
                                 std::string_view id_suffix)
{
    if (!ui.beginCombo(hiddenLabel("Type", id_suffix), scriptPropertyTypeLabel(type))) {
        return false;
    }

    bool changed = false;
    const luna::editor::SceneScriptPropertyType property_types[]{
        luna::editor::SceneScriptPropertyType::Bool,
        luna::editor::SceneScriptPropertyType::Int,
        luna::editor::SceneScriptPropertyType::Float,
        luna::editor::SceneScriptPropertyType::String,
        luna::editor::SceneScriptPropertyType::Vec3,
        luna::editor::SceneScriptPropertyType::Entity,
        luna::editor::SceneScriptPropertyType::Asset,
    };
    for (const luna::editor::SceneScriptPropertyType candidate : property_types) {
        const bool selected = type == candidate;
        if (ui.selectable(scriptPropertyTypeLabel(candidate), selected)) {
            type = candidate;
            changed = true;
        }
        if (selected) {
            ui.setItemDefaultFocus();
        }
    }
    ui.endCombo();
    return changed;
}

bool drawBuiltinAssetCombo(luna::editor::Host& host,
                           luna::editor::Ui& ui,
                           const std::string& label,
                           luna::AssetType type,
                           luna::AssetHandle& handle)
{
    const luna::editor::AssetInfo current_asset = host.assets().describeAsset(handle);
    const bool current_is_builtin_type = current_asset.builtin && current_asset.type == type;
    const std::string preview = current_is_builtin_type ? current_asset.label : std::string("None");
    if (!ui.beginCombo(label, preview)) {
        return false;
    }

    bool changed = false;
    if (ui.selectable("None", !current_is_builtin_type)) {
        if (handle.isValid()) {
            handle = luna::AssetHandle(0);
            changed = true;
        }
    }

    for (const luna::editor::AssetInfo& builtin_asset : host.assets().builtinAssets(type)) {
        const bool selected = handle == builtin_asset.handle;
        if (ui.selectable(builtin_asset.label, selected)) {
            if (handle != builtin_asset.handle) {
                handle = builtin_asset.handle;
                changed = true;
            }
        }
        if (selected) {
            ui.setItemDefaultFocus();
        }
    }

    ui.endCombo();
    return changed;
}

bool drawAssetField(luna::editor::Host& host,
                    luna::editor::Ui& ui,
                    const std::string& id,
                    luna::AssetHandle& handle,
                    std::initializer_list<luna::AssetType> accepted_types)
{
    bool changed = false;
    const luna::editor::AssetInfo asset = host.assets().describeAsset(handle);

    const std::string button_label = asset.label + "##" + id;
    (void) ui.button(button_label, luna::editor::Vec2{.x = -1.0f, .y = ui.scale(38.0f)});

    const bool hovered = ui.isItemHovered();
    if (ui.beginDragDropTarget()) {
        luna::editor::AssetDropPayload payload{};
        if (ui.acceptAssetDragDropPayload(payload, accepted_types) && handle != payload.handle) {
            handle = payload.handle;
            changed = true;
        }
        ui.endDragDropTarget();
    }

    const std::string context_id = id + "Context";
    if (ui.beginPopupContextItem(context_id)) {
        if (ui.menuItem("Clear", false, handle.isValid())) {
            handle = luna::AssetHandle(0);
            changed = true;
        }
        ui.endPopup();
    }

    std::string detail = asset.detail;
    if (handle.isValid()) {
        detail += " | " + handle.toString();
    }
    if (asset.loading) {
        detail += " | Loading";
    }
    ui.textDisabled(detail);

    if (hovered) {
        ui.setTooltip("Drop an accepted asset here. Right-click to clear.");
    }

    return changed;
}

void drawEntityHeader(luna::editor::Host& host, luna::editor::Ui& ui, const luna::editor::SceneEntityDetails& details)
{
    const InspectorSection section = beginInspectorSection(ui, "InspectorEntity", "Entity");
    if (!section.open) {
        endInspectorSection(ui, section);
        return;
    }

    if (!host.scene().canEditScene()) {
        ui.beginDisabled();
    }
    std::string name = details.name;
    if (beginPropertyTable(ui, "EntityHeader")) {
        if (drawInputTextProperty(ui, "Name", "EntityName", name, 256) && name != details.name) {
            (void) host.scene().setEntityName(details.id, std::move(name));
        }
        drawTextProperty(ui, "UUID", details.id.toString());
        ui.endTable();
    }
    if (!host.scene().canEditScene()) {
        ui.endDisabled();
    }
    endInspectorSection(ui, section);
}

void drawAddComponentPopup(luna::editor::Host& host,
                           luna::editor::Ui& ui,
                           const luna::editor::SceneEntityDetails& details)
{
    if (!ui.beginPopup(kAddComponentPopupId)) {
        return;
    }

    const auto draw_add_item = [&](bool missing, std::string_view label, luna::editor::SceneComponentKind kind) {
        if (missing && ui.menuItem(label)) {
            (void) host.scene().addComponent(details.id, kind);
            ui.closeCurrentPopup();
        }
    };

    draw_add_item(!details.components.mesh, "Mesh", luna::editor::SceneComponentKind::Mesh);
    draw_add_item(!details.components.camera, "Camera", luna::editor::SceneComponentKind::Camera);
    draw_add_item(!details.components.light, "Light", luna::editor::SceneComponentKind::Light);
    draw_add_item(!details.components.script, "Script", luna::editor::SceneComponentKind::Script);

    ui.endPopup();
}

void drawAddComponentActions(luna::editor::Host& host,
                             luna::editor::Ui& ui,
                             const luna::editor::SceneEntityDetails& details)
{
    if (details.components.camera && details.components.light && details.components.mesh && details.components.script) {
        return;
    }

    const InspectorSection section = beginInspectorSection(ui, "InspectorAddComponent", "Add Component");
    if (!section.open) {
        endInspectorSection(ui, section);
        return;
    }

    if (!host.scene().canEditScene()) {
        ui.beginDisabled();
    }
    if (ui.button("Add Component",
                  luna::editor::Vec2{.x = -1.0f, .y = 0.0f},
                  luna::editor::ButtonVariant::Subtle)) {
        ui.openPopup(kAddComponentPopupId);
    }
    drawAddComponentPopup(host, ui, details);
    if (!host.scene().canEditScene()) {
        ui.endDisabled();
    }
    endInspectorSection(ui, section);
}

void drawTransform(luna::editor::Host& host, luna::editor::Ui& ui, const luna::editor::SceneEntityDetails& details)
{
    if (!details.components.transform) {
        return;
    }

    const InspectorSection section =
        beginInspectorSection(ui, "InspectorTransform", "Transform", false, "Remove Component");
    if (!section.open) {
        endInspectorSection(ui, section);
        return;
    }

    luna::editor::SceneTransform transform = details.transform;
    if (!host.scene().canEditScene()) {
        ui.beginDisabled();
    }

    bool changed = false;
    if (beginPropertyTable(ui, "TransformProperties")) {
        changed |= drawDragFloat3Property(ui, "Translation", "TransformTranslation", transform.translation, 0.05f, 0.0f, 0.0f, "%.2f");
        changed |= drawDragFloat3Property(ui, "Rotation", "TransformRotation", transform.rotation_degrees, 0.25f, 0.0f, 0.0f, "%.2f");
        changed |= drawDragFloat3Property(ui, "Scale", "TransformScale", transform.scale, 0.01f, 0.0f, 0.0f, "%.2f");
        ui.endTable();
    }

    if (!host.scene().canEditScene()) {
        ui.endDisabled();
    }

    if (changed && !sameTransform(transform, details.transform)) {
        (void) host.scene().setEntityTransform(details.id, transform);
    }

    endInspectorSection(ui, section);
}

void drawCamera(luna::editor::Host& host, luna::editor::Ui& ui, const luna::editor::SceneEntityDetails& details)
{
    if (!details.camera) {
        return;
    }

    const InspectorSection section =
        beginInspectorSection(ui,
                              "InspectorCamera",
                              "Camera",
                              host.scene().canEditScene(),
                              "Remove Component");
    if (section.remove_requested) {
        endInspectorSection(ui, section);
        (void) host.scene().removeComponent(details.id, luna::editor::SceneComponentKind::Camera);
        return;
    }
    if (!section.open) {
        endInspectorSection(ui, section);
        return;
    }

    luna::editor::SceneCameraComponent camera = *details.camera;
    bool changed = false;

    if (!host.scene().canEditScene()) {
        ui.beginDisabled();
    }

    if (beginPropertyTable(ui, "CameraProperties")) {
        changed |= drawCheckboxProperty(ui, "Primary", "CameraPrimary", camera.primary);
        changed |= drawCheckboxProperty(ui, "Fixed Aspect", "CameraFixedAspect", camera.fixed_aspect_ratio);
        beginPropertyRow(ui, "Projection");
        changed |= drawCameraProjectionCombo(ui, camera.projection, "##CameraProjection");

        if (camera.projection == luna::editor::SceneCameraProjection::Perspective) {
            changed |= drawDragFloatProperty(ui,
                                             "Vertical FOV",
                                             "CameraPerspectiveFov",
                                             camera.perspective_vertical_fov_degrees,
                                             0.25f,
                                             1.0f,
                                             179.0f,
                                             "%.2f deg");
            changed |= drawDragFloatProperty(
                ui, "Near", "CameraPerspectiveNear", camera.perspective_near, 0.01f, 0.001f, 1000.0f, "%.3f");
            changed |= drawDragFloatProperty(
                ui, "Far", "CameraPerspectiveFar", camera.perspective_far, 1.0f, 0.001f, 10000.0f, "%.2f");
        } else {
            changed |= drawDragFloatProperty(
                ui, "Size", "CameraOrthoSize", camera.orthographic_size, 0.1f, 0.001f, 10000.0f, "%.2f");
            changed |= drawDragFloatProperty(
                ui, "Near", "CameraOrthoNear", camera.orthographic_near, 0.1f, -10000.0f, 10000.0f, "%.2f");
            changed |= drawDragFloatProperty(
                ui, "Far", "CameraOrthoFar", camera.orthographic_far, 0.1f, -10000.0f, 10000.0f, "%.2f");
        }
        ui.endTable();
    }

    if (!host.scene().canEditScene()) {
        ui.endDisabled();
    }

    camera.perspective_vertical_fov_degrees =
        std::clamp(camera.perspective_vertical_fov_degrees, 1.0f, 179.0f);
    camera.perspective_near = std::clamp(camera.perspective_near, 0.001f, 1000.0f);
    camera.perspective_far = std::clamp(camera.perspective_far, 0.001f, 10000.0f);
    camera.orthographic_size = std::clamp(camera.orthographic_size, 0.001f, 10000.0f);
    camera.orthographic_near = std::clamp(camera.orthographic_near, -10000.0f, 10000.0f);
    camera.orthographic_far = std::clamp(camera.orthographic_far, -10000.0f, 10000.0f);

    if (changed && !sameCamera(camera, *details.camera)) {
        (void) host.scene().setCameraComponent(details.id, camera);
    }

    endInspectorSection(ui, section);
}

void drawLight(luna::editor::Host& host, luna::editor::Ui& ui, const luna::editor::SceneEntityDetails& details)
{
    if (!details.light) {
        return;
    }

    const InspectorSection section =
        beginInspectorSection(ui,
                              "InspectorLight",
                              "Light",
                              host.scene().canEditScene(),
                              "Remove Component");
    if (section.remove_requested) {
        endInspectorSection(ui, section);
        (void) host.scene().removeComponent(details.id, luna::editor::SceneComponentKind::Light);
        return;
    }
    if (!section.open) {
        endInspectorSection(ui, section);
        return;
    }

    luna::editor::SceneLightComponent light = *details.light;
    bool changed = false;

    if (!host.scene().canEditScene()) {
        ui.beginDisabled();
    }

    if (beginPropertyTable(ui, "LightProperties")) {
        changed |= drawCheckboxProperty(ui, "Enabled", "LightEnabled", light.enabled);
        beginPropertyRow(ui, "Type");
        changed |= drawLightTypeCombo(ui, light.type, "##LightType");
        changed |= drawColor3Property(ui, "Color", "LightColor", light.color);
        changed |= drawDragFloatProperty(ui, "Intensity", "LightIntensity", light.intensity, 0.05f, 0.0f, 100.0f, "%.2f");

        if (light.type == luna::editor::SceneLightType::Point || light.type == luna::editor::SceneLightType::Spot) {
            changed |= drawDragFloatProperty(ui, "Range", "LightRange", light.range, 0.1f, 0.001f, 1000.0f, "%.2f");
        }
        if (light.type == luna::editor::SceneLightType::Spot) {
            changed |= drawDragFloatProperty(
                ui, "Inner Cone", "LightInnerCone", light.inner_cone_angle_degrees, 0.5f, 0.1f, 89.0f, "%.1f");
            changed |= drawDragFloatProperty(
                ui, "Outer Cone", "LightOuterCone", light.outer_cone_angle_degrees, 0.5f, 0.1f, 89.0f, "%.1f");
        }
        ui.endTable();
    }

    if (!host.scene().canEditScene()) {
        ui.endDisabled();
    }

    light.intensity = std::clamp(light.intensity, 0.0f, 100.0f);
    light.range = std::clamp(light.range, 0.001f, 1000.0f);
    light.inner_cone_angle_degrees = std::clamp(light.inner_cone_angle_degrees, 0.1f, 89.0f);
    light.outer_cone_angle_degrees = std::clamp(light.outer_cone_angle_degrees,
                                                light.inner_cone_angle_degrees,
                                                89.0f);

    if (changed && !sameLight(light, *details.light)) {
        (void) host.scene().setLightComponent(details.id, light);
    }

    endInspectorSection(ui, section);
}

void drawMesh(luna::editor::Host& host, luna::editor::Ui& ui, const luna::editor::SceneEntityDetails& details)
{
    if (!details.mesh) {
        return;
    }

    const InspectorSection section = beginInspectorSection(ui,
                                                           "InspectorMesh",
                                                           "Mesh",
                                                           host.scene().canEditScene(),
                                                           "Remove Component");
    if (section.remove_requested) {
        endInspectorSection(ui, section);
        (void) host.scene().removeComponent(details.id, luna::editor::SceneComponentKind::Mesh);
        return;
    }
    if (!section.open) {
        endInspectorSection(ui, section);
        return;
    }

    luna::editor::SceneMeshComponent mesh = *details.mesh;
    const luna::AssetHandle previous_mesh_handle = mesh.mesh_handle;
    bool changed = false;

    if (!host.scene().canEditScene()) {
        ui.beginDisabled();
    }

    if (beginPropertyTable(ui, "MeshProperties")) {
        beginPropertyRow(ui, "Primitive");
        changed |= drawBuiltinAssetCombo(host, ui, "##MeshPrimitive", luna::AssetType::Mesh, mesh.mesh_handle);
        beginPropertyRow(ui, "Mesh Asset");
        changed |= drawAssetField(host, ui, "MeshAsset", mesh.mesh_handle, {luna::AssetType::Mesh});
        ui.endTable();
    }

    if (mesh.mesh_handle != previous_mesh_handle) {
        mesh.first_submesh = 0;
        mesh.submesh_count = luna::editor::SceneMeshComponent::AllSubmeshes;
        mesh.submesh_materials.clear();
        syncMaterialSlotsToMesh(host, mesh);
        changed = true;
    }

    const std::optional<std::size_t> total_submeshes = host.assets().meshSubmeshCount(mesh.mesh_handle);
    if (total_submeshes) {
        if (beginPropertyTable(ui, "MeshSubmeshSummary")) {
            drawTextProperty(ui,
                             "Submeshes",
                             std::to_string(resolveSubmeshCount(mesh, *total_submeshes)) + " / " +
                                 std::to_string(*total_submeshes));
            ui.endTable();
        }
        if (fullWidthButton(ui, "Sync Material Slots To Mesh")) {
            syncMaterialSlotsToMesh(host, mesh);
            changed = true;
        }
    } else if (mesh.mesh_handle.isValid() && host.assets().isAssetLoading(mesh.mesh_handle)) {
        ui.textDisabled("Mesh asset is loading...");
    } else if (mesh.mesh_handle.isValid()) {
        ui.textDisabled("Mesh submesh data is unavailable.");
    }

    if (!total_submeshes) {
        int material_slot_count = static_cast<int>(mesh.submesh_materials.size());
        if (beginPropertyTable(ui, "MeshManualSlots")) {
            if (drawDragIntProperty(ui, "Material Slots", "MeshMaterialSlots", material_slot_count, 1.0f, 0, 256)) {
                material_slot_count = std::clamp(material_slot_count, 0, 256);
                mesh.submesh_materials.resize(static_cast<std::size_t>(material_slot_count));
                changed = true;
            }
            ui.endTable();
        }
    }

    ui.separatorText("Submesh Materials");
    if (mesh.submesh_materials.empty()) {
        ui.textDisabled("No material slots.");
    } else {
        for (std::size_t index = 0; index < mesh.submesh_materials.size(); ++index) {
            ui.textDisabled("Submesh " + std::to_string(index));

            luna::AssetHandle material_handle = mesh.submesh_materials[index];
            const std::string id_suffix = std::to_string(index);
            if (beginPropertyTable(ui, "SubmeshMaterialTable" + id_suffix)) {
                beginPropertyRow(ui, "Builtin");
                changed |= drawBuiltinAssetCombo(
                    host, ui, "##SubmeshBuiltinMaterial" + id_suffix, luna::AssetType::Material, material_handle);
                beginPropertyRow(ui, "Asset");
                changed |= drawAssetField(
                    host, ui, "SubmeshMaterial" + id_suffix, material_handle, {luna::AssetType::Material});
                ui.endTable();
            }

            if (mesh.submesh_materials[index] != material_handle) {
                mesh.submesh_materials[index] = material_handle;
                changed = true;
            }

            if (fullWidthButton(ui, "Clear Material##Submesh" + id_suffix)) {
                if (mesh.submesh_materials[index].isValid()) {
                    mesh.submesh_materials[index] = luna::AssetHandle(0);
                    changed = true;
                }
            }
            ui.spacing();
        }
    }

    if (!host.scene().canEditScene()) {
        ui.endDisabled();
    }

    if (changed && !sameMesh(mesh, *details.mesh)) {
        (void) host.scene().setMeshComponent(details.id, mesh);
    }

    endInspectorSection(ui, section);
}

bool drawScriptAssetSelector(luna::editor::Host& host,
                             luna::editor::Ui& ui,
                             luna::editor::SceneScriptEntry& script,
                             std::string_view id_suffix)
{
    const luna::AssetHandle previous_script_asset = script.script_asset;
    bool changed = drawAssetField(host,
                                  ui,
                                  std::string("ScriptAsset") + std::string(id_suffix),
                                  script.script_asset,
                                  {luna::AssetType::Script});

    const luna::editor::ScriptAssetValidation validation = host.scripts().validateScriptAsset(script.script_asset);
    std::string rejected_message;
    if (changed && !validation.accepted) {
        rejected_message = validation.message;
        script.script_asset = previous_script_asset;
        changed = false;
    }

    const luna::editor::ScriptAssetValidation current_validation =
        host.scripts().validateScriptAsset(script.script_asset);
    const luna::editor::AssetInfo current_asset = host.assets().describeAsset(script.script_asset);
    if (!script.script_asset.isValid()) {
        ui.textDisabled("Script Asset: None");
    } else if (current_asset.exists) {
        ui.textDisabled("Script Asset: " + current_asset.label);
        ui.textDisabled("Script Type: " + assetTypeLabel(current_asset.type));
    }

    if (!rejected_message.empty()) {
        ui.textDisabled(rejected_message);
    } else if (!current_validation.accepted && !current_validation.message.empty()) {
        ui.textDisabled(current_validation.message);
    } else if (script.script_asset.isValid() && !current_validation.language.empty()) {
        ui.textDisabled("Script Language: " + current_validation.language);
    }

    return changed;
}

bool drawScriptPropertyOptionCombo(luna::editor::Ui& ui,
                                   luna::editor::SceneScriptProperty& property,
                                   std::string_view id_suffix)
{
    bool changed = false;
    if (property.type == luna::editor::SceneScriptPropertyType::Int) {
        std::string preview = std::to_string(property.int_value);
        for (const luna::editor::SceneScriptPropertyOption& option : property.metadata.options) {
            if (option.int_value == property.int_value) {
                preview = option.label.empty() ? std::to_string(option.int_value) : option.label;
                break;
            }
        }

        if (!ui.beginCombo(hiddenLabel("Value", id_suffix), preview)) {
            return false;
        }
        for (const luna::editor::SceneScriptPropertyOption& option : property.metadata.options) {
            const std::string label = option.label.empty() ? std::to_string(option.int_value) : option.label;
            const bool selected = property.int_value == option.int_value;
            if (ui.selectable(label, selected)) {
                if (property.int_value != option.int_value) {
                    property.int_value = option.int_value;
                    changed = true;
                }
            }
            if (selected) {
                ui.setItemDefaultFocus();
            }
        }
        ui.endCombo();
        return changed;
    }

    if (property.type == luna::editor::SceneScriptPropertyType::String) {
        std::string preview = property.string_value;
        for (const luna::editor::SceneScriptPropertyOption& option : property.metadata.options) {
            if (option.string_value == property.string_value) {
                preview = option.label.empty() ? option.string_value : option.label;
                break;
            }
        }

        if (!ui.beginCombo(hiddenLabel("Value", id_suffix), preview)) {
            return false;
        }
        for (const luna::editor::SceneScriptPropertyOption& option : property.metadata.options) {
            const std::string label = option.label.empty() ? option.string_value : option.label;
            const bool selected = property.string_value == option.string_value;
            if (ui.selectable(label, selected)) {
                if (property.string_value != option.string_value) {
                    property.string_value = option.string_value;
                    changed = true;
                }
            }
            if (selected) {
                ui.setItemDefaultFocus();
            }
        }
        ui.endCombo();
        return changed;
    }

    return false;
}

bool drawScriptAssetProperty(luna::editor::Host& host,
                             luna::editor::Ui& ui,
                             luna::editor::SceneScriptProperty& property,
                             std::string_view id_suffix)
{
    const luna::AssetType required_type = parseAssetTypeFilter(property.metadata.asset_type);
    bool changed = false;
    if (required_type == luna::AssetType::None) {
        changed = drawAssetField(host,
                                 ui,
                                 std::string("ScriptPropertyAsset") + std::string(id_suffix),
                                 property.asset_value,
                                 {});
    } else {
        changed = drawAssetField(host,
                                 ui,
                                 std::string("ScriptPropertyAsset") + std::string(id_suffix),
                                 property.asset_value,
                                 {required_type});
    }

    const luna::editor::AssetInfo asset = host.assets().describeAsset(property.asset_value);
    if (!property.asset_value.isValid()) {
        ui.textDisabled("Resolved Asset: None");
    } else if (!asset.exists) {
        ui.textDisabled("Referenced asset does not exist.");
    } else {
        ui.textDisabled("Resolved Asset: " + asset.label);
        ui.textDisabled("Asset Type: " + assetTypeLabel(asset.type));
        if (required_type != luna::AssetType::None && asset.type != required_type) {
            ui.textDisabled("Referenced asset does not match required type '" + assetTypeLabel(required_type) + "'.");
        }
    }

    return changed;
}

bool drawScriptEntityProperty(luna::editor::Host& host,
                              luna::editor::Ui& ui,
                              const luna::editor::SceneEntityDetails& details,
                              luna::editor::SceneScriptProperty& property,
                              std::string_view id_suffix)
{
    bool changed = false;
    std::string entity_text = property.entity_value.isValid() ? property.entity_value.toString() : std::string{};
    if (ui.inputText(hiddenLabel("Entity", id_suffix), entity_text, 32)) {
        luna::editor::EntityId parsed_id{0};
        if (parseEntityId(entity_text, parsed_id)) {
            property.entity_value = parsed_id;
            changed = true;
        }
    }

    if (ui.beginDragDropTarget()) {
        uint64_t dropped_entity_id = 0;
        if (ui.acceptDragDropPayload(kEntityDragPayload, &dropped_entity_id, sizeof(dropped_entity_id))) {
            const luna::editor::EntityId candidate_id(dropped_entity_id);
            if (entityMatchesFilter(host, candidate_id, property.metadata.entity_filter) &&
                property.entity_value != candidate_id) {
                property.entity_value = candidate_id;
                changed = true;
            }
        }
        ui.endDragDropTarget();
    }

    const bool can_use_this_entity = entityMatchesFilter(details, property.metadata.entity_filter);
    if (!can_use_this_entity) {
        ui.beginDisabled();
    }
    if (fullWidthButton(ui, std::string("Use This Entity") + std::string(id_suffix))) {
        if (property.entity_value != details.id) {
            property.entity_value = details.id;
            changed = true;
        }
    }
    if (!can_use_this_entity) {
        ui.endDisabled();
    }
    if (fullWidthButton(ui, std::string("Clear Entity") + std::string(id_suffix))) {
        if (property.entity_value.isValid()) {
            property.entity_value = luna::editor::EntityId(0);
            changed = true;
        }
    }

    if (!property.entity_value.isValid()) {
        ui.textDisabled("Resolved Entity: None");
    } else if (const std::optional<luna::editor::SceneEntityDetails> entity =
                   host.scene().entityDetails(property.entity_value)) {
        if (entityMatchesFilter(*entity, property.metadata.entity_filter)) {
            ui.textDisabled("Resolved Entity: " + entity->name);
        } else {
            ui.textDisabled("Referenced entity does not match required filter '" +
                            property.metadata.entity_filter + "'.");
        }
    } else {
        ui.textDisabled("Referenced entity does not exist in this scene.");
    }

    return changed;
}

bool drawScriptPropertyValue(luna::editor::Host& host,
                             luna::editor::Ui& ui,
                             const luna::editor::SceneEntityDetails& details,
                             luna::editor::SceneScriptProperty& property,
                             std::string_view id_suffix)
{
    bool changed = false;
    const luna::editor::SceneScriptPropertyMetadata& metadata = property.metadata;

    switch (property.type) {
        case luna::editor::SceneScriptPropertyType::Bool:
            changed |= ui.checkbox(hiddenLabel("Value", id_suffix), property.bool_value);
            break;
        case luna::editor::SceneScriptPropertyType::Int:
            if (!metadata.options.empty()) {
                changed |= drawScriptPropertyOptionCombo(ui, property, id_suffix);
            } else {
                changed |= ui.dragInt(hiddenLabel("Value", id_suffix),
                                      property.int_value,
                                      static_cast<float>(scriptIntStep(metadata)),
                                      metadata.has_min_value && metadata.has_max_value
                                          ? static_cast<int>(metadata.min_value)
                                          : 0,
                                      metadata.has_min_value && metadata.has_max_value
                                          ? static_cast<int>(metadata.max_value)
                                          : 0);
                if (changed) {
                    property.int_value = clampInt(property.int_value, metadata);
                }
            }
            break;
        case luna::editor::SceneScriptPropertyType::Float:
            changed |= ui.dragFloat(hiddenLabel("Value", id_suffix),
                                    property.float_value,
                                    scriptFloatStep(metadata, 0.05f),
                                    metadata.has_min_value && metadata.has_max_value ? metadata.min_value : 0.0f,
                                    metadata.has_min_value && metadata.has_max_value ? metadata.max_value : 0.0f,
                                    "%.3f");
            if (changed) {
                property.float_value = clampFloat(property.float_value, metadata);
            }
            break;
        case luna::editor::SceneScriptPropertyType::String:
            if (!metadata.options.empty()) {
                changed |= drawScriptPropertyOptionCombo(ui, property, id_suffix);
            } else {
                changed |= ui.inputText(hiddenLabel("Value", id_suffix), property.string_value, 512);
            }
            break;
        case luna::editor::SceneScriptPropertyType::Vec3:
            changed |= ui.dragFloat3(hiddenLabel("Value", id_suffix),
                                     property.vec3_value,
                                     scriptFloatStep(metadata, 0.05f),
                                     metadata.has_min_value && metadata.has_max_value ? metadata.min_value : 0.0f,
                                     metadata.has_min_value && metadata.has_max_value ? metadata.max_value : 0.0f,
                                     "%.3f");
            if (changed) {
                property.vec3_value.x = clampFloat(property.vec3_value.x, metadata);
                property.vec3_value.y = clampFloat(property.vec3_value.y, metadata);
                property.vec3_value.z = clampFloat(property.vec3_value.z, metadata);
            }
            break;
        case luna::editor::SceneScriptPropertyType::Entity:
            changed |= drawScriptEntityProperty(host, ui, details, property, id_suffix);
            break;
        case luna::editor::SceneScriptPropertyType::Asset:
            changed |= drawScriptAssetProperty(host, ui, property, id_suffix);
            break;
    }

    return changed;
}

bool drawScriptProperty(luna::editor::Host& host,
                        luna::editor::Ui& ui,
                        const luna::editor::SceneEntityDetails& details,
                        luna::editor::SceneScriptEntry& script,
                        std::size_t script_index,
                        std::size_t property_index,
                        bool allow_structure_edit,
                        bool allow_value_edit,
                        bool& property_value_changed,
                        bool& property_structure_changed,
                        bool& removed_property)
{
    luna::editor::SceneScriptProperty& property = script.properties[property_index];
    const std::string id_suffix =
        "##Script" + std::to_string(script_index) + "Property" + std::to_string(property_index);
    const std::string display_name = !property.metadata.display_name.empty()
                                         ? property.metadata.display_name
                                         : (property.name.empty() ? std::string("<unnamed>") : property.name);

    const std::string property_section_id =
        "InspectorScriptProperty" + std::to_string(script_index) + "_" + std::to_string(property_index);
    const std::string property_section_label = display_name + " (" + scriptPropertyTypeLabel(property.type) + ")";
    const InspectorSection section = beginInspectorSection(
        ui,
        property_section_id,
        property_section_label,
        allow_structure_edit,
        "Remove Property");
    if (section.remove_requested) {
        script.properties.erase(script.properties.begin() + static_cast<std::ptrdiff_t>(property_index));
        removed_property = true;
        property_structure_changed = true;
        endInspectorSection(ui, section);
        return true;
    }
    if (!section.open) {
        endInspectorSection(ui, section);
        return false;
    }

    if (!property.metadata.description.empty()) {
        ui.textWrapped(property.metadata.description);
    }
    if (!property.metadata.category.empty()) {
        ui.textDisabled(property.metadata.category);
    }

    bool changed = false;
    bool name_changed = false;
    bool type_changed = false;
    bool value_changed = false;

    if (beginPropertyTable(ui, "ScriptPropertyTable" + std::to_string(script_index) + "_" + std::to_string(property_index))) {
        if (!allow_structure_edit) {
            ui.beginDisabled();
        }
        const std::string name_before_edit = property.name;
        name_changed = drawInputTextProperty(ui, "Name", std::string("Name") + std::string(id_suffix), property.name, 256);
        bool normalized_name_changed = false;
        if (ui.isItemDeactivatedAfterEdit()) {
            const std::string normalized_name = makeUniquePropertyName(script, property_index, property.name);
            if (property.name != normalized_name) {
                property.name = normalized_name;
                normalized_name_changed = true;
            }
        }
        if (name_changed || normalized_name_changed) {
            clearScriptPropertyMetadata(property);
        }

        const luna::editor::SceneScriptPropertyType previous_type = property.type;
        beginPropertyRow(ui, "Type");
        type_changed = drawScriptPropertyTypeCombo(ui, property.type, id_suffix);
        if (type_changed && property.type != previous_type) {
            resetScriptPropertyValue(property);
            clearScriptPropertyMetadata(property);
        } else if (type_changed) {
            type_changed = false;
        }
        if (!allow_structure_edit) {
            ui.endDisabled();
        }

        if (!allow_value_edit) {
            ui.beginDisabled();
        }
        beginPropertyRow(ui, "Value");
        value_changed = drawScriptPropertyValue(host, ui, details, property, id_suffix);
        if (!allow_value_edit) {
            ui.endDisabled();
        }

        ui.endTable();

        name_changed = name_changed || normalized_name_changed || property.name != name_before_edit;
    }

    property_structure_changed |= name_changed || type_changed;
    changed |= name_changed || type_changed;
    property_value_changed |= value_changed;
    changed |= value_changed;

    endInspectorSection(ui, section);
    return changed;
}

void drawScript(luna::editor::Host& host, luna::editor::Ui& ui, const luna::editor::SceneEntityDetails& details)
{
    if (!details.script) {
        return;
    }

    const InspectorSection section =
        beginInspectorSection(ui,
                              "InspectorScript",
                              "Script",
                              host.scene().canEditScene(),
                              "Remove Component");
    if (section.remove_requested) {
        endInspectorSection(ui, section);
        (void) host.scene().removeComponent(details.id, luna::editor::SceneComponentKind::Script);
        return;
    }
    if (!section.open) {
        endInspectorSection(ui, section);
        return;
    }

    luna::editor::SceneScriptComponent script_component = *details.script;
    bool changed = false;
    const bool allow_structure_edit = host.scene().canEditScene();
    const bool allow_property_value_edit =
        allow_structure_edit || host.runtimeViewport().isRuntimeViewportEnabled();

    const luna::editor::ScriptLanguageStatus language_status = host.scripts().projectScriptLanguage();
    if (beginPropertyTable(ui, "ScriptComponentProperties")) {
        if (language_status.available) {
            drawTextProperty(ui, "Language", language_status.language);
        } else if (!language_status.message.empty()) {
            drawTextProperty(ui, "Language", language_status.message);
        }

        if (!allow_structure_edit) {
            ui.beginDisabled();
        }
        changed |= drawCheckboxProperty(ui, "Enabled", "ScriptComponentEnabled", script_component.enabled);
        if (!allow_structure_edit) {
            ui.endDisabled();
        }
        drawTextProperty(ui, "Scripts", std::to_string(script_component.scripts.size()));
        ui.endTable();
    }

    if (!allow_structure_edit) {
        ui.beginDisabled();
    }
    if (fullWidthButton(ui, "Add Script##ScriptComponent")) {
        luna::editor::SceneScriptEntry entry{};
        entry.id = luna::editor::ScriptEntryId{};
        script_component.scripts.push_back(std::move(entry));
        changed = true;
    }
    if (!allow_structure_edit) {
        ui.endDisabled();
    }

    for (std::size_t script_index = 0; script_index < script_component.scripts.size(); ++script_index) {
        luna::editor::SceneScriptEntry& script = script_component.scripts[script_index];
        const std::string script_suffix = "##ScriptEntry" + std::to_string(script_index);

        const std::string script_section_id = "InspectorScriptEntry" + std::to_string(script_index);
        const InspectorSection script_section = beginInspectorSection(
            ui,
            script_section_id,
            "Script " + std::to_string(script_index),
            allow_structure_edit,
            "Remove Script");
        if (script_section.remove_requested) {
            script_component.scripts.erase(script_component.scripts.begin() + static_cast<std::ptrdiff_t>(script_index));
            changed = true;
            endInspectorSection(ui, script_section);
            break;
        }
        if (!script_section.open) {
            endInspectorSection(ui, script_section);
            continue;
        }

        if (beginPropertyTable(ui, "ScriptEntryProperties" + std::to_string(script_index))) {
            if (!allow_structure_edit) {
                ui.beginDisabled();
            }
            changed |= drawCheckboxProperty(ui, "Enabled", std::string("ScriptEnabled") + script_suffix, script.enabled);
            changed |= drawInputTextProperty(ui, "Type Name", std::string("ScriptTypeName") + script_suffix, script.type_name, 256);
            beginPropertyRow(ui, "Asset");
            changed |= drawScriptAssetSelector(host, ui, script, script_suffix);
            changed |= drawDragIntProperty(
                ui, "Execution Order", std::string("ScriptExecutionOrder") + script_suffix, script.execution_order, 1.0f, -10000, 10000);
            if (!allow_structure_edit) {
                ui.endDisabled();
            }
            ui.endTable();
        }

        ui.separatorText("Properties");
        if (beginPropertyTable(ui, "ScriptPropertySummary" + std::to_string(script_index))) {
            drawTextProperty(ui, "Count", std::to_string(script.properties.size()));
            ui.endTable();
        }
        if (!allow_structure_edit) {
            ui.beginDisabled();
        }
        if (fullWidthButton(ui, std::string("Sync Properties From Script") + script_suffix)) {
            const luna::editor::ScriptSchemaSyncResult sync_result = host.scripts().syncScriptProperties(script);
            if (sync_result.success) {
                script.properties = sync_result.properties;
                changed = true;
            } else if (!sync_result.message.empty()) {
                ui.textDisabled(sync_result.message);
            }
        }
        if (fullWidthButton(ui, std::string("Add Property") + script_suffix)) {
            luna::editor::SceneScriptProperty property{};
            property.name = makeUniquePropertyName(script, script.properties.size(), "Property");
            script.properties.push_back(std::move(property));
            changed = true;
        }
        if (!allow_structure_edit) {
            ui.endDisabled();
        }
        if (allow_structure_edit && normalizeScriptPropertyNames(script)) {
            changed = true;
        }

        for (std::size_t property_index = 0; property_index < script.properties.size(); ++property_index) {
            bool removed_property = false;
            bool property_value_changed = false;
            bool property_structure_changed = false;
            const bool property_changed = drawScriptProperty(host,
                                                             ui,
                                                             details,
                                                             script,
                                                             script_index,
                                                             property_index,
                                                             allow_structure_edit,
                                                             allow_property_value_edit,
                                                             property_value_changed,
                                                             property_structure_changed,
                                                             removed_property);
            if (allow_structure_edit) {
                changed |= property_changed;
            } else if (property_value_changed && property_index < script.properties.size()) {
                (void) host.scene().setScriptProperty(details.id, script_index, property_index, script.properties[property_index]);
            }
            if (removed_property) {
                break;
            }
        }

        endInspectorSection(ui, script_section);
    }

    if (changed && !sameScript(script_component, *details.script)) {
        (void) host.scene().setScriptComponent(details.id, script_component);
    }

    endInspectorSection(ui, section);
}

void drawRelationship(luna::editor::Host& host, luna::editor::Ui& ui, const luna::editor::SceneEntityDetails& details)
{
    const InspectorSection section =
        beginInspectorSection(ui, "InspectorRelationship", "Relationship", false, "Remove Component");
    if (!section.open) {
        endInspectorSection(ui, section);
        return;
    }

    if (details.parent_id.isValid()) {
        const luna::editor::SceneEntityReference parent{
            .id = details.parent_id,
            .name = details.parent_name,
        };
        if (beginPropertyTable(ui, "RelationshipParent")) {
            beginPropertyRow(ui, "Parent");
            if (fullWidthButton(ui, scopedEntityLabel(parent, "ParentEntity"))) {
                host.selection().selectEntity(details.parent_id);
            }
            drawTextProperty(ui, "Parent UUID", details.parent_id.toString());
            ui.endTable();
        } else if (fullWidthButton(ui, scopedEntityLabel(parent, "ParentEntity"))) {
            host.selection().selectEntity(details.parent_id);
        }

        if (!host.scene().canEditScene()) {
            ui.beginDisabled();
        }
        if (fullWidthButton(ui, "Detach From Parent")) {
            (void) host.scene().reparentEntity(details.id, luna::editor::EntityId(0), true);
        }
        if (!host.scene().canEditScene()) {
            ui.endDisabled();
        }
    } else {
        if (beginPropertyTable(ui, "RelationshipParent")) {
            drawTextProperty(ui, "Parent", "None");
            ui.endTable();
        }
    }

    ui.separatorText("Children");
    if (details.children.empty()) {
        ui.textDisabled("No child entities.");
    } else {
        for (const luna::editor::SceneEntityReference& child : details.children) {
            if (ui.selectable(scopedEntityLabel(child, "ChildEntity"), false)) {
                host.selection().selectEntity(child.id);
            }
            ui.textDisabled(child.id.toString());
        }
    }
    endInspectorSection(ui, section);
}

} // namespace

namespace luna::editor {

class InspectorPlugin final : public Plugin {
public:
    [[nodiscard]] PluginDescriptor descriptor() const override
    {
        return PluginDescriptor{
            .id = kPluginId,
            .display_name = "Inspector",
            .version = "0.1.0",
        };
    }

    bool onLoad(Host& host) override
    {
        return host.windows().registerWindow(WindowDescriptor{
            .id = kWindowId,
            .title = "Inspector",
            .default_open = true,
            .default_size = Vec2{.x = 380.0f, .y = 520.0f},
            .draw =
                [](WindowDrawContext& context) {
                    drawInspectorWindow(context);
                },
        });
    }

    void onUnload(Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }

private:
    static void drawInspectorWindow(WindowDrawContext& context)
    {
        Host& host = context.host();
        Ui& ui = context.ui();
        const CompactInspectorStyleGuard compact_style_guard{};

        const EntityId selected_entity_id = host.selection().selectedEntityId();
        if (!selected_entity_id.isValid()) {
            ui.text("Select an entity to inspect.");
            return;
        }

        const std::optional<SceneEntityDetails> details = host.scene().entityDetails(selected_entity_id);
        if (!details) {
            host.selection().clearSelection();
            ui.text("Select an entity to inspect.");
            return;
        }

        if (!host.scene().canEditScene()) {
            ui.textDisabled("Runtime viewport is active; scene editing is disabled.");
        }

        drawEntityHeader(host, ui, *details);
        drawAddComponentActions(host, ui, *details);
        drawTransform(host, ui, *details);
        drawCamera(host, ui, *details);
        drawLight(host, ui, *details);
        drawMesh(host, ui, *details);
        drawScript(host, ui, *details);
        drawRelationship(host, ui, *details);
    }
};

std::unique_ptr<Plugin> createInspectorPlugin()
{
    return std::make_unique<InspectorPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kInspectorPluginRegistration{
    kPluginId,
    createInspectorPlugin,
};

} // namespace

} // namespace luna::editor
