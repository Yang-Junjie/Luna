#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "Asset/Editor/ImageLoader.h"
#include "Asset/Editor/ImporterManager.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "EditorApi/EditorApi.h"
#include "EditorAssetDragDrop.h"
#include "EditorStyle.h"
#include "Imgui/ImGuiContext.h"
#include "LunaEditorLayer.h"
#include "Project/ProjectManager.h"
#include "Renderer/Mesh.h"
#include "Scene/Scene.h"
#include "Script/ScriptAsset.h"
#include "Script/ScriptPluginManager.h"
#include "Shell/EditorShell.h"

#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <Device.h>
#include <Queue.h>

#include <cctype>
#include <cstddef>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

std::string toString(std::string_view value)
{
    return std::string(value.data(), value.size());
}

ImVec4 withAlpha(ImVec4 color, float alpha)
{
    color.w = alpha;
    return color;
}

bool pushButtonVariant(luna::editor::ButtonVariant variant)
{
    switch (variant) {
        case luna::editor::ButtonVariant::Primary:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.15f, 0.31f, 0.35f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.18f, 0.40f, 0.45f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.11f, 0.26f, 0.30f, 1.0f});
            return true;
        case luna::editor::ButtonVariant::Danger:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.32f, 0.16f, 0.17f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.46f, 0.20f, 0.22f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.26f, 0.12f, 0.14f, 1.0f});
            return true;
        case luna::editor::ButtonVariant::Subtle:
            ImGui::PushStyleColor(ImGuiCol_Button, withAlpha(ImGui::GetStyleColorVec4(ImGuiCol_Button), 0.55f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            return true;
        case luna::editor::ButtonVariant::Default:
            break;
    }

    return false;
}

std::vector<std::string> splitMenuPath(std::string_view path)
{
    std::vector<std::string> parts;
    size_t offset = 0;
    while (offset < path.size()) {
        const size_t separator = path.find('/', offset);
        const size_t end = separator == std::string_view::npos ? path.size() : separator;
        if (end > offset) {
            parts.emplace_back(path.substr(offset, end - offset));
        }
        if (separator == std::string_view::npos) {
            break;
        }
        offset = separator + 1;
    }
    return parts;
}

std::string joinMenuPath(const std::vector<std::string>& parts, size_t count)
{
    std::string result;
    const size_t part_count = (std::min) (count, parts.size());
    for (size_t index = 0; index < part_count; ++index) {
        if (!result.empty()) {
            result += '/';
        }
        result += parts[index];
    }
    return result;
}

std::string normalizeMenuPath(std::string_view path)
{
    const std::vector<std::string> parts = splitMenuPath(path);
    return joinMenuPath(parts, parts.size());
}

bool hasMenuPathPrefix(const std::vector<std::string>& path, const std::vector<std::string>& prefix)
{
    if (prefix.size() >= path.size()) {
        return false;
    }

    for (size_t index = 0; index < prefix.size(); ++index) {
        if (path[index] != prefix[index]) {
            return false;
        }
    }
    return true;
}

bool containsStringView(std::initializer_list<std::string_view> values, std::string_view needle)
{
    for (std::string_view value : values) {
        if (value == needle) {
            return true;
        }
    }
    return false;
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

ImGuiWindowFlags toImGuiWindowFlags(luna::editor::WindowFlags flags)
{
    ImGuiWindowFlags imgui_flags = ImGuiWindowFlags_None;
    if (luna::editor::hasWindowFlag(flags, luna::editor::WindowFlag::NoSavedSettings)) {
        imgui_flags |= ImGuiWindowFlags_NoSavedSettings;
    }
    if (luna::editor::hasWindowFlag(flags, luna::editor::WindowFlag::NoDocking)) {
        imgui_flags |= ImGuiWindowFlags_NoDocking;
    }
    return imgui_flags;
}

ImGuiTableFlags toImGuiTableFlags(luna::editor::TableFlags flags)
{
    ImGuiTableFlags imgui_flags = ImGuiTableFlags_None;
    if (luna::editor::hasTableFlag(flags, luna::editor::TableFlag::RowBg)) {
        imgui_flags |= ImGuiTableFlags_RowBg;
    }
    if (luna::editor::hasTableFlag(flags, luna::editor::TableFlag::BordersInnerH)) {
        imgui_flags |= ImGuiTableFlags_BordersInnerH;
    }
    if (luna::editor::hasTableFlag(flags, luna::editor::TableFlag::BordersInnerV)) {
        imgui_flags |= ImGuiTableFlags_BordersInnerV;
    }
    if (luna::editor::hasTableFlag(flags, luna::editor::TableFlag::SizingStretchProp)) {
        imgui_flags |= ImGuiTableFlags_SizingStretchProp;
    }
    if (luna::editor::hasTableFlag(flags, luna::editor::TableFlag::ScrollY)) {
        imgui_flags |= ImGuiTableFlags_ScrollY;
    }
    return imgui_flags;
}

ImGuiTableColumnFlags toImGuiTableColumnFlags(luna::editor::TableColumnFlags flags)
{
    ImGuiTableColumnFlags imgui_flags = ImGuiTableColumnFlags_None;
    if (luna::editor::hasTableColumnFlag(flags, luna::editor::TableColumnFlag::WidthFixed)) {
        imgui_flags |= ImGuiTableColumnFlags_WidthFixed;
    }
    if (luna::editor::hasTableColumnFlag(flags, luna::editor::TableColumnFlag::WidthStretch)) {
        imgui_flags |= ImGuiTableColumnFlags_WidthStretch;
    }
    return imgui_flags;
}

ImGuiTreeNodeFlags toImGuiTreeNodeFlags(luna::editor::TreeNodeFlags flags)
{
    ImGuiTreeNodeFlags imgui_flags = ImGuiTreeNodeFlags_None;
    if (luna::editor::hasTreeNodeFlag(flags, luna::editor::TreeNodeFlag::OpenOnArrow)) {
        imgui_flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    }
    if (luna::editor::hasTreeNodeFlag(flags, luna::editor::TreeNodeFlag::OpenOnDoubleClick)) {
        imgui_flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    }
    if (luna::editor::hasTreeNodeFlag(flags, luna::editor::TreeNodeFlag::SpanAvailWidth)) {
        imgui_flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
    }
    if (luna::editor::hasTreeNodeFlag(flags, luna::editor::TreeNodeFlag::Leaf)) {
        imgui_flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (luna::editor::hasTreeNodeFlag(flags, luna::editor::TreeNodeFlag::NoTreePushOnOpen)) {
        imgui_flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (luna::editor::hasTreeNodeFlag(flags, luna::editor::TreeNodeFlag::Selected)) {
        imgui_flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (luna::editor::hasTreeNodeFlag(flags, luna::editor::TreeNodeFlag::DefaultOpen)) {
        imgui_flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }
    if (luna::editor::hasTreeNodeFlag(flags, luna::editor::TreeNodeFlag::FramePadding)) {
        imgui_flags |= ImGuiTreeNodeFlags_FramePadding;
    }
    return imgui_flags;
}

ImGuiMouseButton toImGuiMouseButton(luna::editor::MouseButton button)
{
    switch (button) {
        case luna::editor::MouseButton::Left:
            return ImGuiMouseButton_Left;
        case luna::editor::MouseButton::Right:
            return ImGuiMouseButton_Right;
        case luna::editor::MouseButton::Middle:
            return ImGuiMouseButton_Middle;
    }
    return ImGuiMouseButton_Left;
}

ImTextureID toImGuiTextureId(luna::editor::TextureHandle texture_id) noexcept
{
    if constexpr (std::is_pointer_v<ImTextureID>) {
        return reinterpret_cast<ImTextureID>(texture_id);
    } else {
        return static_cast<ImTextureID>(texture_id);
    }
}

luna::editor::TextureHandle toEditorTextureHandle(ImTextureID texture_id) noexcept
{
    if constexpr (std::is_pointer_v<ImTextureID>) {
        return reinterpret_cast<luna::editor::TextureHandle>(texture_id);
    } else {
        return static_cast<luna::editor::TextureHandle>(texture_id);
    }
}

std::vector<char> makeTextEditBuffer(const std::string& value, std::size_t buffer_size)
{
    const std::size_t size = (std::max) (buffer_size, value.size() + 1);
    std::vector<char> buffer(size, '\0');
    const std::size_t copy_size = (std::min) (value.size(), buffer.size() - 1);
    std::copy_n(value.data(), copy_size, buffer.data());
    return buffer;
}

luna::editor::Vec3 toEditorVec3(const glm::vec3& value)
{
    return luna::editor::Vec3{.x = value.x, .y = value.y, .z = value.z};
}

glm::vec3 toEngineVec3(const luna::editor::Vec3& value)
{
    return glm::vec3{value.x, value.y, value.z};
}

luna::editor::SceneTransform toEditorSceneTransform(const luna::TransformComponent& transform)
{
    return luna::editor::SceneTransform{
        .translation = toEditorVec3(transform.translation),
        .rotation_degrees = toEditorVec3(glm::degrees(transform.rotation)),
        .scale = toEditorVec3(transform.scale),
    };
}

luna::TransformComponent toEngineTransform(const luna::editor::SceneTransform& transform)
{
    luna::TransformComponent result{};
    result.translation = toEngineVec3(transform.translation);
    result.setRotationEuler(glm::radians(toEngineVec3(transform.rotation_degrees)));
    result.scale = toEngineVec3(transform.scale);
    return result;
}

luna::editor::SceneCameraProjection toEditorCameraProjection(luna::Camera::ProjectionType projection) noexcept
{
    switch (projection) {
        case luna::Camera::ProjectionType::Perspective:
            return luna::editor::SceneCameraProjection::Perspective;
        case luna::Camera::ProjectionType::Orthographic:
            return luna::editor::SceneCameraProjection::Orthographic;
    }

    return luna::editor::SceneCameraProjection::Perspective;
}

luna::Camera::ProjectionType toEngineCameraProjection(luna::editor::SceneCameraProjection projection) noexcept
{
    switch (projection) {
        case luna::editor::SceneCameraProjection::Perspective:
            return luna::Camera::ProjectionType::Perspective;
        case luna::editor::SceneCameraProjection::Orthographic:
            return luna::Camera::ProjectionType::Orthographic;
    }

    return luna::Camera::ProjectionType::Perspective;
}

luna::editor::SceneCameraComponent toEditorCameraComponent(const luna::CameraComponent& camera_component)
{
    return luna::editor::SceneCameraComponent{
        .primary = camera_component.primary,
        .fixed_aspect_ratio = camera_component.fixedAspectRatio,
        .projection = toEditorCameraProjection(camera_component.projectionType),
        .perspective_vertical_fov_degrees = glm::degrees(camera_component.perspectiveVerticalFovRadians),
        .perspective_near = camera_component.perspectiveNear,
        .perspective_far = camera_component.perspectiveFar,
        .orthographic_size = camera_component.orthographicSize,
        .orthographic_near = camera_component.orthographicNear,
        .orthographic_far = camera_component.orthographicFar,
    };
}

luna::CameraComponent toEngineCameraComponent(const luna::editor::SceneCameraComponent& camera_component)
{
    luna::CameraComponent result{};
    result.primary = camera_component.primary;
    result.fixedAspectRatio = camera_component.fixed_aspect_ratio;
    result.projectionType = toEngineCameraProjection(camera_component.projection);
    result.perspectiveVerticalFovRadians = glm::radians(camera_component.perspective_vertical_fov_degrees);
    result.perspectiveNear = camera_component.perspective_near;
    result.perspectiveFar = camera_component.perspective_far;
    result.orthographicSize = camera_component.orthographic_size;
    result.orthographicNear = camera_component.orthographic_near;
    result.orthographicFar = camera_component.orthographic_far;
    return result;
}

luna::editor::SceneLightType toEditorLightType(luna::LightComponent::Type type) noexcept
{
    switch (type) {
        case luna::LightComponent::Type::Directional:
            return luna::editor::SceneLightType::Directional;
        case luna::LightComponent::Type::Point:
            return luna::editor::SceneLightType::Point;
        case luna::LightComponent::Type::Spot:
            return luna::editor::SceneLightType::Spot;
    }

    return luna::editor::SceneLightType::Directional;
}

luna::LightComponent::Type toEngineLightType(luna::editor::SceneLightType type) noexcept
{
    switch (type) {
        case luna::editor::SceneLightType::Directional:
            return luna::LightComponent::Type::Directional;
        case luna::editor::SceneLightType::Point:
            return luna::LightComponent::Type::Point;
        case luna::editor::SceneLightType::Spot:
            return luna::LightComponent::Type::Spot;
    }

    return luna::LightComponent::Type::Directional;
}

luna::editor::SceneLightComponent toEditorLightComponent(const luna::LightComponent& light_component)
{
    return luna::editor::SceneLightComponent{
        .type = toEditorLightType(light_component.type),
        .enabled = light_component.enabled,
        .color = toEditorVec3(light_component.color),
        .intensity = light_component.intensity,
        .range = light_component.range,
        .inner_cone_angle_degrees = glm::degrees(light_component.innerConeAngleRadians),
        .outer_cone_angle_degrees = glm::degrees(light_component.outerConeAngleRadians),
    };
}

luna::LightComponent toEngineLightComponent(const luna::editor::SceneLightComponent& light_component)
{
    luna::LightComponent result{};
    result.type = toEngineLightType(light_component.type);
    result.enabled = light_component.enabled;
    result.color = toEngineVec3(light_component.color);
    result.intensity = light_component.intensity;
    result.range = light_component.range;
    result.innerConeAngleRadians = glm::radians(light_component.inner_cone_angle_degrees);
    result.outerConeAngleRadians = glm::radians(light_component.outer_cone_angle_degrees);
    return result;
}

luna::editor::SceneMeshComponent toEditorMeshComponent(const luna::MeshComponent& mesh_component)
{
    return luna::editor::SceneMeshComponent{
        .mesh_handle = mesh_component.meshHandle,
        .first_submesh = mesh_component.firstSubmesh,
        .submesh_count = mesh_component.submeshCount,
        .submesh_materials = mesh_component.submeshMaterials,
    };
}

luna::MeshComponent toEngineMeshComponent(const luna::editor::SceneMeshComponent& mesh_component)
{
    luna::MeshComponent result{};
    result.meshHandle = mesh_component.mesh_handle;
    result.firstSubmesh = mesh_component.first_submesh;
    result.submeshCount = mesh_component.submesh_count;
    result.submeshMaterials = mesh_component.submesh_materials;
    return result;
}

luna::editor::SceneScriptPropertyType toEditorScriptPropertyType(luna::ScriptPropertyType type) noexcept
{
    switch (type) {
        case luna::ScriptPropertyType::Bool:
            return luna::editor::SceneScriptPropertyType::Bool;
        case luna::ScriptPropertyType::Int:
            return luna::editor::SceneScriptPropertyType::Int;
        case luna::ScriptPropertyType::Float:
            return luna::editor::SceneScriptPropertyType::Float;
        case luna::ScriptPropertyType::String:
            return luna::editor::SceneScriptPropertyType::String;
        case luna::ScriptPropertyType::Vec3:
            return luna::editor::SceneScriptPropertyType::Vec3;
        case luna::ScriptPropertyType::Entity:
            return luna::editor::SceneScriptPropertyType::Entity;
        case luna::ScriptPropertyType::Asset:
            return luna::editor::SceneScriptPropertyType::Asset;
    }

    return luna::editor::SceneScriptPropertyType::Float;
}

luna::ScriptPropertyType toEngineScriptPropertyType(luna::editor::SceneScriptPropertyType type) noexcept
{
    switch (type) {
        case luna::editor::SceneScriptPropertyType::Bool:
            return luna::ScriptPropertyType::Bool;
        case luna::editor::SceneScriptPropertyType::Int:
            return luna::ScriptPropertyType::Int;
        case luna::editor::SceneScriptPropertyType::Float:
            return luna::ScriptPropertyType::Float;
        case luna::editor::SceneScriptPropertyType::String:
            return luna::ScriptPropertyType::String;
        case luna::editor::SceneScriptPropertyType::Vec3:
            return luna::ScriptPropertyType::Vec3;
        case luna::editor::SceneScriptPropertyType::Entity:
            return luna::ScriptPropertyType::Entity;
        case luna::editor::SceneScriptPropertyType::Asset:
            return luna::ScriptPropertyType::Asset;
    }

    return luna::ScriptPropertyType::Float;
}

luna::editor::SceneScriptPropertyMetadata toEditorScriptPropertyMetadata(const luna::ScriptPropertyMetadata& metadata)
{
    luna::editor::SceneScriptPropertyMetadata result{
        .display_name = metadata.displayName,
        .description = metadata.description,
        .category = metadata.category,
        .has_min_value = metadata.hasMinValue,
        .has_max_value = metadata.hasMaxValue,
        .has_step_value = metadata.hasStepValue,
        .min_value = metadata.minValue,
        .max_value = metadata.maxValue,
        .step_value = metadata.stepValue,
        .asset_type = metadata.assetType,
        .entity_filter = metadata.entityFilter,
    };
    result.options.reserve(metadata.options.size());
    for (const luna::ScriptPropertyOption& option : metadata.options) {
        result.options.push_back(luna::editor::SceneScriptPropertyOption{
            .label = option.label,
            .int_value = option.intValue,
            .string_value = option.stringValue,
        });
    }
    return result;
}

luna::ScriptPropertyMetadata toEngineScriptPropertyMetadata(const luna::editor::SceneScriptPropertyMetadata& metadata)
{
    luna::ScriptPropertyMetadata result{
        .displayName = metadata.display_name,
        .description = metadata.description,
        .category = metadata.category,
        .hasMinValue = metadata.has_min_value,
        .hasMaxValue = metadata.has_max_value,
        .hasStepValue = metadata.has_step_value,
        .minValue = metadata.min_value,
        .maxValue = metadata.max_value,
        .stepValue = metadata.step_value,
        .assetType = metadata.asset_type,
        .entityFilter = metadata.entity_filter,
    };
    result.options.reserve(metadata.options.size());
    for (const luna::editor::SceneScriptPropertyOption& option : metadata.options) {
        result.options.push_back(luna::ScriptPropertyOption{
            .label = option.label,
            .intValue = option.int_value,
            .stringValue = option.string_value,
        });
    }
    return result;
}

luna::editor::SceneScriptProperty toEditorScriptProperty(const luna::ScriptProperty& property)
{
    return luna::editor::SceneScriptProperty{
        .name = property.name,
        .type = toEditorScriptPropertyType(property.type),
        .bool_value = property.boolValue,
        .int_value = property.intValue,
        .float_value = property.floatValue,
        .string_value = property.stringValue,
        .vec3_value = toEditorVec3(property.vec3Value),
        .entity_value = property.entityValue,
        .asset_value = property.assetValue,
        .metadata = toEditorScriptPropertyMetadata(property.metadata),
    };
}

luna::ScriptProperty toEngineScriptProperty(const luna::editor::SceneScriptProperty& property)
{
    luna::ScriptProperty result{
        .name = property.name,
        .type = toEngineScriptPropertyType(property.type),
        .boolValue = property.bool_value,
        .intValue = property.int_value,
        .floatValue = property.float_value,
        .stringValue = property.string_value,
        .vec3Value = toEngineVec3(property.vec3_value),
        .entityValue = property.entity_value,
        .assetValue = property.asset_value,
        .metadata = toEngineScriptPropertyMetadata(property.metadata),
    };
    return result;
}

luna::editor::SceneScriptComponent toEditorScriptComponent(const luna::ScriptComponent& script_component)
{
    luna::editor::SceneScriptComponent result{
        .enabled = script_component.enabled,
    };
    result.scripts.reserve(script_component.scripts.size());
    for (const luna::ScriptEntry& script : script_component.scripts) {
        luna::editor::SceneScriptEntry entry{
            .id = script.id,
            .enabled = script.enabled,
            .script_asset = script.scriptAsset,
            .type_name = script.typeName,
            .execution_order = script.executionOrder,
        };
        entry.properties.reserve(script.properties.size());
        for (const luna::ScriptProperty& property : script.properties) {
            entry.properties.push_back(toEditorScriptProperty(property));
        }
        result.scripts.push_back(std::move(entry));
    }
    return result;
}

luna::ScriptComponent toEngineScriptComponent(const luna::editor::SceneScriptComponent& script_component)
{
    luna::ScriptComponent result{
        .enabled = script_component.enabled,
    };
    result.scripts.reserve(script_component.scripts.size());
    for (const luna::editor::SceneScriptEntry& script : script_component.scripts) {
        luna::ScriptEntry entry{
            .id = script.id,
            .enabled = script.enabled,
            .scriptAsset = script.script_asset,
            .typeName = script.type_name,
            .executionOrder = script.execution_order,
        };
        entry.properties.reserve(script.properties.size());
        for (const luna::editor::SceneScriptProperty& property : script.properties) {
            entry.properties.push_back(toEngineScriptProperty(property));
        }
        result.scripts.push_back(std::move(entry));
    }
    return result;
}

void copySceneScriptPropertyValue(luna::editor::SceneScriptProperty& destination,
                                  const luna::editor::SceneScriptProperty& source)
{
    destination.bool_value = source.bool_value;
    destination.int_value = source.int_value;
    destination.float_value = source.float_value;
    destination.string_value = source.string_value;
    destination.vec3_value = source.vec3_value;
    destination.entity_value = source.entity_value;
    destination.asset_value = source.asset_value;
}

const luna::editor::SceneScriptProperty* findMatchingScriptProperty(const luna::editor::SceneScriptEntry& script,
                                                                    std::string_view name,
                                                                    luna::editor::SceneScriptPropertyType type)
{
    for (const luna::editor::SceneScriptProperty& property : script.properties) {
        if (property.type == type && equalsIgnoreCase(property.name, name)) {
            return &property;
        }
    }

    return nullptr;
}

std::string scriptAssetLanguage(const luna::AssetMetadata& metadata)
{
    return metadata.GetConfig<std::string>("Language", "");
}

bool toAuthoringComponentKind(luna::editor::SceneComponentKind component_kind,
                              luna::authoring::AuthoringComponentKind& out_component_kind) noexcept
{
    switch (component_kind) {
        case luna::editor::SceneComponentKind::Camera:
            out_component_kind = luna::authoring::AuthoringComponentKind::Camera;
            return true;
        case luna::editor::SceneComponentKind::Light:
            out_component_kind = luna::authoring::AuthoringComponentKind::Light;
            return true;
        case luna::editor::SceneComponentKind::Mesh:
            out_component_kind = luna::authoring::AuthoringComponentKind::Mesh;
            return true;
        case luna::editor::SceneComponentKind::Script:
            out_component_kind = luna::authoring::AuthoringComponentKind::Script;
            return true;
        case luna::editor::SceneComponentKind::Transform:
            break;
    }

    return false;
}

} // namespace

namespace luna::editor {

class EditorUi final : public Ui {
public:
    bool beginWindow(std::string_view id, std::string_view title, bool* open, WindowFlags flags) override
    {
        const std::string label = toString(title) + "###" + toString(id);
        return ImGui::Begin(label.c_str(), open, toImGuiWindowFlags(flags));
    }

    void endWindow() override
    {
        ImGui::End();
    }

    void text(std::string_view value) override
    {
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    }

    void textDisabled(std::string_view value) override
    {
        ImGui::TextDisabled("%.*s", static_cast<int>(value.size()), value.data());
    }

    void textWrapped(std::string_view value) override
    {
        ImGui::TextWrapped("%.*s", static_cast<int>(value.size()), value.data());
    }

    void bulletText(std::string_view value) override
    {
        ImGui::BulletText("%.*s", static_cast<int>(value.size()), value.data());
    }

    void separator() override
    {
        ImGui::Separator();
    }

    void separatorText(std::string_view label) override
    {
        const std::string label_string = toString(label);
        ImGui::SeparatorText(label_string.c_str());
    }

    void sameLine() override
    {
        ImGui::SameLine();
    }

    void spacing() override
    {
        ImGui::Spacing();
    }

    void indent(float width) override
    {
        ImGui::Indent(width > 0.0f ? scaleEditorUi(width) : width);
    }

    void unindent(float width) override
    {
        ImGui::Unindent(width > 0.0f ? scaleEditorUi(width) : width);
    }

    void beginDisabled() override
    {
        ImGui::BeginDisabled();
    }

    void endDisabled() override
    {
        ImGui::EndDisabled();
    }

    void setNextItemWidth(float width) override
    {
        ImGui::SetNextItemWidth(width > 0.0f ? scaleEditorUi(width) : width);
    }

    Vec2 contentRegionAvail() const noexcept override
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        return Vec2{.x = available.x, .y = available.y};
    }

    Vec2 windowFramebufferScale() const noexcept override
    {
        const ImGuiViewport* viewport = ImGui::GetWindowViewport();
        const ImVec2 scale = viewport != nullptr ? viewport->FramebufferScale : ImGui::GetIO().DisplayFramebufferScale;
        return Vec2{.x = scale.x, .y = scale.y};
    }

    bool button(std::string_view label, Vec2 size, ButtonVariant variant) override
    {
        const std::string label_string = toString(label);
        const bool pushed_colors = pushButtonVariant(variant);
        const bool pressed = ImGui::Button(label_string.c_str(), ImVec2{size.x, size.y});
        if (pushed_colors) {
            ImGui::PopStyleColor(3);
        }
        return pressed;
    }

    bool checkbox(std::string_view label, bool& value) override
    {
        const std::string label_string = toString(label);
        return ImGui::Checkbox(label_string.c_str(), &value);
    }

    bool colorEdit3(std::string_view label, Vec3& value) override
    {
        const std::string label_string = toString(label);
        float color[3]{value.x, value.y, value.z};
        const bool changed = ImGui::ColorEdit3(label_string.c_str(), color);
        if (changed) {
            value = Vec3{.x = color[0], .y = color[1], .z = color[2]};
        }
        return changed;
    }

    bool sliderInt(std::string_view label, int& value, int min_value, int max_value) override
    {
        const std::string label_string = toString(label);
        return ImGui::SliderInt(label_string.c_str(), &value, min_value, max_value);
    }

    bool sliderFloat(
        std::string_view label, float& value, float min_value, float max_value, std::string_view format) override
    {
        const std::string label_string = toString(label);
        const std::string format_string = toString(format);
        return ImGui::SliderFloat(label_string.c_str(), &value, min_value, max_value, format_string.c_str());
    }

    bool dragInt(std::string_view label, int& value, float speed, int min_value, int max_value) override
    {
        const std::string label_string = toString(label);
        return ImGui::DragInt(label_string.c_str(), &value, speed, min_value, max_value);
    }

    bool dragFloat(std::string_view label,
                   float& value,
                   float speed,
                   float min_value,
                   float max_value,
                   std::string_view format) override
    {
        const std::string label_string = toString(label);
        const std::string format_string = toString(format);
        return ImGui::DragFloat(label_string.c_str(), &value, speed, min_value, max_value, format_string.c_str());
    }

    bool dragFloat3(std::string_view label,
                    Vec3& value,
                    float speed,
                    float min_value,
                    float max_value,
                    std::string_view format) override
    {
        const std::string label_string = toString(label);
        const std::string format_string = toString(format);
        float vector[3]{value.x, value.y, value.z};
        const bool changed =
            ImGui::DragFloat3(label_string.c_str(), vector, speed, min_value, max_value, format_string.c_str());
        if (changed) {
            value = Vec3{.x = vector[0], .y = vector[1], .z = vector[2]};
        }
        return changed;
    }

    bool inputText(std::string_view label, std::string& value, std::size_t buffer_size) override
    {
        const std::string label_string = toString(label);
        std::vector<char> buffer = makeTextEditBuffer(value, buffer_size);
        const bool changed = ImGui::InputText(label_string.c_str(), buffer.data(), buffer.size());
        if (changed) {
            value = buffer.data();
        }
        return changed;
    }

    bool inputTextWithHint(std::string_view label,
                           std::string_view hint,
                           std::string& value,
                           std::size_t buffer_size) override
    {
        const std::string label_string = toString(label);
        const std::string hint_string = toString(hint);
        std::vector<char> buffer = makeTextEditBuffer(value, buffer_size);
        const bool changed = ImGui::InputTextWithHint(
            label_string.c_str(), hint_string.c_str(), buffer.data(), buffer.size());
        if (changed) {
            value = buffer.data();
        }
        return changed;
    }

    bool colorEdit4(std::string_view label, Vec4& value) override
    {
        const std::string label_string = toString(label);
        float color[4]{value.x, value.y, value.z, value.w};
        const bool changed = ImGui::ColorEdit4(label_string.c_str(), color);
        if (changed) {
            value = Vec4{.x = color[0], .y = color[1], .z = color[2], .w = color[3]};
        }
        return changed;
    }

    bool treeNode(std::string_view label) override
    {
        const std::string label_string = toString(label);
        return ImGui::TreeNode(label_string.c_str());
    }

    void treePop() override
    {
        ImGui::TreePop();
    }

    bool beginCombo(std::string_view label, std::string_view preview_value) override
    {
        const std::string label_string = toString(label);
        const std::string preview_string = toString(preview_value);
        return ImGui::BeginCombo(label_string.c_str(), preview_string.c_str());
    }

    void endCombo() override
    {
        ImGui::EndCombo();
    }

    bool selectable(std::string_view label, bool selected) override
    {
        const std::string label_string = toString(label);
        return ImGui::Selectable(label_string.c_str(), selected);
    }

    void setItemDefaultFocus() override
    {
        ImGui::SetItemDefaultFocus();
    }

    bool image(const TextureView& texture, Vec2 size) override
    {
        if (!texture.valid() || size.x <= 0.0f || size.y <= 0.0f) {
            return false;
        }

        const ImTextureID texture_id = toImGuiTextureId(texture.id);
        if (texture_id == 0) {
            return false;
        }

        const ImVec2 uv0(0.0f, texture.y_flip ? 1.0f : 0.0f);
        const ImVec2 uv1(1.0f, texture.y_flip ? 0.0f : 1.0f);
        ImGui::Image(texture_id, ImVec2{size.x, size.y}, uv0, uv1);
        return true;
    }

    bool isItemHovered() const noexcept override
    {
        return ImGui::IsItemHovered();
    }

    bool isItemClicked(MouseButton button) const noexcept override
    {
        const ImGuiID item_id = ImGui::GetItemID();
        if (item_id == 0 || !ImGui::IsItemHovered()) {
            return false;
        }

        return ImGui::IsMouseClicked(toImGuiMouseButton(button), ImGuiInputFlags_None, item_id);
    }

    bool isItemDoubleClicked(MouseButton button) const noexcept override
    {
        const ImGuiID item_id = ImGui::GetItemID();
        if (item_id == 0 || !ImGui::IsItemHovered()) {
            return false;
        }

        return ImGui::IsMouseDoubleClicked(toImGuiMouseButton(button), item_id);
    }

    bool isItemDeactivatedAfterEdit() const noexcept override
    {
        return ImGui::IsItemDeactivatedAfterEdit();
    }

    void setTooltip(std::string_view value) override
    {
        ImGui::SetTooltip("%.*s", static_cast<int>(value.size()), value.data());
    }

    bool invisibleButton(std::string_view id, Vec2 size) override
    {
        const std::string id_string = toString(id);
        return ImGui::InvisibleButton(id_string.c_str(), ImVec2{size.x, size.y});
    }

    bool treeNodeEx(std::string_view id, std::string_view label, TreeNodeFlags flags) override
    {
        const std::string scoped_label = toString(label) + "###" + toString(id);
        return ImGui::TreeNodeEx(scoped_label.c_str(), toImGuiTreeNodeFlags(flags));
    }

    bool beginSection(std::string_view id, std::string_view label, bool default_open) override
    {
        const std::string id_string = toString(id);
        const std::string label_string = toString(label);
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;
        if (default_open) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{scaleEditorUi(2.0f), scaleEditorUi(2.0f)});
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
        const bool open = ImGui::TreeNodeEx(id_string.c_str(), flags, "%s", label_string.c_str());
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        const ImVec2 item_min = ImGui::GetItemRectMin();
        const ImVec2 item_max = ImGui::GetItemRectMax();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const float accent_width = scaleEditorUi(3.0f);
        draw_list->AddRectFilled(item_min,
                                 ImVec2{item_min.x + accent_width, item_max.y},
                                 ImGui::GetColorU32(withAlpha(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), 0.72f)));
        draw_list->AddLine(ImVec2{item_min.x, item_max.y - 1.0f},
                           ImVec2{item_max.x, item_max.y - 1.0f},
                           ImGui::GetColorU32(ImGuiCol_Border));
        return open;
    }

    void endSection() override
    {
        ImGui::TreePop();
    }

    bool beginMenu(std::string_view label, bool enabled) override
    {
        const std::string label_string = toString(label);
        return ImGui::BeginMenu(label_string.c_str(), enabled);
    }

    void endMenu() override
    {
        ImGui::EndMenu();
    }

    bool menuItem(std::string_view label, bool selected, bool enabled) override
    {
        const std::string label_string = toString(label);
        return ImGui::MenuItem(label_string.c_str(), nullptr, selected, enabled);
    }

    void openPopup(std::string_view id) override
    {
        const std::string id_string = toString(id);
        ImGui::OpenPopup(id_string.c_str());
    }

    bool beginPopup(std::string_view id) override
    {
        const std::string id_string = toString(id);
        return ImGui::BeginPopup(id_string.c_str());
    }

    bool beginPopupContextItem(std::string_view id, MouseButton button) override
    {
        const std::string id_string = toString(id);
        ImGuiPopupFlags flags = ImGuiPopupFlags_None;
        if (button == MouseButton::Right) {
            flags = ImGuiPopupFlags_MouseButtonRight;
        } else if (button == MouseButton::Left) {
            flags = ImGuiPopupFlags_MouseButtonLeft;
        } else {
            flags = ImGuiPopupFlags_MouseButtonMiddle;
        }
        return ImGui::BeginPopupContextItem(id_string.empty() ? nullptr : id_string.c_str(), flags);
    }

    void closeCurrentPopup() override
    {
        ImGui::CloseCurrentPopup();
    }

    void endPopup() override
    {
        ImGui::EndPopup();
    }

    bool beginDragDropSource() override
    {
        return ImGui::BeginDragDropSource();
    }

    bool setDragDropPayload(std::string_view type, const void* data, std::size_t size) override
    {
        const std::string type_string = toString(type);
        return ImGui::SetDragDropPayload(type_string.c_str(), data, size);
    }

    void endDragDropSource() override
    {
        ImGui::EndDragDropSource();
    }

    bool beginDragDropTarget() override
    {
        return ImGui::BeginDragDropTarget();
    }

    bool acceptDragDropPayload(std::string_view type, void* out_data, std::size_t size) override
    {
        if (out_data == nullptr || size == 0u) {
            return false;
        }

        const std::string type_string = toString(type);
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type_string.c_str());
        if (payload == nullptr || payload->Data == nullptr || payload->DataSize != static_cast<int>(size)) {
            return false;
        }

        std::memcpy(out_data, payload->Data, size);
        return true;
    }

    bool acceptAssetDragDropPayload(AssetDropPayload& out_payload,
                                    const AssetType* accepted_types,
                                    std::size_t accepted_type_count) override
    {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(luna::editor::kAssetDragDropPayload);
        if (payload == nullptr || payload->Data == nullptr ||
            payload->DataSize != static_cast<int>(sizeof(luna::editor::AssetDragDropData))) {
            return false;
        }

        luna::editor::AssetDragDropData asset_payload =
            *static_cast<const luna::editor::AssetDragDropData*>(payload->Data);
        const AssetType asset_type = luna::editor::getAssetType(asset_payload);
        bool accepted = accepted_type_count == 0u;
        for (std::size_t index = 0; index < accepted_type_count && !accepted; ++index) {
            accepted = accepted_types != nullptr && accepted_types[index] == asset_type;
        }
        if (!accepted) {
            return false;
        }

        out_payload = AssetDropPayload{
            .handle = luna::editor::getAssetHandle(asset_payload),
            .type = asset_type,
        };
        return out_payload.valid();
    }

    void endDragDropTarget() override
    {
        ImGui::EndDragDropTarget();
    }

    float scale(float value) const noexcept override
    {
        return scaleEditorUi(value);
    }

    Vec2 scaled(Vec2 value) const noexcept override
    {
        return Vec2{
            .x = scaleEditorUi(value.x),
            .y = scaleEditorUi(value.y),
        };
    }

    bool beginTable(std::string_view id, int column_count, TableFlags flags, Vec2 outer_size) override
    {
        const std::string id_string = toString(id);
        const Vec2 scaled_outer_size = scaled(outer_size);
        return ImGui::BeginTable(id_string.c_str(),
                                 column_count,
                                 toImGuiTableFlags(flags),
                                 ImVec2{scaled_outer_size.x, scaled_outer_size.y});
    }

    void endTable() override
    {
        ImGui::EndTable();
    }

    void tableSetupColumn(std::string_view label, TableColumnFlags flags, float init_width_or_weight) override
    {
        const std::string label_string = toString(label);
        const float scaled_init_width_or_weight = hasTableColumnFlag(flags, TableColumnFlag::WidthFixed)
                                                      ? scaleEditorUi(init_width_or_weight)
                                                      : init_width_or_weight;
        ImGui::TableSetupColumn(label_string.c_str(), toImGuiTableColumnFlags(flags), scaled_init_width_or_weight);
    }

    void tableHeadersRow() override
    {
        ImGui::TableHeadersRow();
    }

    void tableNextRow() override
    {
        ImGui::TableNextRow();
    }

    bool tableNextColumn() override
    {
        return ImGui::TableNextColumn();
    }
};

class EditorAssetService final : public AssetService {
public:
    explicit EditorAssetService(Host& host)
        : m_host(&host)
    {}

    AssetInfo describeAsset(AssetHandle handle) const override
    {
        if (const std::optional<AssetInfo> info = assetInfo(handle)) {
            return *info;
        }

        if (!handle.isValid()) {
            return AssetInfo{
                .handle = handle,
                .label = "None",
                .detail = "No asset assigned",
            };
        }

        return AssetInfo{
            .handle = handle,
            .label = "Missing asset",
            .detail = "Referenced asset is not in the database",
        };
    }

    std::optional<AssetInfo> assetInfo(AssetHandle handle) const override
    {
        if (!handle.isValid()) {
            return std::nullopt;
        }

        if (BuiltinAssets::isBuiltinAsset(handle)) {
            const AssetType type = BuiltinAssets::isBuiltinMesh(handle)       ? AssetType::Mesh
                                   : BuiltinAssets::isBuiltinMaterial(handle) ? AssetType::Material
                                                                              : AssetType::None;
            return AssetInfo{
                .handle = handle,
                .type = type,
                .label = BuiltinAssets::getDisplayName(handle),
                .detail = std::string("Builtin ") + AssetUtils::AssetTypeToString(type),
                .exists = true,
                .builtin = true,
                .memory_only = true,
            };
        }

        if (!AssetDatabase::exists(handle)) {
            return std::nullopt;
        }

        return makeAssetInfo(AssetDatabase::getAssetMetadata(handle));
    }

    std::optional<AssetInfo> assetInfoByPath(const std::filesystem::path& path) const override
    {
        const AssetHandle handle = findAssetHandleByPath(path);
        return handle.isValid() ? assetInfo(handle) : std::nullopt;
    }

    std::vector<AssetInfo> listAssets(AssetType type_filter, bool include_builtin) const override
    {
        std::vector<AssetInfo> assets;
        const auto& database = AssetDatabase::getDatabase();
        assets.reserve(database.size());

        std::unordered_set<AssetHandle> seen_handles;
        seen_handles.reserve(database.size());
        for (const auto& [handle, metadata] : database) {
            if (type_filter != AssetType::None && metadata.Type != type_filter) {
                continue;
            }

            assets.push_back(makeAssetInfo(metadata));
            seen_handles.insert(handle);
        }

        if (include_builtin) {
            const auto append_builtin_assets = [&](AssetType type) {
                for (AssetInfo builtin_asset : builtinAssets(type)) {
                    if (type_filter != AssetType::None && builtin_asset.type != type_filter) {
                        continue;
                    }
                    if (seen_handles.insert(builtin_asset.handle).second) {
                        assets.push_back(std::move(builtin_asset));
                    }
                }
            };
            append_builtin_assets(AssetType::Mesh);
            append_builtin_assets(AssetType::Material);
        }

        std::sort(assets.begin(), assets.end(), [](const AssetInfo& lhs, const AssetInfo& rhs) {
            if (lhs.type != rhs.type) {
                return static_cast<int>(lhs.type) < static_cast<int>(rhs.type);
            }
            if (lhs.label != rhs.label) {
                return lhs.label < rhs.label;
            }
            return lhs.project_path.generic_string() < rhs.project_path.generic_string();
        });
        return assets;
    }

    std::vector<AssetInfo> builtinAssets(AssetType type) const override
    {
        std::vector<AssetInfo> assets;
        if (type == AssetType::Mesh) {
            const auto& builtin_meshes = BuiltinAssets::getBuiltinMeshes();
            assets.reserve(builtin_meshes.size());
            for (const BuiltinMeshDescriptor& mesh : builtin_meshes) {
                assets.push_back(describeAsset(mesh.Handle));
            }
        } else if (type == AssetType::Material) {
            const auto& builtin_materials = BuiltinAssets::getBuiltinMaterials();
            assets.reserve(builtin_materials.size());
            for (const BuiltinMaterialDescriptor& material : builtin_materials) {
                assets.push_back(describeAsset(material.Handle));
            }
        }
        return assets;
    }

    bool assetExists(AssetHandle handle) const override
    {
        return handle.isValid() && (BuiltinAssets::isBuiltinAsset(handle) || AssetDatabase::exists(handle));
    }

    bool assetPathExists(const std::filesystem::path& path) const override
    {
        const std::optional<std::filesystem::path> resolved_path = resolveProjectAssetPath(path);
        if (!resolved_path) {
            return false;
        }

        std::error_code ec;
        return std::filesystem::exists(*resolved_path, ec) && !ec;
    }

    AssetHandle findAssetHandleByPath(const std::filesystem::path& path) const override
    {
        const std::optional<std::filesystem::path> project_relative_path = makeProjectRelativeAssetPath(path);
        if (!project_relative_path) {
            return AssetHandle(0);
        }
        return AssetDatabase::findHandleByFilePath(*project_relative_path);
    }

    std::optional<std::filesystem::path> assetsRootPath() const override
    {
        const ProjectService* project_service = projectService();
        if (project_service == nullptr) {
            return std::nullopt;
        }

        const std::optional<std::filesystem::path> project_root = project_service->projectRootPath();
        const std::optional<ProjectInfo> project_info = project_service->projectInfo();
        if (!project_root || !project_info) {
            return std::nullopt;
        }

        return (*project_root / project_info->AssetsPath).lexically_normal();
    }

    std::optional<std::filesystem::path>
        resolveProjectAssetPath(const std::filesystem::path& project_relative_path) const override
    {
        const ProjectService* project_service = projectService();
        if (project_service == nullptr) {
            return std::nullopt;
        }

        const std::optional<std::filesystem::path> project_root = project_service->projectRootPath();
        if (!project_root) {
            return std::nullopt;
        }

        const std::optional<std::filesystem::path> normalized_relative_path =
            makeProjectRelativeAssetPath(project_relative_path);
        if (!normalized_relative_path) {
            return std::nullopt;
        }

        return (*project_root / *normalized_relative_path).lexically_normal();
    }

    std::optional<std::filesystem::path> makeProjectRelativeAssetPath(const std::filesystem::path& path) const override
    {
        if (path.empty()) {
            return std::nullopt;
        }

        std::filesystem::path normalized_path = path.lexically_normal();
        if (normalized_path.is_absolute()) {
            const ProjectService* project_service = projectService();
            if (project_service == nullptr) {
                return std::nullopt;
            }

            const std::optional<std::filesystem::path> project_root = project_service->projectRootPath();
            if (!project_root) {
                return std::nullopt;
            }

            std::error_code ec;
            normalized_path = std::filesystem::relative(normalized_path, *project_root, ec).lexically_normal();
            if (ec) {
                return std::nullopt;
            }
        }

        if (normalized_path.empty() || normalized_path.is_absolute()) {
            return std::nullopt;
        }

        const std::string normalized_string = normalized_path.generic_string();
        if (normalized_string == "." || normalized_string == ".." || normalized_string.starts_with("../")) {
            return std::nullopt;
        }

        return normalized_path;
    }

    AssetRefreshResult refreshAssets() override
    {
        const ProjectService* project_service = projectService();
        AssetRefreshResult result{
            .project_loaded = project_service != nullptr && project_service->hasProjectLoaded(),
            .revision = m_asset_revision,
        };
        if (!result.project_loaded) {
            result.message = "Cannot sync assets because no project is currently loaded.";
            LUNA_EDITOR_WARN("{}", result.message);
            return result;
        }

        const ImporterManager::ImportStats stats = ImporterManager::syncProjectAssets();
        ++m_asset_revision;
        result = makeRefreshResult(stats, m_asset_revision);
        logAssetRefreshStats(stats);
        return result;
    }

    uint64_t assetRevision() const noexcept override
    {
        return m_asset_revision;
    }

    bool isAssetLoading(AssetHandle handle) const override
    {
        return handle.isValid() && AssetManager::get().isAssetLoading(handle);
    }

    bool acceptsAssetType(AssetType type,
                          const AssetType* accepted_types,
                          std::size_t accepted_type_count) const override
    {
        if (accepted_type_count == 0u) {
            return true;
        }

        for (std::size_t index = 0; index < accepted_type_count; ++index) {
            if (accepted_types != nullptr && accepted_types[index] == type) {
                return true;
            }
        }

        return false;
    }

    std::optional<std::size_t> meshSubmeshCount(AssetHandle mesh_handle) const override
    {
        if (!mesh_handle.isValid()) {
            return std::nullopt;
        }

        const AssetInfo info = describeAsset(mesh_handle);
        if (info.type != AssetType::Mesh || !info.exists) {
            return std::nullopt;
        }

        const std::shared_ptr<Mesh> mesh = AssetManager::get().requestAssetAs<Mesh>(mesh_handle);
        if (!mesh || !mesh->isValid()) {
            return std::nullopt;
        }

        return mesh->getSubMeshes().size();
    }

    bool beginAssetDragDropSource(AssetHandle handle, std::string_view label = {}) override
    {
        const std::optional<AssetInfo> info = assetInfo(handle);
        if (!info || !info->exists || info->type == AssetType::None) {
            return false;
        }

        AssetMetadata metadata;
        metadata.Handle = info->handle;
        metadata.Type = info->type;
        metadata.Name = info->label;
        metadata.FilePath = info->project_path;
        metadata.MemoryOnly = info->memory_only;

        const std::string label_string = toString(label);
        return luna::editor::beginAssetDragDropSource(metadata, label_string.empty() ? nullptr : label_string.c_str());
    }

private:
    AssetInfo makeAssetInfo(const AssetMetadata& metadata) const
    {
        std::string label = metadata.Name;
        if (label.empty() && !metadata.FilePath.empty()) {
            label = metadata.FilePath.generic_string();
        }
        if (label.empty()) {
            label = "Unnamed Asset";
        }

        return AssetInfo{
            .handle = metadata.Handle,
            .type = metadata.Type,
            .label = std::move(label),
            .detail = AssetUtils::AssetTypeToString(metadata.Type),
            .exists = true,
            .builtin = metadata.MemoryOnly && BuiltinAssets::isBuiltinAsset(metadata.Handle),
            .loading = AssetManager::get().isAssetLoading(metadata.Handle),
            .memory_only = metadata.MemoryOnly,
            .project_path = metadata.FilePath.lexically_normal(),
            .absolute_path = resolveProjectAssetPath(metadata.FilePath).value_or(std::filesystem::path{}),
        };
    }

    static AssetRefreshResult makeRefreshResult(const ImporterManager::ImportStats& stats, uint64_t revision)
    {
        AssetRefreshResult result{
            .success = stats.failedAssets == 0 && stats.missingMetadataAfterSync == 0,
            .project_loaded = true,
            .revision = revision,
            .message = stats.failedAssets == 0 && stats.missingMetadataAfterSync == 0
                           ? "Asset sync completed."
                           : "Asset sync completed with errors.",
            .discovered_assets = stats.discoveredAssets,
            .imported_missing_assets = stats.importedMissingAssets,
            .loaded_existing_metadata = stats.loadedExistingMetadata,
            .rebuilt_metadata = stats.rebuiltMetadata,
            .unsupported_files_skipped = stats.unsupportedFilesSkipped,
            .failed_assets = stats.failedAssets,
            .missing_metadata_after_sync = stats.missingMetadataAfterSync,
            .script_files_skipped_no_plugin = stats.scriptFilesSkippedNoPlugin,
            .script_files_skipped_unsupported_language = stats.scriptFilesSkippedUnsupportedLanguage,
            .generated_model_files = stats.generatedModelFiles,
            .generated_model_metadata = stats.generatedModelMetadata,
            .generated_material_files = stats.generatedMaterialFiles,
            .generated_material_metadata = stats.generatedMaterialMetadata,
            .generated_texture_metadata = stats.generatedTextureMetadata,
            .failed_generated_model_assets = stats.failedGeneratedModelAssets,
        };
        return result;
    }

    static void logAssetRefreshStats(const ImporterManager::ImportStats& stats)
    {
        LUNA_EDITOR_INFO(
            "Project asset sync: discovered={}, imported_missing={}, loaded_existing={}, rebuilt={}, unsupported={}, "
            "script_skipped_no_plugin={}, script_skipped_unsupported_language={}, failed={}, missing_after_sync={}, "
            "generated_models={}, generated_model_meta={}, generated_materials={}, generated_material_meta={}, "
            "generated_texture_meta={}, generated_model_failures={}",
            stats.discoveredAssets,
            stats.importedMissingAssets,
            stats.loadedExistingMetadata,
            stats.rebuiltMetadata,
            stats.unsupportedFilesSkipped,
            stats.scriptFilesSkippedNoPlugin,
            stats.scriptFilesSkippedUnsupportedLanguage,
            stats.failedAssets,
            stats.missingMetadataAfterSync,
            stats.generatedModelFiles,
            stats.generatedModelMetadata,
            stats.generatedMaterialFiles,
            stats.generatedMaterialMetadata,
            stats.generatedTextureMetadata,
            stats.failedGeneratedModelAssets);
    }

private:
    const ProjectService* projectService() const
    {
        return m_host != nullptr ? &m_host->project() : nullptr;
    }

private:
    Host* m_host{nullptr};
    uint64_t m_asset_revision{0};
};

class EditorSceneService final : public SceneService {
public:
    explicit EditorSceneService(LunaEditorLayer& editor_layer)
        : m_editor_layer(&editor_layer)
    {}

    std::string sceneLabel() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getAssetLabel() : std::string{};
    }

    size_t entityCount() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getScene().entityManager().entityCount() : 0u;
    }

    bool canEditScene() const noexcept override
    {
        return m_editor_layer != nullptr && !m_editor_layer->isRuntimeViewportEnabled();
    }

    bool openSceneFile(const std::filesystem::path& scene_file_path) override
    {
        return m_editor_layer != nullptr && m_editor_layer->openSceneFile(scene_file_path);
    }

    std::vector<SceneEntityInfo> entityHierarchy() const override
    {
        if (m_editor_layer == nullptr) {
            return {};
        }

        EntityManager& entity_manager = m_editor_layer->getInspectionScene().entityManager();
        auto view = entity_manager.registry().view<TagComponent, RelationshipComponent>();
        std::vector<SceneEntityInfo> entities;
        entities.reserve(view.size_hint());

        for (const auto entity_handle : view) {
            Entity entity(entity_handle, &entity_manager);
            if (!entity) {
                continue;
            }

            entities.push_back(SceneEntityInfo{
                .id = entity.getUUID(),
                .parent_id = entity.getParentUUID(),
                .name = entity.hasComponent<TagComponent>() ? entity.getComponent<TagComponent>().tag
                                                            : std::string("Unnamed Entity"),
                .child_ids = entity.getChildren(),
            });
        }

        return entities;
    }

    bool entityExists(EntityId entity_id) const noexcept override
    {
        return findEntity(entity_id).isValid();
    }

    std::optional<SceneEntityDetails> entityDetails(EntityId entity_id) const override
    {
        Entity entity = findEntity(entity_id);
        if (!entity) {
            return std::nullopt;
        }

        SceneEntityDetails details{
            .id = entity.getUUID(),
            .parent_id = entity.getParentUUID(),
            .name = entity.hasComponent<TagComponent>() ? entity.getComponent<TagComponent>().tag
                                                        : std::string("Unnamed Entity"),
            .components =
                SceneEntityComponents{
                    .transform = entity.hasComponent<TransformComponent>(),
                    .camera = entity.hasComponent<CameraComponent>(),
                    .light = entity.hasComponent<LightComponent>(),
                    .mesh = entity.hasComponent<MeshComponent>(),
                    .script = entity.hasComponent<ScriptComponent>(),
                },
        };

        if (Entity parent = entity.getParent()) {
            details.parent_id = parent.getUUID();
            details.parent_name = parent.getName();
        }

        if (details.components.transform) {
            details.transform = toEditorSceneTransform(entity.getComponent<TransformComponent>());
        }
        if (details.components.camera) {
            details.camera = toEditorCameraComponent(entity.getComponent<CameraComponent>());
        }
        if (details.components.light) {
            details.light = toEditorLightComponent(entity.getComponent<LightComponent>());
        }
        if (details.components.mesh) {
            details.mesh = toEditorMeshComponent(entity.getComponent<MeshComponent>());
        }
        if (details.components.script) {
            details.script = toEditorScriptComponent(entity.getComponent<ScriptComponent>());
        }

        EntityManager* entity_manager = entity.getEntityManager();
        if (entity_manager != nullptr) {
            details.children.reserve(entity.getChildren().size());
            for (const UUID child_id : entity.getChildren()) {
                Entity child = entity_manager->findEntityByUUID(child_id);
                if (!child) {
                    continue;
                }

                details.children.push_back(SceneEntityReference{
                    .id = child.getUUID(),
                    .name = child.getName(),
                });
            }
        }

        return details;
    }

    bool isEntityDescendantOf(EntityId entity_id, EntityId potential_ancestor_id) const override
    {
        Entity entity = findEntity(entity_id);
        Entity potential_ancestor = findEntity(potential_ancestor_id);
        if (!entity || !potential_ancestor) {
            return false;
        }

        std::unordered_set<UUID> visited_entities;
        for (Entity current = entity.getParent(); current; current = current.getParent()) {
            if (!visited_entities.insert(current.getUUID()).second) {
                return false;
            }
            if (current == potential_ancestor) {
                return true;
            }
        }

        return false;
    }

    SceneEnvironmentSettings sceneEnvironmentSettings() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getScene().environmentSettings()
                                         : SceneEnvironmentSettings{};
    }

    SceneShadowSettings sceneShadowSettings() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getScene().shadowSettings() : SceneShadowSettings{};
    }

    bool setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings) override
    {
        return m_editor_layer != nullptr && m_editor_layer->setSceneEnvironmentSettings(settings);
    }

    bool setSceneShadowSettings(const SceneShadowSettings& settings) override
    {
        return m_editor_layer != nullptr && m_editor_layer->setSceneShadowSettings(settings);
    }

    EntityId createEntity(std::string name) override
    {
        return createEntity(SceneEntityCreateRequest{
            .kind = SceneEntityCreateKind::Empty,
            .name = std::move(name),
        });
    }

    EntityId createEntity(const SceneEntityCreateRequest& request) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return EntityId(0);
        }

        Entity parent = findEntity(request.parent_id);
        Entity entity;
        switch (request.kind) {
            case SceneEntityCreateKind::Empty:
                entity = m_editor_layer->createEntity(request.name.empty() ? std::string("Empty Entity") : request.name,
                                                      parent);
                break;
            case SceneEntityCreateKind::Camera:
                entity = m_editor_layer->createCameraEntity(parent);
                break;
            case SceneEntityCreateKind::DirectionalLight:
                entity = m_editor_layer->createDirectionalLightEntity(parent);
                break;
            case SceneEntityCreateKind::PointLight:
                entity = m_editor_layer->createPointLightEntity(parent);
                break;
            case SceneEntityCreateKind::SpotLight:
                entity = m_editor_layer->createSpotLightEntity(parent);
                break;
            case SceneEntityCreateKind::PrimitiveMesh:
                entity = m_editor_layer->createPrimitiveEntity(request.asset_handle, parent);
                break;
            case SceneEntityCreateKind::MeshAsset:
                entity = m_editor_layer->createEntityFromMeshAsset(request.asset_handle, parent);
                break;
            case SceneEntityCreateKind::ModelAsset:
                entity = m_editor_layer->createEntityFromModelAsset(request.asset_handle, parent);
                break;
        }

        return entity ? entity.getUUID() : EntityId(0);
    }

    bool destroyEntity(EntityId entity_id) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        if (!entity) {
            return false;
        }

        const EntityId selected_entity_id = m_editor_layer->getSelectedEntityId();
        const bool clear_selection =
            selected_entity_id.isValid() &&
            (selected_entity_id == entity_id || isEntityDescendantOf(selected_entity_id, entity_id));
        const bool destroyed = m_editor_layer->destroyEntity(entity);
        if (destroyed && clear_selection) {
            m_editor_layer->setSelectedEntityId(EntityId(0));
        }
        return destroyed;
    }

    bool reparentEntity(EntityId entity_id, EntityId new_parent_id, bool preserve_world_transform) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        Entity new_parent = findEntity(new_parent_id);
        if (!entity || (new_parent_id.isValid() && !new_parent)) {
            return false;
        }
        if (new_parent && (entity == new_parent || isEntityDescendantOf(new_parent_id, entity_id))) {
            return false;
        }

        return m_editor_layer->reparentEntity(entity, new_parent, preserve_world_transform);
    }

    bool setEntityName(EntityId entity_id, std::string name) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        return entity && m_editor_layer->setEntityName(entity, std::move(name));
    }

    bool setEntityTransform(EntityId entity_id, const SceneTransform& transform) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        return entity && entity.hasComponent<TransformComponent>() &&
               m_editor_layer->setEntityTransform(entity, toEngineTransform(transform));
    }

    bool setCameraComponent(EntityId entity_id, const SceneCameraComponent& camera_component) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        return entity && m_editor_layer->setCameraComponent(entity, toEngineCameraComponent(camera_component));
    }

    bool setLightComponent(EntityId entity_id, const SceneLightComponent& light_component) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        return entity && m_editor_layer->setLightComponent(entity, toEngineLightComponent(light_component));
    }

    bool setMeshComponent(EntityId entity_id, const SceneMeshComponent& mesh_component) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        return entity && m_editor_layer->setMeshComponent(entity, toEngineMeshComponent(mesh_component));
    }

    bool setScriptComponent(EntityId entity_id, const SceneScriptComponent& script_component) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        return entity && m_editor_layer->setScriptComponent(entity, toEngineScriptComponent(script_component));
    }

    bool setScriptProperty(EntityId entity_id,
                           std::size_t script_index,
                           std::size_t property_index,
                           const SceneScriptProperty& property) override
    {
        if (m_editor_layer == nullptr) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        if (!entity || !entity.hasComponent<ScriptComponent>()) {
            return false;
        }

        ScriptComponent& script_component = entity.getComponent<ScriptComponent>();
        if (script_index >= script_component.scripts.size() ||
            property_index >= script_component.scripts[script_index].properties.size()) {
            return false;
        }

        if (canEditScene()) {
            ScriptComponent updated_component = script_component;
            updated_component.scripts[script_index].properties[property_index] = toEngineScriptProperty(property);
            return m_editor_layer->setScriptComponent(entity, updated_component);
        }

        script_component.scripts[script_index].properties[property_index] = toEngineScriptProperty(property);
        m_editor_layer->patchRuntimeScriptProperty(entity_id, script_index, property_index);
        return true;
    }

    bool addComponent(EntityId entity_id, SceneComponentKind component_kind) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        authoring::AuthoringComponentKind authoring_component_kind{};
        if (!toAuthoringComponentKind(component_kind, authoring_component_kind)) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        return entity && m_editor_layer->addComponent(entity, authoring_component_kind);
    }

    bool removeComponent(EntityId entity_id, SceneComponentKind component_kind) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return false;
        }

        authoring::AuthoringComponentKind authoring_component_kind{};
        if (!toAuthoringComponentKind(component_kind, authoring_component_kind)) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        return entity && m_editor_layer->removeComponent(entity, authoring_component_kind);
    }

    bool applyMeshAssetToEntity(EntityId entity_id, AssetHandle mesh_handle) override
    {
        if (m_editor_layer == nullptr || !canEditScene() || !mesh_handle.isValid()) {
            return false;
        }

        Entity entity = findEntity(entity_id);
        if (!entity) {
            return false;
        }

        m_editor_layer->applyMeshAssetToEntity(entity, mesh_handle);
        return true;
    }

private:
    Entity findEntity(EntityId entity_id) const noexcept
    {
        if (m_editor_layer == nullptr || !entity_id.isValid()) {
            return {};
        }

        return m_editor_layer->getInspectionScene().entityManager().findEntityByUUID(entity_id);
    }

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

class EditorSelectionService final : public SelectionService {
public:
    explicit EditorSelectionService(LunaEditorLayer& editor_layer)
        : m_editor_layer(&editor_layer)
    {}

    EntityId selectedEntityId() const noexcept override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getSelectedEntityId() : EntityId(0);
    }

    void selectEntity(EntityId entity_id) override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->setSelectedEntityId(entity_id);
        }
    }

    void clearSelection() override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->setSelectedEntityId(EntityId(0));
        }
    }

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

class EditorRenderingService final : public RenderingService {
public:
    explicit EditorRenderingService(LunaEditorLayer& editor_layer)
        : m_editor_layer(&editor_layer)
    {}

    std::string backendName() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getRenderingBackendName() : std::string("Unknown");
    }

    RenderingBackendCapabilities backendCapabilities() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getRenderingBackendCapabilities()
                                         : RenderingBackendCapabilities{};
    }

    RenderGraphProfileSnapshot renderGraphProfile() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getRenderGraphProfileSnapshot()
                                         : RenderGraphProfileSnapshot{};
    }

    bool isRenderGraphProfilingEnabled() const noexcept override
    {
        return m_editor_layer != nullptr && m_editor_layer->isRenderGraphProfilingEnabled();
    }

    void setRenderGraphProfilingEnabled(bool enabled) override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->setRenderGraphProfilingEnabled(enabled);
        }
    }

    std::filesystem::path defaultRenderProfileExportPath(std::string_view backend_name) const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->defaultRenderProfileExportPath(backend_name)
                                         : std::filesystem::path{};
    }

    bool exportRenderGraphProfileChromeTraceJson(const RenderGraphProfileSnapshot& profile,
                                                 const std::filesystem::path& output_path,
                                                 std::string* error_message) const override
    {
        return m_editor_layer != nullptr
                   ? m_editor_layer->exportRenderGraphProfileChromeTraceJson(profile, output_path, error_message)
                   : false;
    }

    std::vector<RenderFeatureInfo> defaultRenderFeatureInfos() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getDefaultRenderFeatureInfos()
                                         : std::vector<RenderFeatureInfo>{};
    }

    std::vector<RenderFeatureParameterInfo> defaultRenderFeatureParameters(std::string_view feature_name) const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getDefaultRenderFeatureParameters(feature_name)
                                         : std::vector<RenderFeatureParameterInfo>{};
    }

    bool setDefaultRenderFeatureEnabled(std::string_view feature_name, bool enabled) override
    {
        return m_editor_layer != nullptr && m_editor_layer->setDefaultRenderFeatureEnabled(feature_name, enabled);
    }

    bool setDefaultRenderFeatureParameter(std::string_view feature_name,
                                          std::string_view parameter_name,
                                          const RenderFeatureParameterValue& value) override
    {
        return m_editor_layer != nullptr &&
               m_editor_layer->setDefaultRenderFeatureParameter(feature_name, parameter_name, value);
    }

    std::vector<RenderDebugViewModeInfo> renderDebugViewModes() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getRenderDebugViewModes()
                                         : std::vector<RenderDebugViewModeInfo>{};
    }

    RenderDebugViewMode renderDebugViewMode() const noexcept override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getRenderDebugViewMode() : RenderDebugViewMode::None;
    }

    void setRenderDebugViewMode(RenderDebugViewMode mode) override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->setRenderDebugViewMode(mode);
        }
    }

    float renderDebugVelocityScale() const noexcept override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getRenderDebugVelocityScale() : 0.0f;
    }

    void setRenderDebugVelocityScale(float scale) override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->setRenderDebugVelocityScale(scale);
        }
    }

    TextureView renderDebugTextureView() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getRenderDebugTextureView() : TextureView{};
    }

    float frameTimeMilliseconds() const noexcept override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getFrameTimeMilliseconds() : 0.0f;
    }

    float framesPerSecond() const noexcept override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getFramesPerSecond() : 0.0f;
    }

    UVec2 sceneOutputSize() const noexcept override
    {
        if (m_editor_layer == nullptr) {
            return {};
        }

        return UVec2{
            .x = m_editor_layer->getSceneOutputWidth(),
            .y = m_editor_layer->getSceneOutputHeight(),
        };
    }

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

class EditorViewportService final : public ViewportService {
public:
    explicit EditorViewportService(LunaEditorLayer& editor_layer)
        : m_editor_layer(&editor_layer)
    {}

    ViewportPresentation syncSceneViewport(UVec2 framebuffer_size) override
    {
        return m_editor_layer != nullptr ? m_editor_layer->syncSceneViewport(framebuffer_size.x, framebuffer_size.y)
                                         : ViewportPresentation{};
    }

    TextureView sceneTextureView() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getSceneTextureView() : TextureView{};
    }

    void drawDefaultSceneViewport(Ui& ui) override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->drawDefaultSceneViewport(ui);
        }
    }

    Vec3 editorCameraPosition() const noexcept override
    {
        if (m_editor_layer == nullptr) {
            return {};
        }

        const auto camera_position = m_editor_layer->getEditorCameraPosition();
        return Vec3{
            .x = camera_position[0],
            .y = camera_position[1],
            .z = camera_position[2],
        };
    }

    std::string gizmoOperationName() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getGizmoOperationName() : std::string("Unknown");
    }

    std::string gizmoModeName() const override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getGizmoModeName() : std::string("Unknown");
    }

    bool pickDebugVisualizationEnabled() const noexcept override
    {
        return m_editor_layer != nullptr && m_editor_layer->isPickDebugVisualizationEnabled();
    }

    void setPickDebugVisualizationEnabled(bool enabled) override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->setPickDebugVisualizationEnabled(enabled);
        }
    }

    bool editorGridEnabled() const noexcept override
    {
        return m_editor_layer != nullptr && m_editor_layer->isEditorGridEnabled();
    }

    void setEditorGridEnabled(bool enabled) override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->setEditorGridEnabled(enabled);
        }
    }

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

class EditorRuntimeViewportService final : public RuntimeViewportService {
public:
    explicit EditorRuntimeViewportService(LunaEditorLayer& editor_layer)
        : m_editor_layer(&editor_layer)
    {}

    bool isRuntimeViewportEnabled() const noexcept override
    {
        return m_editor_layer != nullptr && m_editor_layer->isRuntimeViewportEnabled();
    }

    bool isRuntimeViewportRequested() const noexcept override
    {
        return m_editor_layer != nullptr && m_editor_layer->isRuntimeViewportRequested();
    }

    void setRuntimeViewportRequested(bool enabled) override
    {
        if (m_editor_layer != nullptr) {
            m_editor_layer->setRuntimeViewportRequested(enabled);
        }
    }

    size_t runtimeEntityCount() const noexcept override
    {
        return m_editor_layer != nullptr ? m_editor_layer->getRuntimeEntityCount() : 0u;
    }

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

class EditorHistoryService final : public HistoryService {
public:
    explicit EditorHistoryService(LunaEditorLayer& editor_layer)
        : m_editor_layer(&editor_layer)
    {}

    bool canUndo() const noexcept override
    {
        return m_editor_layer != nullptr && m_editor_layer->canUndo();
    }

    bool canRedo() const noexcept override
    {
        return m_editor_layer != nullptr && m_editor_layer->canRedo();
    }

    bool undo() override
    {
        return m_editor_layer != nullptr && m_editor_layer->undoEditorCommand();
    }

    bool redo() override
    {
        return m_editor_layer != nullptr && m_editor_layer->redoEditorCommand();
    }

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

class EditorCommandService final : public CommandService {
public:
    explicit EditorCommandService(Host& host)
        : m_host(&host)
    {}

    bool registerCommand(CommandDescriptor descriptor) override
    {
        if (descriptor.id.empty() || !descriptor.execute) {
            return false;
        }

        const std::string id = descriptor.id;
        const bool inserted = m_order_by_id.find(id) == m_order_by_id.end();
        m_commands[id] = std::move(descriptor);
        if (inserted) {
            m_order_by_id.emplace(id, m_order.size());
            m_order.push_back(id);
        }
        return true;
    }

    void unregisterCommandsForOwner(std::string_view owner_id)
    {
        if (owner_id.empty()) {
            return;
        }

        const std::string owner_key = toString(owner_id);
        for (auto it = m_commands.begin(); it != m_commands.end();) {
            if (it->second.owner_id == owner_key) {
                it = m_commands.erase(it);
            } else {
                ++it;
            }
        }

        m_order.erase(std::remove_if(m_order.begin(),
                                     m_order.end(),
                                     [&](const std::string& id) {
                                         return m_commands.find(id) == m_commands.end();
                                     }),
                      m_order.end());
        rebuildOrderMap();
    }

    void unregisterCommand(std::string_view id) override
    {
        const std::string key = toString(id);
        m_commands.erase(key);
        const auto order_it = m_order_by_id.find(key);
        if (order_it == m_order_by_id.end()) {
            return;
        }

        m_order.erase(m_order.begin() + static_cast<std::ptrdiff_t>(order_it->second));
        rebuildOrderMap();
    }

    bool execute(std::string_view id) override
    {
        const auto it = m_commands.find(toString(id));
        if (it == m_commands.end() || !canExecute(id) || m_host == nullptr) {
            return false;
        }

        it->second.execute(*m_host);
        return true;
    }

    bool canExecute(std::string_view id) const override
    {
        const auto it = m_commands.find(toString(id));
        if (it == m_commands.end() || m_host == nullptr) {
            return false;
        }

        return !it->second.can_execute || it->second.can_execute(*m_host);
    }

    bool isChecked(std::string_view id) const override
    {
        const auto it = m_commands.find(toString(id));
        if (it == m_commands.end() || m_host == nullptr || !it->second.is_checked) {
            return false;
        }

        return it->second.is_checked(*m_host);
    }

    const CommandDescriptor* findCommand(std::string_view id) const
    {
        const auto it = m_commands.find(toString(id));
        return it != m_commands.end() ? &it->second : nullptr;
    }

private:
    void rebuildOrderMap()
    {
        m_order_by_id.clear();
        for (size_t index = 0; index < m_order.size(); ++index) {
            m_order_by_id.emplace(m_order[index], index);
        }
    }

private:
    Host* m_host{nullptr};
    std::unordered_map<std::string, CommandDescriptor> m_commands;
    std::vector<std::string> m_order;
    std::unordered_map<std::string, size_t> m_order_by_id;
};

class EditorMenuService final : public MenuService {
public:
    explicit EditorMenuService(EditorCommandService& command_service)
        : m_command_service(&command_service)
    {}

    bool addMenuItem(MenuItemDescriptor descriptor) override
    {
        if (descriptor.menu_path.empty() || descriptor.command_id.empty()) {
            return false;
        }

        descriptor.menu_path = normalizeMenuPath(descriptor.menu_path);
        if (descriptor.menu_path.empty()) {
            return false;
        }

        const auto existing = std::find_if(m_items.begin(), m_items.end(), [&](const MenuItemDescriptor& item) {
            return item.menu_path == descriptor.menu_path && item.command_id == descriptor.command_id;
        });
        if (existing != m_items.end()) {
            *existing = std::move(descriptor);
            return true;
        }

        m_items.push_back(std::move(descriptor));
        return true;
    }

    void removeMenuItem(std::string_view menu_path, std::string_view command_id) override
    {
        const std::string normalized_path = normalizeMenuPath(menu_path);
        const std::string command_key = toString(command_id);
        m_items.erase(std::remove_if(m_items.begin(),
                                     m_items.end(),
                                     [&](const MenuItemDescriptor& item) {
                                         return item.menu_path == normalized_path && item.command_id == command_key;
                                     }),
                      m_items.end());
    }

    void removeMenuItemsForCommand(std::string_view command_id) override
    {
        const std::string command_key = toString(command_id);
        m_items.erase(std::remove_if(m_items.begin(),
                                     m_items.end(),
                                     [&](const MenuItemDescriptor& item) {
                                         return item.command_id == command_key;
                                     }),
                      m_items.end());
    }

    void removeMenuItemsForOwner(std::string_view owner_id)
    {
        if (owner_id.empty()) {
            return;
        }

        const std::string owner_key = toString(owner_id);
        m_items.erase(std::remove_if(m_items.begin(),
                                     m_items.end(),
                                     [&](const MenuItemDescriptor& item) {
                                         return item.owner_id == owner_key;
                                     }),
                      m_items.end());
    }

    void drawMenuItems(std::string_view menu_path)
    {
        const std::string normalized_path = normalizeMenuPath(menu_path);
        if (!normalized_path.empty()) {
            drawMenuContents(normalized_path);
        }
    }

    void drawMenuBarItems(std::initializer_list<std::string_view> handled_roots)
    {
        std::vector<std::string> roots;
        for (const MenuItemDescriptor& item : m_items) {
            const std::vector<std::string> parts = splitMenuPath(item.menu_path);
            if (parts.empty() || containsStringView(handled_roots, parts.front())) {
                continue;
            }

            if (std::find(roots.begin(), roots.end(), parts.front()) == roots.end()) {
                roots.push_back(parts.front());
            }
        }

        for (const std::string& root : roots) {
            if (ImGui::BeginMenu(root.c_str())) {
                drawMenuContents(root);
                ImGui::EndMenu();
            }
        }
    }

private:
    std::vector<std::string> directChildMenus(std::string_view menu_path) const
    {
        const std::vector<std::string> parent_parts = splitMenuPath(menu_path);
        std::vector<std::string> children;

        for (const MenuItemDescriptor& item : m_items) {
            const std::vector<std::string> item_parts = splitMenuPath(item.menu_path);
            if (!hasMenuPathPrefix(item_parts, parent_parts)) {
                continue;
            }

            const std::string& child = item_parts[parent_parts.size()];
            if (std::find(children.begin(), children.end(), child) == children.end()) {
                children.push_back(child);
            }
        }

        return children;
    }

    void drawMenuContents(std::string_view menu_path)
    {
        const std::string normalized_path = normalizeMenuPath(menu_path);

        for (const MenuItemDescriptor& item : m_items) {
            if (item.menu_path == normalized_path) {
                drawCommandMenuItem(item);
            }
        }

        for (const std::string& child : directChildMenus(normalized_path)) {
            const std::string child_path = normalized_path + "/" + child;
            if (ImGui::BeginMenu(child.c_str())) {
                drawMenuContents(child_path);
                ImGui::EndMenu();
            }
        }
    }

    void drawCommandMenuItem(const MenuItemDescriptor& item)
    {
        const CommandDescriptor* command =
            m_command_service != nullptr ? m_command_service->findCommand(item.command_id) : nullptr;

        std::string label = item.label;
        if (label.empty() && command != nullptr) {
            label = command->label;
        }
        if (label.empty()) {
            label = item.command_id;
        }

        std::string shortcut = item.shortcut;
        if (shortcut.empty() && command != nullptr) {
            shortcut = command->shortcut;
        }

        const bool enabled = m_command_service != nullptr && m_command_service->canExecute(item.command_id);
        const bool selected = m_command_service != nullptr && m_command_service->isChecked(item.command_id);
        if (ImGui::MenuItem(label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), selected, enabled) &&
            m_command_service != nullptr) {
            (void) m_command_service->execute(item.command_id);
        }
    }

private:
    EditorCommandService* m_command_service{nullptr};
    std::vector<MenuItemDescriptor> m_items;
};

class EditorPluginAssetService final : public PluginAssetService {
public:
    void registerPlugin(PluginDescriptor descriptor)
    {
        if (descriptor.id.empty()) {
            return;
        }

        if (descriptor.root_path.empty()) {
            return;
        }

        m_plugins[descriptor.id] = PluginEntry{
            .root_path = descriptor.root_path.lexically_normal(),
        };
    }

    void unregisterPlugin(std::string_view plugin_id)
    {
        const std::string id = toString(plugin_id);
        m_plugins.erase(id);

        for (auto it = m_textures.begin(); it != m_textures.end();) {
            if (it->second.plugin_id == id) {
                it = m_textures.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::optional<std::filesystem::path> pluginRootPath(std::string_view plugin_id) const override
    {
        const auto it = m_plugins.find(toString(plugin_id));
        if (it == m_plugins.end()) {
            return std::nullopt;
        }
        return it->second.root_path;
    }

    std::optional<std::filesystem::path> assetRootPath(std::string_view plugin_id) const override
    {
        const auto root_path = pluginRootPath(plugin_id);
        if (!root_path) {
            return std::nullopt;
        }
        return (*root_path / "assets").lexically_normal();
    }

    std::optional<std::filesystem::path>
        resolvePath(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) const override
    {
        const auto asset_root = assetRootPath(plugin_id);
        if (!asset_root) {
            return std::nullopt;
        }

        const auto safe_relative_path = normalizeRelativeAssetPath(relative_asset_path);
        if (!safe_relative_path) {
            return std::nullopt;
        }

        return (*asset_root / *safe_relative_path).lexically_normal();
    }

    bool exists(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) const override
    {
        const auto resolved_path = resolvePath(plugin_id, relative_asset_path);
        if (!resolved_path) {
            return false;
        }

        std::error_code ec;
        return std::filesystem::exists(*resolved_path, ec) && !ec;
    }

    std::optional<std::string> readText(std::string_view plugin_id,
                                        const std::filesystem::path& relative_asset_path) const override
    {
        const auto resolved_path = resolvePath(plugin_id, relative_asset_path);
        if (!resolved_path) {
            return std::nullopt;
        }

        std::ifstream stream(*resolved_path, std::ios::binary);
        if (!stream) {
            return std::nullopt;
        }

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    PluginAssetBytes readBytes(std::string_view plugin_id,
                               const std::filesystem::path& relative_asset_path) const override
    {
        const auto resolved_path = resolvePath(plugin_id, relative_asset_path);
        if (!resolved_path) {
            return {};
        }

        std::ifstream stream(*resolved_path, std::ios::binary | std::ios::ate);
        if (!stream) {
            return {};
        }

        const std::streamsize size = stream.tellg();
        if (size <= 0) {
            return {};
        }

        PluginAssetBytes result{};
        result.data.resize(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        if (!stream.read(reinterpret_cast<char*>(result.data.data()), size)) {
            return {};
        }
        return result;
    }

    TextureView texture(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) override
    {
        const std::string plugin_id_string = toString(plugin_id);
        const auto safe_relative_path = normalizeRelativeAssetPath(relative_asset_path);
        if (!safe_relative_path) {
            return {};
        }

        const std::string cache_key = plugin_id_string + ":" + safe_relative_path->generic_string();
        if (const auto it = m_textures.find(cache_key); it != m_textures.end()) {
            return it->second.view;
        }

        const auto resolved_path = resolvePath(plugin_id, *safe_relative_path);
        if (!resolved_path) {
            return {};
        }

        const ImageData image = ImageLoader::LoadImageFromFile(resolved_path->string());
        if (!image.isValid()) {
            LUNA_EDITOR_WARN("Plugin asset '{}' failed to load texture '{}'",
                             plugin_id_string,
                             resolved_path->string());
            return {};
        }

        const RHI::Ref<RHI::Texture> texture =
            uploadPluginTexture(image, plugin_id_string + "/" + safe_relative_path->generic_string());
        if (!texture) {
            return {};
        }

        const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(texture);
        if (texture_id == 0) {
            return {};
        }

        PluginTexture plugin_texture{
            .plugin_id = plugin_id_string,
            .texture = texture,
            .view =
                TextureView{
                    .id = toEditorTextureHandle(texture_id),
                    .size = UVec2{.x = image.Width, .y = image.Height},
                },
        };
        const TextureView view = plugin_texture.view;
        m_textures.emplace(cache_key, std::move(plugin_texture));
        return view;
    }

private:
    struct PluginEntry {
        std::filesystem::path root_path;
    };

    struct PluginTexture {
        std::string plugin_id;
        RHI::Ref<RHI::Texture> texture;
        TextureView view{};
    };

    static std::optional<std::filesystem::path> normalizeRelativeAssetPath(const std::filesystem::path& path)
    {
        if (path.empty() || path.is_absolute()) {
            return std::nullopt;
        }

        const std::filesystem::path normalized_path = path.lexically_normal();
        const std::string normalized_string = normalized_path.generic_string();
        if (normalized_string.empty() || normalized_string == "." || normalized_string == ".." ||
            normalized_string.starts_with("../")) {
            return std::nullopt;
        }

        return normalized_path;
    }

    static RHI::Ref<RHI::Texture> uploadPluginTexture(const ImageData& image, std::string_view debug_name)
    {
        auto& renderer = Application::get().getRenderer();
        const auto& device = renderer.getDevice();
        const auto& graphics_queue = renderer.getGraphicsQueue();
        if (!device || !graphics_queue) {
            return {};
        }

        constexpr uint32_t kTextureDataPitchAlignment = 256u;

        const uint64_t texel_count = static_cast<uint64_t>(image.Width) * static_cast<uint64_t>(image.Height);
        if (texel_count == 0 || image.ByteData.empty() || image.ByteData.size() % texel_count != 0) {
            return {};
        }

        const uint32_t bytes_per_pixel = static_cast<uint32_t>(image.ByteData.size() / texel_count);
        const uint32_t bytes_per_row = image.Width * bytes_per_pixel;
        const uint32_t aligned_bytes_per_row = static_cast<uint32_t>(
            ((bytes_per_row + kTextureDataPitchAlignment - 1u) / kTextureDataPitchAlignment) *
            kTextureDataPitchAlignment);
        const uint64_t upload_size = static_cast<uint64_t>(aligned_bytes_per_row) *
                                     static_cast<uint64_t>(image.Height);

        const std::string debug_name_string = toString(debug_name);
        const auto staging_buffer = device->CreateBuffer(RHI::BufferBuilder()
                                                             .SetSize(upload_size)
                                                             .SetUsage(RHI::BufferUsageFlags::TransferSrc)
                                                             .SetMemoryUsage(RHI::BufferMemoryUsage::CpuToGpu)
                                                             .SetName(debug_name_string + "_Upload")
                                                             .Build());
        if (!staging_buffer) {
            return {};
        }

        void* mapped = staging_buffer->Map();
        if (mapped == nullptr) {
            return {};
        }

        auto* destination = static_cast<uint8_t*>(mapped);
        const auto* source = image.ByteData.data();
        for (uint32_t row = 0; row < image.Height; ++row) {
            std::memcpy(destination + static_cast<uint64_t>(row) * aligned_bytes_per_row,
                        source + static_cast<uint64_t>(row) * bytes_per_row,
                        bytes_per_row);
        }
        staging_buffer->Flush(0, upload_size);
        staging_buffer->Unmap();

        const auto texture = device->CreateTexture(RHI::TextureBuilder()
                                                       .SetSize(image.Width, image.Height)
                                                       .SetFormat(image.ImageFormat)
                                                       .SetUsage(RHI::TextureUsageFlags::Sampled |
                                                                 RHI::TextureUsageFlags::TransferDst)
                                                       .SetInitialState(RHI::ResourceState::Undefined)
                                                       .SetName(debug_name_string)
                                                       .Build());
        if (!texture) {
            return {};
        }

        const auto upload_commands = device->CreateCommandBufferEncoder();
        if (!upload_commands) {
            return {};
        }

        const RHI::BufferImageCopy copy_region{
            .BufferOffset = 0,
            .BufferRowLength = bytes_per_pixel > 0 ? aligned_bytes_per_row / bytes_per_pixel : image.Width,
            .BufferImageHeight = 0,
            .ImageSubresource =
                {
                    .AspectMask = RHI::ImageAspectFlags::Color,
                    .MipLevel = 0,
                    .BaseArrayLayer = 0,
                    .LayerCount = 1,
                },
            .ImageOffsetX = 0,
            .ImageOffsetY = 0,
            .ImageOffsetZ = 0,
            .ImageExtentWidth = image.Width,
            .ImageExtentHeight = image.Height,
            .ImageExtentDepth = 1,
        };
        const std::array<RHI::BufferImageCopy, 1> copy_regions{copy_region};

        upload_commands->Begin();
        upload_commands->TransitionImage(texture, RHI::ImageTransition::UndefinedToTransferDst);
        upload_commands->CopyBufferToImage(staging_buffer, texture, RHI::ResourceState::CopyDest, copy_regions);
        upload_commands->TransitionImage(texture, RHI::ImageTransition::TransferDstToShaderRead);
        upload_commands->End();

        graphics_queue->Submit(upload_commands);
        graphics_queue->WaitIdle();
        upload_commands->ReturnToPool();
        return texture;
    }

private:
    std::unordered_map<std::string, PluginEntry> m_plugins;
    std::unordered_map<std::string, PluginTexture> m_textures;
};

class EditorProjectService final : public ProjectService {
public:
    bool hasProjectLoaded() const override
    {
        return ProjectManager::instance().getProjectRootPath().has_value() &&
               ProjectManager::instance().getProjectInfo().has_value();
    }

    std::optional<std::filesystem::path> projectRootPath() const override
    {
        return ProjectManager::instance().getProjectRootPath();
    }

    std::optional<ProjectInfo> projectInfo() const override
    {
        return ProjectManager::instance().getProjectInfo();
    }

    void setProjectInfo(const ProjectInfo& info) override
    {
        ProjectManager::instance().setProjectInfo(info);
    }

    bool saveProject() override
    {
        return ProjectManager::instance().saveProject();
    }
};

class EditorScriptPluginService final : public ScriptPluginService {
public:
    explicit EditorScriptPluginService(ProjectService& project_service)
        : m_project_service(&project_service)
    {}

    void refreshProjectScriptPlugins() override
    {
        refreshScriptPluginCandidates();

        const auto project_info = m_project_service != nullptr ? m_project_service->projectInfo() : std::nullopt;
        const ScriptPluginSelectionResult selection =
            ScriptPluginManager::instance().resolveProjectSelection(project_info ? &*project_info : nullptr);

        if (!selection.StatusMessage.empty()) {
            m_script_plugin_status = selection.StatusMessage;
        } else {
            m_script_plugin_status.clear();
        }

        if (selection.isResolved() && selection.Candidate != nullptr && project_info) {
            if (project_info->Scripting.SelectedPluginId != selection.Candidate->Manifest.PluginId ||
                project_info->Scripting.SelectedBackendName != selection.BackendName) {
                setProjectScriptPluginSelection(selection.Candidate, false);
            }
        }
    }

    const std::vector<ScriptPluginCandidate>& getDiscoveredScriptPlugins() const override
    {
        return m_script_plugin_candidates;
    }

    const std::string& getScriptPluginStatus() const override
    {
        return m_script_plugin_status;
    }

    const ScriptPluginCandidate* getSelectedScriptPluginCandidate() const override
    {
        const auto project_info = m_project_service != nullptr ? m_project_service->projectInfo() : std::nullopt;
        if (!project_info) {
            return nullptr;
        }

        const ScriptPluginSelectionResult selection =
            ScriptPluginManager::instance().resolveProjectSelection(&*project_info);
        return selection.Candidate;
    }

    bool selectScriptPlugin(const ScriptPluginCandidate* candidate) override
    {
        if (!setProjectScriptPluginSelection(candidate)) {
            return false;
        }

        const auto project_info = m_project_service != nullptr ? m_project_service->projectInfo() : std::nullopt;
        const ScriptPluginSelectionResult selection =
            ScriptPluginManager::instance().resolveProjectSelection(project_info ? &*project_info : nullptr);
        m_script_plugin_status = selection.StatusMessage;

        return true;
    }

private:
    void refreshScriptPluginCandidates()
    {
        const auto project_root = m_project_service != nullptr ? m_project_service->projectRootPath() : std::nullopt;
        ScriptPluginManager::instance().refreshDiscoveredPlugins(project_root);
        m_script_plugin_candidates = ScriptPluginManager::instance().getDiscoveredPlugins();

        if (!project_root) {
            m_script_plugin_status.clear();
            return;
        }

        if (m_script_plugin_candidates.empty()) {
            m_script_plugin_status = "No script plugins discovered.";
        } else if (m_script_plugin_candidates.size() == 1) {
            const auto& candidate = m_script_plugin_candidates.front();
            m_script_plugin_status = "Discovered 1 script plugin: " + candidate.Manifest.DisplayName + ".";
        } else {
            m_script_plugin_status =
                "Discovered " + std::to_string(m_script_plugin_candidates.size()) + " script plugins. Select one.";
        }
    }

    bool setProjectScriptPluginSelection(const ScriptPluginCandidate* candidate, bool log_changes = true)
    {
        const auto project_info = m_project_service != nullptr ? m_project_service->projectInfo() : std::nullopt;
        if (!project_info) {
            return false;
        }

        ProjectInfo updated_project_info = *project_info;
        const std::string selected_plugin_id = candidate != nullptr ? candidate->Manifest.PluginId : std::string{};
        const std::string selected_backend_name =
            candidate != nullptr ? candidate->Manifest.BackendName : std::string{};

        if (updated_project_info.Scripting.SelectedPluginId == selected_plugin_id &&
            updated_project_info.Scripting.SelectedBackendName == selected_backend_name) {
            return true;
        }

        updated_project_info.Scripting.SelectedPluginId = selected_plugin_id;
        updated_project_info.Scripting.SelectedBackendName = selected_backend_name;
        m_project_service->setProjectInfo(updated_project_info);

        if (!m_project_service->saveProject()) {
            if (log_changes) {
                LUNA_EDITOR_WARN("Failed to persist selected script plugin '{}'",
                                 candidate != nullptr ? candidate->Manifest.PluginId : std::string("<none>"));
            }
            return false;
        }

        if (log_changes) {
            if (candidate != nullptr) {
                LUNA_EDITOR_INFO(
                    "Selected script plugin '{}' ({})", candidate->Manifest.PluginId, candidate->Manifest.BackendName);
            } else {
                LUNA_EDITOR_INFO("Cleared project script plugin selection");
            }
        }

        return true;
    }

private:
    std::vector<ScriptPluginCandidate> m_script_plugin_candidates;
    std::string m_script_plugin_status;
    ProjectService* m_project_service{nullptr};
};

class EditorScriptService final : public ScriptService {
public:
    explicit EditorScriptService(ProjectService& project_service)
        : m_project_service(&project_service)
    {}

    ScriptLanguageStatus projectScriptLanguage() const override
    {
        const auto project_info = m_project_service != nullptr ? m_project_service->projectInfo() : std::nullopt;
        const ScriptPluginSelectionResult selection =
            ScriptPluginManager::instance().resolveProjectSelection(project_info ? &*project_info : nullptr);
        if (!selection.isResolved()) {
            return ScriptLanguageStatus{
                .message = selection.StatusMessage.empty() ? std::string("No usable script plugin is selected.")
                                                           : selection.StatusMessage,
            };
        }

        std::string language;
        if (const ScriptBackendDescriptor* backend = ScriptPluginManager::instance().findBackend(selection.BackendName);
            backend != nullptr) {
            language = backend->language;
        }
        if (language.empty() && selection.Candidate != nullptr) {
            language = selection.Candidate->Manifest.Language;
        }

        if (language.empty()) {
            return ScriptLanguageStatus{
                .message = "Selected script plugin does not declare a script language.",
            };
        }

        return ScriptLanguageStatus{
            .available = true,
            .language = std::move(language),
        };
    }

    ScriptAssetValidation validateScriptAsset(AssetHandle script_asset) const override
    {
        if (!script_asset.isValid()) {
            return ScriptAssetValidation{
                .accepted = true,
            };
        }

        if (!AssetDatabase::exists(script_asset)) {
            return ScriptAssetValidation{
                .message = "Script asset does not exist.",
            };
        }

        const AssetMetadata& metadata = AssetDatabase::getAssetMetadata(script_asset);
        if (metadata.Type != AssetType::Script) {
            return ScriptAssetValidation{
                .message = "Selected asset is not a Script asset.",
            };
        }

        const ScriptLanguageStatus language_status = projectScriptLanguage();
        if (!language_status.available) {
            return ScriptAssetValidation{
                .message = language_status.message.empty() ? std::string("No usable script plugin is selected.")
                                                           : language_status.message,
            };
        }

        const std::string metadata_language = scriptAssetLanguage(metadata);
        if (metadata_language.empty()) {
            return ScriptAssetValidation{
                .message = "Script asset metadata does not declare a script language.",
            };
        }

        if (!equalsIgnoreCase(metadata_language, language_status.language)) {
            return ScriptAssetValidation{
                .language = metadata_language,
                .message = "Script asset language '" + metadata_language +
                           "' does not match selected project script language '" + language_status.language + "'.",
            };
        }

        return ScriptAssetValidation{
            .accepted = true,
            .language = metadata_language,
        };
    }

    ScriptSchemaSyncResult syncScriptProperties(const SceneScriptEntry& script) const override
    {
        if (!script.script_asset.isValid()) {
            return ScriptSchemaSyncResult{
                .message = "Select a script asset before syncing properties.",
            };
        }

        const ScriptAssetValidation validation = validateScriptAsset(script.script_asset);
        if (!validation.accepted) {
            return ScriptSchemaSyncResult{
                .message = validation.message,
            };
        }

        if (!AssetDatabase::exists(script.script_asset)) {
            return ScriptSchemaSyncResult{
                .message = "Script asset does not exist.",
            };
        }

        const AssetMetadata& metadata = AssetDatabase::getAssetMetadata(script.script_asset);
        const std::shared_ptr<ScriptAsset> script_asset =
            AssetManager::get().loadAssetAs<ScriptAsset>(script.script_asset);
        if (!script_asset) {
            return ScriptSchemaSyncResult{
                .message = "Failed to load script asset.",
            };
        }

        ScriptSchemaRequest request{};
        request.assetName = !metadata.Name.empty() ? metadata.Name : metadata.FilePath.filename().string();
        request.typeName = script.type_name;
        request.language = script_asset->language;
        request.source = script_asset->source;

        const auto project_info = m_project_service != nullptr ? m_project_service->projectInfo() : std::nullopt;
        const std::vector<ScriptPropertySchema> schemas = ScriptPluginManager::instance().getPropertySchemaForProject(
            project_info ? &*project_info : nullptr, request);
        if (schemas.empty()) {
            return ScriptSchemaSyncResult{
                .message = "The selected script did not expose a Properties schema.",
            };
        }

        ScriptSchemaSyncResult result{
            .success = true,
        };
        result.properties.reserve(schemas.size());
        for (const ScriptPropertySchema& schema : schemas) {
            if (schema.name.empty()) {
                continue;
            }

            SceneScriptProperty property = toEditorScriptProperty(schema.defaultValue);
            property.name = schema.name;
            property.type = toEditorScriptPropertyType(schema.type);
            property.metadata = toEditorScriptPropertyMetadata(schema.metadata);

            if (const SceneScriptProperty* existing = findMatchingScriptProperty(script, schema.name, property.type)) {
                copySceneScriptPropertyValue(property, *existing);
            }

            result.properties.push_back(std::move(property));
        }

        if (result.properties.empty()) {
            result.success = false;
            result.message = "The selected script did not expose any usable Properties.";
        }

        return result;
    }

private:
    ProjectService* m_project_service{nullptr};
};

class EditorWindowService final : public WindowService {
public:
    EditorWindowService(Host& host, Ui& ui)
        : m_host(&host),
          m_ui(&ui)
    {}

    bool registerWindow(WindowDescriptor descriptor) override
    {
        if (descriptor.id.empty() || descriptor.title.empty() || !descriptor.draw) {
            return false;
        }

        const std::string id = descriptor.id;
        const bool inserted = m_order_by_id.find(id) == m_order_by_id.end();
        RegisteredWindow registered_window{};
        registered_window.descriptor = std::move(descriptor);
        registered_window.open = registered_window.descriptor.default_open;
        m_windows[id] = std::move(registered_window);

        if (inserted) {
            m_order_by_id.emplace(id, m_order.size());
            m_order.push_back(id);
        }
        return true;
    }

    void unregisterWindowsForOwner(std::string_view owner_id)
    {
        if (owner_id.empty()) {
            return;
        }

        const std::string owner_key = toString(owner_id);
        for (auto it = m_windows.begin(); it != m_windows.end();) {
            if (it->second.descriptor.owner_id == owner_key) {
                it = m_windows.erase(it);
            } else {
                ++it;
            }
        }

        m_order.erase(std::remove_if(m_order.begin(),
                                     m_order.end(),
                                     [&](const std::string& id) {
                                         return m_windows.find(id) == m_windows.end();
                                     }),
                      m_order.end());
        rebuildOrderMap();
    }

    void unregisterWindow(std::string_view id) override
    {
        const std::string key = toString(id);
        m_windows.erase(key);
        const auto order_it = m_order_by_id.find(key);
        if (order_it == m_order_by_id.end()) {
            return;
        }

        m_order.erase(m_order.begin() + static_cast<std::ptrdiff_t>(order_it->second));
        rebuildOrderMap();
    }

    bool isWindowOpen(std::string_view id) const override
    {
        const auto it = m_windows.find(toString(id));
        return it != m_windows.end() && it->second.open;
    }

    void setWindowOpen(std::string_view id, bool open) override
    {
        const auto it = m_windows.find(toString(id));
        if (it != m_windows.end()) {
            it->second.open = open;
        }
    }

    void drawWindowMenuItems()
    {
        for (const std::string& id : m_order) {
            auto it = m_windows.find(id);
            if (it == m_windows.end()) {
                continue;
            }

            bool open = it->second.open;
            if (ImGui::MenuItem(it->second.descriptor.title.c_str(), nullptr, &open)) {
                it->second.open = open;
            }
        }
    }

    void drawWindows()
    {
        if (m_host == nullptr || m_ui == nullptr) {
            return;
        }

        WindowDrawContext context(*m_host, *m_ui);
        for (const std::string& id : m_order) {
            auto it = m_windows.find(id);
            if (it == m_windows.end() || !it->second.open) {
                continue;
            }

            WindowDescriptor& descriptor = it->second.descriptor;
            if (descriptor.default_size.x > 0.0f || descriptor.default_size.y > 0.0f) {
                const Vec2 default_size = m_ui->scaled(descriptor.default_size);
                ImGui::SetNextWindowSize(ImVec2{default_size.x, default_size.y}, ImGuiCond_FirstUseEver);
            }

            const bool no_padding = hasWindowFlag(descriptor.flags, WindowFlag::NoPadding);
            if (no_padding) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
            }
            if (m_ui->beginWindow(descriptor.id, descriptor.title, &it->second.open, descriptor.flags)) {
                descriptor.draw(context);
            }
            m_ui->endWindow();
            if (no_padding) {
                ImGui::PopStyleVar();
            }
        }
    }

private:
    struct RegisteredWindow {
        WindowDescriptor descriptor;
        bool open{false};
    };

    void rebuildOrderMap()
    {
        m_order_by_id.clear();
        for (size_t index = 0; index < m_order.size(); ++index) {
            m_order_by_id.emplace(m_order[index], index);
        }
    }

private:
    Host* m_host{nullptr};
    Ui* m_ui{nullptr};
    std::unordered_map<std::string, RegisteredWindow> m_windows;
    std::vector<std::string> m_order;
    std::unordered_map<std::string, size_t> m_order_by_id;
};

struct EditorShell::Impl {
    Impl(EditorShell& shell, LunaEditorLayer& editor_layer)
        : ui(),
          asset_service(shell),
          project_service(),
          scene_service(editor_layer),
          selection_service(editor_layer),
          rendering_service(editor_layer),
          viewport_service(editor_layer),
          runtime_viewport_service(editor_layer),
          history_service(editor_layer),
          command_service(shell),
          menu_service(command_service),
          plugin_asset_service(),
          script_plugin_service(project_service),
          script_service(project_service),
          window_service(shell, ui)
    {}

    EditorUi ui;
    EditorAssetService asset_service;
    EditorProjectService project_service;
    EditorSceneService scene_service;
    EditorSelectionService selection_service;
    EditorRenderingService rendering_service;
    EditorViewportService viewport_service;
    EditorRuntimeViewportService runtime_viewport_service;
    EditorHistoryService history_service;
    EditorCommandService command_service;
    EditorMenuService menu_service;
    EditorPluginAssetService plugin_asset_service;
    EditorScriptPluginService script_plugin_service;
    EditorScriptService script_service;
    EditorWindowService window_service;
    std::vector<std::unique_ptr<Plugin>> plugins;
};

EditorShell::EditorShell(LunaEditorLayer& editor_layer)
    : m_impl(std::make_unique<Impl>(*this, editor_layer))
{}

EditorShell::~EditorShell()
{
    unloadPlugins();
}

Ui& EditorShell::ui()
{
    return m_impl->ui;
}

AssetService& EditorShell::assets()
{
    return m_impl->asset_service;
}

ProjectService& EditorShell::project()
{
    return m_impl->project_service;
}

WindowService& EditorShell::windows()
{
    return m_impl->window_service;
}

CommandService& EditorShell::commands()
{
    return m_impl->command_service;
}

HistoryService& EditorShell::history()
{
    return m_impl->history_service;
}

MenuService& EditorShell::menus()
{
    return m_impl->menu_service;
}

PluginAssetService& EditorShell::pluginAssets()
{
    return m_impl->plugin_asset_service;
}

ScriptPluginService& EditorShell::scriptPlugins()
{
    return m_impl->script_plugin_service;
}

ScriptService& EditorShell::scripts()
{
    return m_impl->script_service;
}

RenderingService& EditorShell::rendering()
{
    return m_impl->rendering_service;
}

SceneService& EditorShell::scene()
{
    return m_impl->scene_service;
}

SelectionService& EditorShell::selection()
{
    return m_impl->selection_service;
}

RuntimeViewportService& EditorShell::runtimeViewport()
{
    return m_impl->runtime_viewport_service;
}

ViewportService& EditorShell::viewport()
{
    return m_impl->viewport_service;
}

bool EditorShell::loadPlugin(std::unique_ptr<Plugin> plugin, const std::filesystem::path& root_path)
{
    if (!plugin) {
        return false;
    }

    PluginDescriptor descriptor = plugin->descriptor();
    if (descriptor.id.empty()) {
        LUNA_EDITOR_WARN("Ignoring editor plugin with empty id");
        return false;
    }
    if (!root_path.empty()) {
        descriptor.root_path = root_path;
    }

    m_impl->plugin_asset_service.registerPlugin(descriptor);
    if (!plugin->onLoad(*this)) {
        LUNA_EDITOR_WARN("Editor plugin '{}' failed to load", descriptor.id);
        plugin->onUnload(*this);
        m_impl->plugin_asset_service.unregisterPlugin(descriptor.id);
        return false;
    }

    LUNA_EDITOR_INFO("Loaded editor plugin '{}' ({})", descriptor.id, descriptor.display_name);
    m_impl->plugins.push_back(std::move(plugin));
    return true;
}

void EditorShell::unloadPlugins()
{
    for (auto it = m_impl->plugins.rbegin(); it != m_impl->plugins.rend(); ++it) {
        if (*it) {
            const PluginDescriptor descriptor = (*it)->descriptor();
            (*it)->onUnload(*this);
            m_impl->plugin_asset_service.unregisterPlugin(descriptor.id);
        }
    }
    m_impl->plugins.clear();
}

void EditorShell::registerPluginAssetRoot(std::string_view plugin_id, const std::filesystem::path& root_path)
{
    m_impl->plugin_asset_service.registerPlugin(PluginDescriptor{
        .id = toString(plugin_id),
        .root_path = root_path,
    });
}

void EditorShell::unregisterPluginAssetRoot(std::string_view plugin_id)
{
    m_impl->plugin_asset_service.unregisterPlugin(plugin_id);
}

void EditorShell::unregisterNativePluginContributions(std::string_view owner_id)
{
    m_impl->menu_service.removeMenuItemsForOwner(owner_id);
    m_impl->command_service.unregisterCommandsForOwner(owner_id);
    m_impl->window_service.unregisterWindowsForOwner(owner_id);
}

void EditorShell::update(float delta_seconds)
{
    for (const auto& plugin : m_impl->plugins) {
        if (plugin) {
            plugin->onUpdate(*this, delta_seconds);
        }
    }
}

void EditorShell::drawMenuItems(std::string_view menu_path)
{
    m_impl->menu_service.drawMenuItems(menu_path);
}

void EditorShell::drawMenuBarItems(std::initializer_list<std::string_view> handled_roots)
{
    m_impl->menu_service.drawMenuBarItems(handled_roots);
}

void EditorShell::drawWindowMenuItems()
{
    m_impl->window_service.drawWindowMenuItems();
}

void EditorShell::drawWindows()
{
    m_impl->window_service.drawWindows();
    for (const auto& plugin : m_impl->plugins) {
        if (plugin) {
            plugin->onDrawUi(*this, m_impl->ui);
        }
    }
}

} // namespace luna::editor
