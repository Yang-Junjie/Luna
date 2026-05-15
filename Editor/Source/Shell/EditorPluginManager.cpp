#include "Shell/EditorPluginManager.h"

#include "Core/Log.h"
#include "EditorApi/EditorAssetService.h"
#include "EditorApi/EditorCommandService.h"
#include "EditorApi/EditorMenuService.h"
#include "EditorApi/EditorPluginAssetService.h"
#include "EditorApi/EditorProjectService.h"
#include "EditorApi/EditorRuntimeViewportService.h"
#include "EditorApi/EditorSceneService.h"
#include "EditorApi/EditorSelectionService.h"
#include "EditorApi/EditorUi.h"
#include "EditorApi/EditorViewportService.h"
#include "EditorApi/EditorWindowService.h"
#include "Shell/EditorBuiltinPluginRegistry.h"
#include "Shell/EditorPluginDependencyResolver.h"
#include "Shell/EditorPluginManifest.h"
#include "Shell/EditorShell.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace {

} // namespace

namespace luna::editor {

struct NativePluginContext {
    EditorShell* shell{nullptr};
    std::string plugin_id;
    const LunaEditorHostApi* host_api{nullptr};
};

namespace {

std::string_view nativeString(const char* value) noexcept
{
    return value != nullptr ? std::string_view(value) : std::string_view{};
}

Vec2 toEditorVec2(const LunaEditorVec2* value) noexcept
{
    return value != nullptr ? Vec2{.x = value->x, .y = value->y} : Vec2{};
}

LunaEditorVec2 toNativeVec2(Vec2 value) noexcept
{
    return LunaEditorVec2{.x = value.x, .y = value.y};
}

Vec3 toEditorVec3(const LunaEditorVec3& value) noexcept
{
    return Vec3{.x = value.x, .y = value.y, .z = value.z};
}

LunaEditorVec3 toNativeVec3(Vec3 value) noexcept
{
    return LunaEditorVec3{.x = value.x, .y = value.y, .z = value.z};
}

Vec4 toEditorVec4(const LunaEditorVec4& value) noexcept
{
    return Vec4{.x = value.x, .y = value.y, .z = value.z, .w = value.w};
}

LunaEditorVec4 toNativeVec4(Vec4 value) noexcept
{
    return LunaEditorVec4{.x = value.x, .y = value.y, .z = value.z, .w = value.w};
}

TextureView toEditorTextureView(const LunaEditorTextureView& value) noexcept
{
    return TextureView{
        .id = value.texture_id,
        .size = UVec2{.x = value.width, .y = value.height},
        .y_flip = value.y_flip != 0,
    };
}

LunaEditorTextureView toNativeTextureView(TextureView value) noexcept
{
    return LunaEditorTextureView{
        .texture_id = value.id,
        .width = value.size.x,
        .height = value.size.y,
        .y_flip = value.y_flip ? 1 : 0,
    };
}

LunaEditorViewportPresentation toNativeViewportPresentation(ViewportPresentation value) noexcept
{
    return LunaEditorViewportPresentation{
        .struct_size = sizeof(LunaEditorViewportPresentation),
        .api_version = LUNA_EDITOR_VIEWPORT_API_VERSION,
        .scene_texture = toNativeTextureView(value.scene_texture),
        .framebuffer_width = value.framebuffer_size.x,
        .framebuffer_height = value.framebuffer_size.y,
        .presentable = value.presentable ? 1 : 0,
    };
}

AssetType toEditorAssetType(uint32_t value) noexcept
{
    switch (value) {
        case LunaEditorAssetType_Texture:
            return AssetType::Texture;
        case LunaEditorAssetType_Mesh:
            return AssetType::Mesh;
        case LunaEditorAssetType_Material:
            return AssetType::Material;
        case LunaEditorAssetType_Model:
            return AssetType::Model;
        case LunaEditorAssetType_Scene:
            return AssetType::Scene;
        case LunaEditorAssetType_Script:
            return AssetType::Script;
        case LunaEditorAssetType_None:
        default:
            return AssetType::None;
    }
}

uint32_t toNativeAssetType(AssetType value) noexcept
{
    switch (value) {
        case AssetType::Texture:
            return LunaEditorAssetType_Texture;
        case AssetType::Mesh:
            return LunaEditorAssetType_Mesh;
        case AssetType::Material:
            return LunaEditorAssetType_Material;
        case AssetType::Model:
            return LunaEditorAssetType_Model;
        case AssetType::Scene:
            return LunaEditorAssetType_Scene;
        case AssetType::Script:
            return LunaEditorAssetType_Script;
        case AssetType::None:
        default:
            return LunaEditorAssetType_None;
    }
}

SceneEntityCreateKind toEditorSceneEntityCreateKind(uint32_t value) noexcept
{
    switch (value) {
        case LunaEditorSceneEntityCreateKind_Camera:
            return SceneEntityCreateKind::Camera;
        case LunaEditorSceneEntityCreateKind_DirectionalLight:
            return SceneEntityCreateKind::DirectionalLight;
        case LunaEditorSceneEntityCreateKind_PointLight:
            return SceneEntityCreateKind::PointLight;
        case LunaEditorSceneEntityCreateKind_SpotLight:
            return SceneEntityCreateKind::SpotLight;
        case LunaEditorSceneEntityCreateKind_PrimitiveMesh:
            return SceneEntityCreateKind::PrimitiveMesh;
        case LunaEditorSceneEntityCreateKind_MeshAsset:
            return SceneEntityCreateKind::MeshAsset;
        case LunaEditorSceneEntityCreateKind_ModelAsset:
            return SceneEntityCreateKind::ModelAsset;
        case LunaEditorSceneEntityCreateKind_Empty:
        default:
            return SceneEntityCreateKind::Empty;
    }
}

SceneComponentKind toEditorSceneComponentKind(uint32_t value) noexcept
{
    switch (value) {
        case LunaEditorSceneComponentKind_Camera:
            return SceneComponentKind::Camera;
        case LunaEditorSceneComponentKind_Light:
            return SceneComponentKind::Light;
        case LunaEditorSceneComponentKind_Mesh:
            return SceneComponentKind::Mesh;
        case LunaEditorSceneComponentKind_Script:
            return SceneComponentKind::Script;
        case LunaEditorSceneComponentKind_Transform:
        default:
            return SceneComponentKind::Transform;
    }
}

uint32_t toNativeSceneEntityComponentFlags(const SceneEntityComponents& components) noexcept
{
    uint32_t flags = LunaEditorSceneEntityComponentFlag_None;
    if (components.transform) {
        flags |= LunaEditorSceneEntityComponentFlag_Transform;
    }
    if (components.camera) {
        flags |= LunaEditorSceneEntityComponentFlag_Camera;
    }
    if (components.light) {
        flags |= LunaEditorSceneEntityComponentFlag_Light;
    }
    if (components.mesh) {
        flags |= LunaEditorSceneEntityComponentFlag_Mesh;
    }
    if (components.script) {
        flags |= LunaEditorSceneEntityComponentFlag_Script;
    }
    return flags;
}

LunaEditorSceneTransform toNativeSceneTransform(const SceneTransform& transform) noexcept
{
    return LunaEditorSceneTransform{
        .translation = LunaEditorVec3{
            .x = transform.translation.x,
            .y = transform.translation.y,
            .z = transform.translation.z,
        },
        .rotation_degrees = LunaEditorVec3{
            .x = transform.rotation_degrees.x,
            .y = transform.rotation_degrees.y,
            .z = transform.rotation_degrees.z,
        },
        .scale = LunaEditorVec3{
            .x = transform.scale.x,
            .y = transform.scale.y,
            .z = transform.scale.z,
        },
    };
}

SceneTransform toEditorSceneTransform(const LunaEditorSceneTransform& transform) noexcept
{
    return SceneTransform{
        .translation = Vec3{
            .x = transform.translation.x,
            .y = transform.translation.y,
            .z = transform.translation.z,
        },
        .rotation_degrees = Vec3{
            .x = transform.rotation_degrees.x,
            .y = transform.rotation_degrees.y,
            .z = transform.rotation_degrees.z,
        },
        .scale = Vec3{
            .x = transform.scale.x,
            .y = transform.scale.y,
            .z = transform.scale.z,
        },
    };
}

uint32_t toNativeSceneCameraProjection(SceneCameraProjection value) noexcept
{
    switch (value) {
        case SceneCameraProjection::Orthographic:
            return 1u;
        case SceneCameraProjection::Perspective:
        default:
            return 0u;
    }
}

SceneCameraProjection toEditorSceneCameraProjection(uint32_t value) noexcept
{
    switch (value) {
        case 1u:
            return SceneCameraProjection::Orthographic;
        case 0u:
        default:
            return SceneCameraProjection::Perspective;
    }
}

uint32_t toNativeSceneLightType(SceneLightType value) noexcept
{
    switch (value) {
        case SceneLightType::Point:
            return 1u;
        case SceneLightType::Spot:
            return 2u;
        case SceneLightType::Directional:
        default:
            return 0u;
    }
}

SceneLightType toEditorSceneLightType(uint32_t value) noexcept
{
    switch (value) {
        case 1u:
            return SceneLightType::Point;
        case 2u:
            return SceneLightType::Spot;
        case 0u:
        default:
            return SceneLightType::Directional;
    }
}

bool hasNativeSceneCameraComponentLayout(const LunaEditorSceneCameraComponent* out_component) noexcept
{
    return out_component != nullptr && out_component->api_version == 1u &&
           out_component->struct_size >= sizeof(LunaEditorSceneCameraComponent);
}

bool hasNativeSceneLightComponentLayout(const LunaEditorSceneLightComponent* out_component) noexcept
{
    return out_component != nullptr && out_component->api_version == 1u &&
           out_component->struct_size >= sizeof(LunaEditorSceneLightComponent);
}

bool hasNativeSceneMeshComponentLayout(const LunaEditorSceneMeshComponent* out_component) noexcept
{
    return out_component != nullptr && out_component->api_version == 1u &&
           out_component->struct_size >= sizeof(LunaEditorSceneMeshComponent);
}

LunaEditorSceneCameraComponent toNativeSceneCameraComponent(const SceneCameraComponent& component) noexcept
{
    return LunaEditorSceneCameraComponent{
        .struct_size = sizeof(LunaEditorSceneCameraComponent),
        .api_version = 1u,
        .primary = component.primary ? 1 : 0,
        .fixed_aspect_ratio = component.fixed_aspect_ratio ? 1 : 0,
        .projection = toNativeSceneCameraProjection(component.projection),
        .perspective_vertical_fov_degrees = component.perspective_vertical_fov_degrees,
        .perspective_near = component.perspective_near,
        .perspective_far = component.perspective_far,
        .orthographic_size = component.orthographic_size,
        .orthographic_near = component.orthographic_near,
        .orthographic_far = component.orthographic_far,
    };
}

SceneCameraComponent toEditorSceneCameraComponent(const LunaEditorSceneCameraComponent& component) noexcept
{
    return SceneCameraComponent{
        .primary = component.primary != 0,
        .fixed_aspect_ratio = component.fixed_aspect_ratio != 0,
        .projection = toEditorSceneCameraProjection(component.projection),
        .perspective_vertical_fov_degrees = component.perspective_vertical_fov_degrees,
        .perspective_near = component.perspective_near,
        .perspective_far = component.perspective_far,
        .orthographic_size = component.orthographic_size,
        .orthographic_near = component.orthographic_near,
        .orthographic_far = component.orthographic_far,
    };
}

LunaEditorSceneLightComponent toNativeSceneLightComponent(const SceneLightComponent& component) noexcept
{
    return LunaEditorSceneLightComponent{
        .struct_size = sizeof(LunaEditorSceneLightComponent),
        .api_version = 1u,
        .type = toNativeSceneLightType(component.type),
        .enabled = component.enabled ? 1 : 0,
        .color = LunaEditorVec3{.x = component.color.x, .y = component.color.y, .z = component.color.z},
        .intensity = component.intensity,
        .range = component.range,
        .inner_cone_angle_degrees = component.inner_cone_angle_degrees,
        .outer_cone_angle_degrees = component.outer_cone_angle_degrees,
    };
}

SceneLightComponent toEditorSceneLightComponent(const LunaEditorSceneLightComponent& component) noexcept
{
    return SceneLightComponent{
        .type = toEditorSceneLightType(component.type),
        .enabled = component.enabled != 0,
        .color = Vec3{.x = component.color.x, .y = component.color.y, .z = component.color.z},
        .intensity = component.intensity,
        .range = component.range,
        .inner_cone_angle_degrees = component.inner_cone_angle_degrees,
        .outer_cone_angle_degrees = component.outer_cone_angle_degrees,
    };
}

bool fillNativeSceneMeshComponent(const SceneMeshComponent& component, LunaEditorSceneMeshComponent* out_component)
{
    if (!hasNativeSceneMeshComponentLayout(out_component)) {
        return false;
    }

    out_component->mesh_handle = static_cast<uint64_t>(component.mesh_handle);
    out_component->first_submesh = component.first_submesh;
    out_component->submesh_count = component.submesh_count;
    out_component->submesh_material_count = component.submesh_materials.size();

    const size_t copy_count = (std::min)(out_component->submesh_material_capacity, component.submesh_materials.size());
    if (out_component->submesh_material_handles != nullptr && copy_count > 0u) {
        for (size_t index = 0; index < copy_count; ++index) {
            out_component->submesh_material_handles[index] = static_cast<uint64_t>(component.submesh_materials[index]);
        }
    }
    return copy_count == component.submesh_materials.size();
}

SceneMeshComponent toEditorSceneMeshComponent(const LunaEditorSceneMeshComponent& component)
{
    SceneMeshComponent result{};
    result.mesh_handle = AssetHandle(component.mesh_handle);
    result.first_submesh = component.first_submesh;
    result.submesh_count = component.submesh_count;
    result.submesh_materials.reserve(component.submesh_material_capacity);
    for (size_t index = 0; index < component.submesh_material_capacity && component.submesh_material_handles != nullptr;
         ++index) {
        result.submesh_materials.push_back(AssetHandle(component.submesh_material_handles[index]));
    }
    return result;
}

ButtonVariant toEditorButtonVariant(uint32_t value) noexcept
{
    switch (value) {
        case LunaEditorButtonVariant_Primary:
            return ButtonVariant::Primary;
        case LunaEditorButtonVariant_Danger:
            return ButtonVariant::Danger;
        case LunaEditorButtonVariant_Subtle:
            return ButtonVariant::Subtle;
        case LunaEditorButtonVariant_Default:
        default:
            return ButtonVariant::Default;
    }
}

MouseButton toEditorMouseButton(int button) noexcept
{
    switch (button) {
        case LunaEditorMouseButton_Right:
            return MouseButton::Right;
        case LunaEditorMouseButton_Middle:
            return MouseButton::Middle;
        case LunaEditorMouseButton_Left:
        default:
            return MouseButton::Left;
    }
}

NativePluginContext* nativeContext(void* api_user_data) noexcept
{
    return static_cast<NativePluginContext*>(api_user_data);
}

EditorShell* nativeShell(void* api_user_data) noexcept
{
    NativePluginContext* context = nativeContext(api_user_data);
    return context != nullptr ? context->shell : nullptr;
}

Ui* nativeUi(void* api_user_data) noexcept
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr ? &shell->ui() : nullptr;
}

AssetService* nativeAssets(void* api_user_data) noexcept
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr ? &shell->assets() : nullptr;
}

PluginAssetService* nativePluginAssets(void* api_user_data) noexcept
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr ? &shell->pluginAssets() : nullptr;
}

ProjectService* nativeProject(void* api_user_data) noexcept
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr ? &shell->project() : nullptr;
}

SceneService* nativeScene(void* api_user_data) noexcept
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr ? &shell->scene() : nullptr;
}

SelectionService* nativeSelection(void* api_user_data) noexcept
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr ? &shell->selection() : nullptr;
}

ViewportService* nativeViewport(void* api_user_data) noexcept
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr ? &shell->viewport() : nullptr;
}

RuntimeViewportService* nativeRuntimeViewport(void* api_user_data) noexcept
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr ? &shell->runtimeViewport() : nullptr;
}

const LunaEditorHostApi* nativeHostApi(NativePluginContext* context) noexcept
{
    return context != nullptr ? context->host_api : nullptr;
}

void copyToNativeBuffer(char* buffer, size_t buffer_size, const std::string& value)
{
    if (buffer == nullptr || buffer_size == 0u) {
        return;
    }

    const size_t copy_size = (std::min)(buffer_size - 1u, value.size());
    if (copy_size > 0u) {
        std::memcpy(buffer, value.data(), copy_size);
    }
    buffer[copy_size] = '\0';
}

void copyPathToNativeBuffer(char* buffer, size_t buffer_size, const std::filesystem::path& value)
{
    copyToNativeBuffer(buffer, buffer_size, value.generic_string());
}

bool hasNativeAssetInfoLayout(const LunaEditorAssetInfo* out_info) noexcept
{
    return out_info != nullptr && out_info->api_version == LUNA_EDITOR_ASSET_INFO_API_VERSION &&
           out_info->struct_size >= sizeof(LunaEditorAssetInfo);
}

bool fillNativeAssetInfo(const AssetInfo& info, LunaEditorAssetInfo* out_info)
{
    if (!hasNativeAssetInfoLayout(out_info)) {
        return false;
    }

    out_info->handle = static_cast<uint64_t>(info.handle);
    out_info->type = toNativeAssetType(info.type);
    out_info->exists = info.exists ? 1 : 0;
    out_info->builtin = info.builtin ? 1 : 0;
    out_info->loading = info.loading ? 1 : 0;
    out_info->memory_only = info.memory_only ? 1 : 0;
    copyToNativeBuffer(out_info->label, out_info->label_size, info.label);
    copyToNativeBuffer(out_info->detail, out_info->detail_size, info.detail);
    copyPathToNativeBuffer(out_info->project_path, out_info->project_path_size, info.project_path);
    copyPathToNativeBuffer(out_info->absolute_path, out_info->absolute_path_size, info.absolute_path);
    return true;
}

bool hasNativeProjectInfoLayout(const LunaEditorProjectInfo* out_info) noexcept
{
    return out_info != nullptr && out_info->api_version == LUNA_EDITOR_PROJECT_INFO_API_VERSION &&
           out_info->struct_size >= sizeof(LunaEditorProjectInfo);
}

bool hasNativeSceneEntityInfoLayout(const LunaEditorSceneEntityInfo* out_info) noexcept
{
    return out_info != nullptr && out_info->api_version == LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION &&
           out_info->struct_size >= sizeof(LunaEditorSceneEntityInfo);
}

bool fillNativeSceneEntityInfo(const SceneEntityDetails& details, LunaEditorSceneEntityInfo* out_info)
{
    if (!hasNativeSceneEntityInfoLayout(out_info)) {
        return false;
    }

    out_info->id = static_cast<uint64_t>(details.id);
    out_info->parent_id = static_cast<uint64_t>(details.parent_id);
    out_info->component_flags = toNativeSceneEntityComponentFlags(details.components);
    out_info->child_count = details.children.size();
    copyToNativeBuffer(out_info->name, out_info->name_size, details.name);
    copyToNativeBuffer(out_info->parent_name, out_info->parent_name_size, details.parent_name);
    return true;
}

void nativeLog(void* api_user_data, LunaEditorLogLevel level, const char* message)
{
    NativePluginContext* context = nativeContext(api_user_data);
    const std::string_view plugin_id = context != nullptr ? std::string_view(context->plugin_id) : std::string_view{};
    const std::string_view text = nativeString(message);

    switch (level) {
        case LunaEditorLogLevel_Trace:
            LUNA_EDITOR_TRACE("[{}] {}", plugin_id, text);
            break;
        case LunaEditorLogLevel_Info:
            LUNA_EDITOR_INFO("[{}] {}", plugin_id, text);
            break;
        case LunaEditorLogLevel_Warn:
            LUNA_EDITOR_WARN("[{}] {}", plugin_id, text);
            break;
        case LunaEditorLogLevel_Error:
            LUNA_EDITOR_ERROR("[{}] {}", plugin_id, text);
            break;
        default:
            LUNA_EDITOR_INFO("[{}] {}", plugin_id, text);
            break;
    }
}

int nativeUiBeginWindow(void* api_user_data, const char* id, const char* title, int* open, uint32_t flags)
{
    Ui* ui = nativeUi(api_user_data);
    if (ui == nullptr || id == nullptr || title == nullptr) {
        return 0;
    }

    bool open_value = open == nullptr || *open != 0;
    const bool visible = ui->beginWindow(nativeString(id), nativeString(title), open != nullptr ? &open_value : nullptr, flags);
    if (open != nullptr) {
        *open = open_value ? 1 : 0;
    }
    return visible ? 1 : 0;
}

void nativeUiEndWindow(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endWindow();
    }
}

void nativeUiText(void* api_user_data, const char* value)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->text(nativeString(value));
    }
}

void nativeUiTextDisabled(void* api_user_data, const char* value)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->textDisabled(nativeString(value));
    }
}

void nativeUiTextWrapped(void* api_user_data, const char* value)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->textWrapped(nativeString(value));
    }
}

void nativeUiBulletText(void* api_user_data, const char* value)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->bulletText(nativeString(value));
    }
}

void nativeUiSeparator(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->separator();
    }
}

void nativeUiSeparatorText(void* api_user_data, const char* label)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->separatorText(nativeString(label));
    }
}

void nativeUiSameLine(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->sameLine();
    }
}

void nativeUiSpacing(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->spacing();
    }
}

void nativeUiIndent(void* api_user_data, float width)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->indent(width);
    }
}

void nativeUiUnindent(void* api_user_data, float width)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->unindent(width);
    }
}

void nativeUiBeginDisabled(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->beginDisabled();
    }
}

void nativeUiEndDisabled(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endDisabled();
    }
}

void nativeUiSetNextItemWidth(void* api_user_data, float width)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->setNextItemWidth(width);
    }
}

void nativeUiContentRegionAvail(void* api_user_data, LunaEditorVec2* out_value)
{
    if (out_value == nullptr) {
        return;
    }
    if (Ui* ui = nativeUi(api_user_data)) {
        *out_value = toNativeVec2(ui->contentRegionAvail());
    } else {
        *out_value = LunaEditorVec2{};
    }
}

void nativeUiWindowFramebufferScale(void* api_user_data, LunaEditorVec2* out_value)
{
    if (out_value == nullptr) {
        return;
    }
    if (Ui* ui = nativeUi(api_user_data)) {
        *out_value = toNativeVec2(ui->windowFramebufferScale());
    } else {
        *out_value = LunaEditorVec2{.x = 1.0f, .y = 1.0f};
    }
}

int nativeUiButton(void* api_user_data, const char* label, const LunaEditorVec2* size, uint32_t variant)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && ui->button(nativeString(label), toEditorVec2(size), toEditorButtonVariant(variant)) ? 1 : 0;
}

int nativeUiCheckbox(void* api_user_data, const char* label, int* value)
{
    Ui* ui = nativeUi(api_user_data);
    if (ui == nullptr || label == nullptr || value == nullptr) {
        return 0;
    }

    bool bool_value = *value != 0;
    const bool changed = ui->checkbox(nativeString(label), bool_value);
    *value = bool_value ? 1 : 0;
    return changed ? 1 : 0;
}

int nativeUiColorEdit3(void* api_user_data, const char* label, LunaEditorVec3* value)
{
    Ui* ui = nativeUi(api_user_data);
    if (ui == nullptr || label == nullptr || value == nullptr) {
        return 0;
    }

    Vec3 editor_value = toEditorVec3(*value);
    const bool changed = ui->colorEdit3(nativeString(label), editor_value);
    *value = toNativeVec3(editor_value);
    return changed ? 1 : 0;
}

int nativeUiColorEdit4(void* api_user_data, const char* label, LunaEditorVec4* value)
{
    Ui* ui = nativeUi(api_user_data);
    if (ui == nullptr || label == nullptr || value == nullptr) {
        return 0;
    }

    Vec4 editor_value = toEditorVec4(*value);
    const bool changed = ui->colorEdit4(nativeString(label), editor_value);
    *value = toNativeVec4(editor_value);
    return changed ? 1 : 0;
}

int nativeUiSliderInt(void* api_user_data, const char* label, int* value, int min_value, int max_value)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && value != nullptr &&
                   ui->sliderInt(nativeString(label), *value, min_value, max_value)
               ? 1
               : 0;
}

int nativeUiSliderFloat(void* api_user_data,
                        const char* label,
                        float* value,
                        float min_value,
                        float max_value,
                        const char* format)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && value != nullptr &&
                   ui->sliderFloat(nativeString(label), *value, min_value, max_value, format != nullptr ? nativeString(format) : "%.3f")
               ? 1
               : 0;
}

int nativeUiDragInt(void* api_user_data,
                    const char* label,
                    int* value,
                    float speed,
                    int min_value,
                    int max_value)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && value != nullptr &&
                   ui->dragInt(nativeString(label), *value, speed, min_value, max_value)
               ? 1
               : 0;
}

int nativeUiDragFloat(void* api_user_data,
                      const char* label,
                      float* value,
                      float speed,
                      float min_value,
                      float max_value,
                      const char* format)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && value != nullptr &&
                   ui->dragFloat(nativeString(label), *value, speed, min_value, max_value, format != nullptr ? nativeString(format) : "%.3f")
               ? 1
               : 0;
}

int nativeUiDragFloat3(void* api_user_data,
                       const char* label,
                       LunaEditorVec3* value,
                       float speed,
                       float min_value,
                       float max_value,
                       const char* format)
{
    Ui* ui = nativeUi(api_user_data);
    if (ui == nullptr || label == nullptr || value == nullptr) {
        return 0;
    }

    Vec3 editor_value = toEditorVec3(*value);
    const bool changed = ui->dragFloat3(nativeString(label),
                                        editor_value,
                                        speed,
                                        min_value,
                                        max_value,
                                        format != nullptr ? nativeString(format) : "%.3f");
    *value = toNativeVec3(editor_value);
    return changed ? 1 : 0;
}

int nativeUiInputText(void* api_user_data, const char* label, char* value, size_t buffer_size)
{
    Ui* ui = nativeUi(api_user_data);
    if (ui == nullptr || label == nullptr || value == nullptr || buffer_size == 0u) {
        return 0;
    }

    std::string editor_value(value);
    const bool changed = ui->inputText(nativeString(label), editor_value, buffer_size);
    copyToNativeBuffer(value, buffer_size, editor_value);
    return changed ? 1 : 0;
}

int nativeUiInputTextWithHint(void* api_user_data,
                              const char* label,
                              const char* hint,
                              char* value,
                              size_t buffer_size)
{
    Ui* ui = nativeUi(api_user_data);
    if (ui == nullptr || label == nullptr || hint == nullptr || value == nullptr || buffer_size == 0u) {
        return 0;
    }

    std::string editor_value(value);
    const bool changed = ui->inputTextWithHint(nativeString(label), nativeString(hint), editor_value, buffer_size);
    copyToNativeBuffer(value, buffer_size, editor_value);
    return changed ? 1 : 0;
}

int nativeUiTreeNode(void* api_user_data, const char* label)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && ui->treeNode(nativeString(label)) ? 1 : 0;
}

int nativeUiTreeNodeEx(void* api_user_data, const char* id, const char* label, uint32_t flags)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && id != nullptr && label != nullptr && ui->treeNodeEx(nativeString(id), nativeString(label), flags) ? 1 : 0;
}

void nativeUiTreePop(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->treePop();
    }
}

int nativeUiBeginCombo(void* api_user_data, const char* label, const char* preview_value)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && ui->beginCombo(nativeString(label), nativeString(preview_value)) ? 1 : 0;
}

void nativeUiEndCombo(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endCombo();
    }
}

int nativeUiSelectable(void* api_user_data, const char* label, int selected)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && ui->selectable(nativeString(label), selected != 0) ? 1 : 0;
}

void nativeUiSetItemDefaultFocus(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->setItemDefaultFocus();
    }
}

int nativeUiImage(void* api_user_data, const LunaEditorTextureView* texture, const LunaEditorVec2* size)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && texture != nullptr && ui->image(toEditorTextureView(*texture), toEditorVec2(size)) ? 1 : 0;
}

int nativeUiIsItemHovered(void* api_user_data)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && ui->isItemHovered() ? 1 : 0;
}

int nativeUiIsItemClicked(void* api_user_data, int button)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && ui->isItemClicked(toEditorMouseButton(button)) ? 1 : 0;
}

int nativeUiIsItemDoubleClicked(void* api_user_data, int button)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && ui->isItemDoubleClicked(toEditorMouseButton(button)) ? 1 : 0;
}

int nativeUiIsItemDeactivatedAfterEdit(void* api_user_data)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && ui->isItemDeactivatedAfterEdit() ? 1 : 0;
}

void nativeUiSetTooltip(void* api_user_data, const char* value)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->setTooltip(nativeString(value));
    }
}

int nativeUiInvisibleButton(void* api_user_data, const char* id, const LunaEditorVec2* size)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && id != nullptr && ui->invisibleButton(nativeString(id), toEditorVec2(size)) ? 1 : 0;
}

int nativeUiBeginSection(void* api_user_data, const char* id, const char* label, int default_open)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && id != nullptr && label != nullptr && ui->beginSection(nativeString(id), nativeString(label), default_open != 0) ? 1 : 0;
}

void nativeUiEndSection(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endSection();
    }
}

int nativeUiBeginMenu(void* api_user_data, const char* label, int enabled)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && ui->beginMenu(nativeString(label), enabled != 0) ? 1 : 0;
}

void nativeUiEndMenu(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endMenu();
    }
}

int nativeUiMenuItem(void* api_user_data, const char* label, int selected, int enabled)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && label != nullptr && ui->menuItem(nativeString(label), selected != 0, enabled != 0) ? 1 : 0;
}

void nativeUiOpenPopup(void* api_user_data, const char* id)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->openPopup(nativeString(id));
    }
}

int nativeUiBeginPopup(void* api_user_data, const char* id)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && id != nullptr && ui->beginPopup(nativeString(id)) ? 1 : 0;
}

int nativeUiBeginPopupContextItem(void* api_user_data, const char* id, int button)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && ui->beginPopupContextItem(nativeString(id), toEditorMouseButton(button)) ? 1 : 0;
}

void nativeUiCloseCurrentPopup(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->closeCurrentPopup();
    }
}

void nativeUiEndPopup(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endPopup();
    }
}

int nativeUiBeginDragDropSource(void* api_user_data)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && ui->beginDragDropSource() ? 1 : 0;
}

int nativeUiSetDragDropPayload(void* api_user_data, const char* type, const void* data, size_t size)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && type != nullptr && ui->setDragDropPayload(nativeString(type), data, size) ? 1 : 0;
}

void nativeUiEndDragDropSource(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endDragDropSource();
    }
}

int nativeUiBeginDragDropTarget(void* api_user_data)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && ui->beginDragDropTarget() ? 1 : 0;
}

int nativeUiAcceptDragDropPayload(void* api_user_data, const char* type, void* out_data, size_t size)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && type != nullptr && ui->acceptDragDropPayload(nativeString(type), out_data, size) ? 1 : 0;
}

void nativeUiEndDragDropTarget(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endDragDropTarget();
    }
}

float nativeUiScale(void* api_user_data, float value)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr ? ui->scale(value) : value;
}

void nativeUiScaled(void* api_user_data, const LunaEditorVec2* value, LunaEditorVec2* out_value)
{
    if (out_value == nullptr) {
        return;
    }
    if (Ui* ui = nativeUi(api_user_data)) {
        *out_value = toNativeVec2(ui->scaled(toEditorVec2(value)));
    } else {
        *out_value = value != nullptr ? *value : LunaEditorVec2{};
    }
}

int nativeUiBeginTable(void* api_user_data,
                       const char* id,
                       int column_count,
                       uint32_t flags,
                       const LunaEditorVec2* outer_size)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && id != nullptr && ui->beginTable(nativeString(id), column_count, flags, toEditorVec2(outer_size)) ? 1 : 0;
}

void nativeUiEndTable(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->endTable();
    }
}

void nativeUiTableSetupColumn(void* api_user_data, const char* label, uint32_t flags, float init_width_or_weight)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->tableSetupColumn(nativeString(label), flags, init_width_or_weight);
    }
}

void nativeUiTableHeadersRow(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->tableHeadersRow();
    }
}

void nativeUiTableNextRow(void* api_user_data)
{
    if (Ui* ui = nativeUi(api_user_data)) {
        ui->tableNextRow();
    }
}

int nativeUiTableNextColumn(void* api_user_data)
{
    Ui* ui = nativeUi(api_user_data);
    return ui != nullptr && ui->tableNextColumn() ? 1 : 0;
}

int nativeRegisterCommand(void* api_user_data, const LunaEditorCommandDescriptor* descriptor)
{
    NativePluginContext* context = nativeContext(api_user_data);
    if (context == nullptr || context->shell == nullptr || descriptor == nullptr) {
        return 0;
    }
    if (descriptor->api_version != LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION ||
        descriptor->struct_size < sizeof(LunaEditorCommandDescriptor) || descriptor->id == nullptr ||
        descriptor->id[0] == '\0' || descriptor->execute == nullptr) {
        return 0;
    }

    const std::string plugin_id = context->plugin_id;
    const auto can_execute = descriptor->can_execute;
    const auto is_checked = descriptor->is_checked;
    const auto execute = descriptor->execute;
    void* command_user_data = descriptor->command_user_data;

    CommandDescriptor editor_descriptor{
        .id = std::string(nativeString(descriptor->id)),
        .label = std::string(nativeString(descriptor->label)),
        .description = std::string(nativeString(descriptor->description)),
        .shortcut = std::string(nativeString(descriptor->shortcut)),
        .owner_id = plugin_id,
        .can_execute =
            [context, can_execute, command_user_data](Host&) {
                return can_execute == nullptr || can_execute(command_user_data, nativeHostApi(context)) != 0;
            },
        .is_checked =
            [context, is_checked, command_user_data](Host&) {
                return is_checked != nullptr && is_checked(command_user_data, nativeHostApi(context)) != 0;
            },
        .execute =
            [context, execute, command_user_data](Host&) {
                execute(command_user_data, nativeHostApi(context));
            },
    };
    return context->shell->commands().registerCommand(std::move(editor_descriptor)) ? 1 : 0;
}

void nativeUnregisterCommand(void* api_user_data, const char* id)
{
    if (EditorShell* shell = nativeShell(api_user_data); shell != nullptr && id != nullptr) {
        shell->commands().unregisterCommand(nativeString(id));
    }
}

int nativeExecuteCommand(void* api_user_data, const char* id)
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr && id != nullptr && shell->commands().execute(nativeString(id)) ? 1 : 0;
}

int nativeCanExecuteCommand(void* api_user_data, const char* id)
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr && id != nullptr && shell->commands().canExecute(nativeString(id)) ? 1 : 0;
}

int nativeIsCommandChecked(void* api_user_data, const char* id)
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr && id != nullptr && shell->commands().isChecked(nativeString(id)) ? 1 : 0;
}

int nativeRegisterWindow(void* api_user_data, const LunaEditorWindowDescriptor* descriptor)
{
    NativePluginContext* context = nativeContext(api_user_data);
    if (context == nullptr || context->shell == nullptr || descriptor == nullptr) {
        return 0;
    }
    if (descriptor->api_version != LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION ||
        descriptor->struct_size < sizeof(LunaEditorWindowDescriptor) || descriptor->id == nullptr ||
        descriptor->id[0] == '\0' || descriptor->title == nullptr || descriptor->draw == nullptr) {
        return 0;
    }

    const auto draw = descriptor->draw;
    void* window_user_data = descriptor->window_user_data;

    WindowDescriptor editor_descriptor{
        .id = std::string(nativeString(descriptor->id)),
        .title = std::string(nativeString(descriptor->title)),
        .default_open = descriptor->default_open != 0,
        .default_size = Vec2{.x = descriptor->default_size.x, .y = descriptor->default_size.y},
        .flags = descriptor->flags,
        .owner_id = context->plugin_id,
        .draw =
            [context, draw, window_user_data](WindowDrawContext&) {
                draw(window_user_data, nativeHostApi(context));
            },
    };
    return context->shell->windows().registerWindow(std::move(editor_descriptor)) ? 1 : 0;
}

void nativeUnregisterWindow(void* api_user_data, const char* id)
{
    if (EditorShell* shell = nativeShell(api_user_data); shell != nullptr && id != nullptr) {
        shell->windows().unregisterWindow(nativeString(id));
    }
}

int nativeIsWindowOpen(void* api_user_data, const char* id)
{
    EditorShell* shell = nativeShell(api_user_data);
    return shell != nullptr && id != nullptr && shell->windows().isWindowOpen(nativeString(id)) ? 1 : 0;
}

void nativeSetWindowOpen(void* api_user_data, const char* id, int open)
{
    if (EditorShell* shell = nativeShell(api_user_data); shell != nullptr && id != nullptr) {
        shell->windows().setWindowOpen(nativeString(id), open != 0);
    }
}

int nativeDescribeAsset(void* api_user_data, uint64_t handle, LunaEditorAssetInfo* out_info)
{
    AssetService* assets = nativeAssets(api_user_data);
    return assets != nullptr && fillNativeAssetInfo(assets->describeAsset(AssetHandle(handle)), out_info) ? 1 : 0;
}

int nativeAssetInfo(void* api_user_data, uint64_t handle, LunaEditorAssetInfo* out_info)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr) {
        return 0;
    }

    const std::optional<AssetInfo> info = assets->assetInfo(AssetHandle(handle));
    return info && fillNativeAssetInfo(*info, out_info) ? 1 : 0;
}

int nativeAssetInfoByPath(void* api_user_data, const char* path, LunaEditorAssetInfo* out_info)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr || path == nullptr) {
        return 0;
    }

    const std::optional<AssetInfo> info = assets->assetInfoByPath(std::filesystem::path(nativeString(path)));
    return info && fillNativeAssetInfo(*info, out_info) ? 1 : 0;
}

size_t nativeListAssets(void* api_user_data,
                        uint32_t type_filter,
                        int include_builtin,
                        void* user_data,
                        LunaEditorEnumerateAssetFn enumerate_fn)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr) {
        return 0u;
    }

    const std::vector<AssetInfo> asset_infos = assets->listAssets(toEditorAssetType(type_filter), include_builtin != 0);
    if (enumerate_fn == nullptr) {
        return asset_infos.size();
    }

    size_t enumerated_count = 0u;
    for (const AssetInfo& info : asset_infos) {
        char label[256]{};
        char detail[256]{};
        char project_path[512]{};
        char absolute_path[512]{};
        LunaEditorAssetInfo native_info{
            .struct_size = sizeof(LunaEditorAssetInfo),
            .api_version = LUNA_EDITOR_ASSET_INFO_API_VERSION,
            .label = label,
            .label_size = sizeof(label),
            .detail = detail,
            .detail_size = sizeof(detail),
            .project_path = project_path,
            .project_path_size = sizeof(project_path),
            .absolute_path = absolute_path,
            .absolute_path_size = sizeof(absolute_path),
        };
        if (!fillNativeAssetInfo(info, &native_info)) {
            continue;
        }
        ++enumerated_count;
        if (enumerate_fn(user_data, &native_info) == 0) {
            break;
        }
    }
    return enumerated_count;
}

int nativeAssetExists(void* api_user_data, uint64_t handle)
{
    AssetService* assets = nativeAssets(api_user_data);
    return assets != nullptr && assets->assetExists(AssetHandle(handle)) ? 1 : 0;
}

int nativeAssetPathExists(void* api_user_data, const char* path)
{
    AssetService* assets = nativeAssets(api_user_data);
    return assets != nullptr && path != nullptr && assets->assetPathExists(std::filesystem::path(nativeString(path))) ? 1 : 0;
}

uint64_t nativeFindAssetHandleByPath(void* api_user_data, const char* path)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr || path == nullptr) {
        return 0u;
    }
    return static_cast<uint64_t>(assets->findAssetHandleByPath(std::filesystem::path(nativeString(path))));
}

int nativeAssetsRootPath(void* api_user_data, char* out_path, size_t out_path_size)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr) {
        return 0;
    }

    const std::optional<std::filesystem::path> path = assets->assetsRootPath();
    if (!path) {
        return 0;
    }
    copyPathToNativeBuffer(out_path, out_path_size, *path);
    return 1;
}

int nativeResolveProjectAssetPath(void* api_user_data,
                                  const char* project_relative_path,
                                  char* out_path,
                                  size_t out_path_size)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr || project_relative_path == nullptr) {
        return 0;
    }

    const std::optional<std::filesystem::path> path =
        assets->resolveProjectAssetPath(std::filesystem::path(nativeString(project_relative_path)));
    if (!path) {
        return 0;
    }
    copyPathToNativeBuffer(out_path, out_path_size, *path);
    return 1;
}

int nativeMakeProjectRelativeAssetPath(void* api_user_data, const char* path, char* out_path, size_t out_path_size)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr || path == nullptr) {
        return 0;
    }

    const std::optional<std::filesystem::path> relative_path =
        assets->makeProjectRelativeAssetPath(std::filesystem::path(nativeString(path)));
    if (!relative_path) {
        return 0;
    }
    copyPathToNativeBuffer(out_path, out_path_size, *relative_path);
    return 1;
}

int nativeRefreshAssets(void* api_user_data, LunaEditorAssetRefreshResult* out_result)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr || out_result == nullptr ||
        out_result->api_version != LUNA_EDITOR_ASSET_REFRESH_RESULT_API_VERSION ||
        out_result->struct_size < sizeof(LunaEditorAssetRefreshResult)) {
        return 0;
    }

    const AssetRefreshResult result = assets->refreshAssets();
    out_result->success = result.success ? 1 : 0;
    out_result->project_loaded = result.project_loaded ? 1 : 0;
    out_result->revision = result.revision;
    copyToNativeBuffer(out_result->message, out_result->message_size, result.message);
    out_result->discovered_assets = result.discovered_assets;
    out_result->imported_missing_assets = result.imported_missing_assets;
    out_result->loaded_existing_metadata = result.loaded_existing_metadata;
    out_result->rebuilt_metadata = result.rebuilt_metadata;
    out_result->unsupported_files_skipped = result.unsupported_files_skipped;
    out_result->failed_assets = result.failed_assets;
    out_result->missing_metadata_after_sync = result.missing_metadata_after_sync;
    out_result->script_files_skipped_no_plugin = result.script_files_skipped_no_plugin;
    out_result->script_files_skipped_unsupported_language = result.script_files_skipped_unsupported_language;
    out_result->generated_model_files = result.generated_model_files;
    out_result->generated_model_metadata = result.generated_model_metadata;
    out_result->generated_material_files = result.generated_material_files;
    out_result->generated_material_metadata = result.generated_material_metadata;
    out_result->generated_texture_metadata = result.generated_texture_metadata;
    out_result->failed_generated_model_assets = result.failed_generated_model_assets;
    return 1;
}

uint64_t nativeAssetRevision(void* api_user_data)
{
    AssetService* assets = nativeAssets(api_user_data);
    return assets != nullptr ? assets->assetRevision() : 0u;
}

int nativeIsAssetLoading(void* api_user_data, uint64_t handle)
{
    AssetService* assets = nativeAssets(api_user_data);
    return assets != nullptr && assets->isAssetLoading(AssetHandle(handle)) ? 1 : 0;
}

int nativeAcceptsAssetType(void* api_user_data, uint32_t type, const uint32_t* accepted_types, size_t accepted_type_count)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr) {
        return 0;
    }
    if (accepted_type_count > 0u && accepted_types == nullptr) {
        return 0;
    }

    std::vector<AssetType> editor_accepted_types;
    editor_accepted_types.reserve(accepted_type_count);
    for (size_t index = 0; index < accepted_type_count; ++index) {
        if (accepted_types != nullptr) {
            editor_accepted_types.push_back(toEditorAssetType(accepted_types[index]));
        }
    }
    return assets->acceptsAssetType(toEditorAssetType(type),
                                    editor_accepted_types.data(),
                                    editor_accepted_types.size())
               ? 1
               : 0;
}

int nativeMeshSubmeshCount(void* api_user_data, uint64_t mesh_handle, size_t* out_count)
{
    AssetService* assets = nativeAssets(api_user_data);
    if (assets == nullptr || out_count == nullptr) {
        return 0;
    }

    const std::optional<std::size_t> count = assets->meshSubmeshCount(AssetHandle(mesh_handle));
    if (!count) {
        return 0;
    }
    *out_count = *count;
    return 1;
}

int nativeBeginAssetDragDropSource(void* api_user_data, uint64_t handle, const char* label)
{
    AssetService* assets = nativeAssets(api_user_data);
    return assets != nullptr && assets->beginAssetDragDropSource(AssetHandle(handle), nativeString(label)) ? 1 : 0;
}

int nativePluginRootPath(void* api_user_data, char* out_path, size_t out_path_size)
{
    NativePluginContext* context = nativeContext(api_user_data);
    PluginAssetService* plugin_assets = nativePluginAssets(api_user_data);
    if (context == nullptr || plugin_assets == nullptr) {
        return 0;
    }

    const std::optional<std::filesystem::path> path = plugin_assets->pluginRootPath(context->plugin_id);
    if (!path) {
        return 0;
    }
    copyPathToNativeBuffer(out_path, out_path_size, *path);
    return 1;
}

int nativePluginAssetRootPath(void* api_user_data, char* out_path, size_t out_path_size)
{
    NativePluginContext* context = nativeContext(api_user_data);
    PluginAssetService* plugin_assets = nativePluginAssets(api_user_data);
    if (context == nullptr || plugin_assets == nullptr) {
        return 0;
    }

    const std::optional<std::filesystem::path> path = plugin_assets->assetRootPath(context->plugin_id);
    if (!path) {
        return 0;
    }
    copyPathToNativeBuffer(out_path, out_path_size, *path);
    return 1;
}

int nativePluginAssetResolvePath(void* api_user_data,
                                 const char* relative_asset_path,
                                 char* out_path,
                                 size_t out_path_size)
{
    NativePluginContext* context = nativeContext(api_user_data);
    PluginAssetService* plugin_assets = nativePluginAssets(api_user_data);
    if (context == nullptr || plugin_assets == nullptr || relative_asset_path == nullptr) {
        return 0;
    }

    const std::optional<std::filesystem::path> path =
        plugin_assets->resolvePath(context->plugin_id, std::filesystem::path(nativeString(relative_asset_path)));
    if (!path) {
        return 0;
    }
    copyPathToNativeBuffer(out_path, out_path_size, *path);
    return 1;
}

int nativePluginAssetExists(void* api_user_data, const char* relative_asset_path)
{
    NativePluginContext* context = nativeContext(api_user_data);
    PluginAssetService* plugin_assets = nativePluginAssets(api_user_data);
    return context != nullptr && plugin_assets != nullptr && relative_asset_path != nullptr &&
                   plugin_assets->exists(context->plugin_id, std::filesystem::path(nativeString(relative_asset_path)))
               ? 1
               : 0;
}

int nativePluginAssetReadText(void* api_user_data,
                              const char* relative_asset_path,
                              char* out_text,
                              size_t out_text_size,
                              size_t* out_required_size)
{
    NativePluginContext* context = nativeContext(api_user_data);
    PluginAssetService* plugin_assets = nativePluginAssets(api_user_data);
    if (context == nullptr || plugin_assets == nullptr || relative_asset_path == nullptr) {
        return 0;
    }

    const std::optional<std::string> text =
        plugin_assets->readText(context->plugin_id, std::filesystem::path(nativeString(relative_asset_path)));
    if (!text) {
        return 0;
    }

    const size_t required_size = text->size() + 1u;
    if (out_required_size != nullptr) {
        *out_required_size = required_size;
    }
    if (out_text == nullptr || out_text_size == 0u) {
        return 1;
    }
    if (out_text_size < required_size) {
        copyToNativeBuffer(out_text, out_text_size, *text);
        return 0;
    }
    copyToNativeBuffer(out_text, out_text_size, *text);
    return 1;
}

int nativePluginAssetReadBytes(void* api_user_data,
                               const char* relative_asset_path,
                               void* out_data,
                               size_t out_data_size,
                               size_t* out_required_size)
{
    NativePluginContext* context = nativeContext(api_user_data);
    PluginAssetService* plugin_assets = nativePluginAssets(api_user_data);
    if (context == nullptr || plugin_assets == nullptr || relative_asset_path == nullptr) {
        return 0;
    }

    const std::filesystem::path relative_path(nativeString(relative_asset_path));
    const PluginAssetBytes bytes = plugin_assets->readBytes(context->plugin_id, relative_path);
    if (bytes.data.empty() && !plugin_assets->exists(context->plugin_id, relative_path)) {
        return 0;
    }

    if (out_required_size != nullptr) {
        *out_required_size = bytes.data.size();
    }
    if (out_data == nullptr || out_data_size == 0u) {
        return 1;
    }
    if (out_data_size < bytes.data.size()) {
        if (!bytes.data.empty()) {
            std::memcpy(out_data, bytes.data.data(), out_data_size);
        }
        return 0;
    }
    if (!bytes.data.empty()) {
        std::memcpy(out_data, bytes.data.data(), bytes.data.size());
    }
    return 1;
}

int nativePluginAssetTexture(void* api_user_data, const char* relative_asset_path, LunaEditorTextureView* out_texture)
{
    NativePluginContext* context = nativeContext(api_user_data);
    PluginAssetService* plugin_assets = nativePluginAssets(api_user_data);
    if (context == nullptr || plugin_assets == nullptr || relative_asset_path == nullptr || out_texture == nullptr) {
        return 0;
    }

    const TextureView texture =
        plugin_assets->texture(context->plugin_id, std::filesystem::path(nativeString(relative_asset_path)));
    if (!texture.valid()) {
        *out_texture = LunaEditorTextureView{};
        return 0;
    }
    *out_texture = toNativeTextureView(texture);
    return 1;
}

int nativeAddMenuItem(void* api_user_data, const LunaEditorMenuItemDescriptor* descriptor)
{
    NativePluginContext* context = nativeContext(api_user_data);
    if (context == nullptr || context->shell == nullptr || descriptor == nullptr) {
        return 0;
    }
    if (descriptor->api_version != LUNA_EDITOR_MENU_ITEM_DESCRIPTOR_API_VERSION ||
        descriptor->struct_size < sizeof(LunaEditorMenuItemDescriptor) || descriptor->menu_path == nullptr ||
        descriptor->menu_path[0] == '\0' || descriptor->command_id == nullptr || descriptor->command_id[0] == '\0') {
        return 0;
    }

    return context->shell->menus().addMenuItem(MenuItemDescriptor{
               .menu_path = std::string(nativeString(descriptor->menu_path)),
               .command_id = std::string(nativeString(descriptor->command_id)),
               .label = std::string(nativeString(descriptor->label)),
               .shortcut = std::string(nativeString(descriptor->shortcut)),
               .owner_id = context->plugin_id,
           })
               ? 1
               : 0;
}

void nativeRemoveMenuItem(void* api_user_data, const char* menu_path, const char* command_id)
{
    if (EditorShell* shell = nativeShell(api_user_data); shell != nullptr && menu_path != nullptr &&
                                                           command_id != nullptr) {
        shell->menus().removeMenuItem(nativeString(menu_path), nativeString(command_id));
    }
}

void nativeRemoveMenuItemsForCommand(void* api_user_data, const char* command_id)
{
    if (EditorShell* shell = nativeShell(api_user_data); shell != nullptr && command_id != nullptr) {
        shell->menus().removeMenuItemsForCommand(nativeString(command_id));
    }
}

int nativeHasProjectLoaded(void* api_user_data)
{
    ProjectService* project = nativeProject(api_user_data);
    return project != nullptr && project->hasProjectLoaded() ? 1 : 0;
}

int nativeProjectRootPath(void* api_user_data, char* out_path, size_t out_path_size)
{
    ProjectService* project = nativeProject(api_user_data);
    if (project == nullptr) {
        return 0;
    }

    const std::optional<std::filesystem::path> path = project->projectRootPath();
    if (!path) {
        return 0;
    }
    copyPathToNativeBuffer(out_path, out_path_size, *path);
    return 1;
}

int nativeProjectInfo(void* api_user_data, LunaEditorProjectInfo* out_info)
{
    ProjectService* project = nativeProject(api_user_data);
    if (project == nullptr || !hasNativeProjectInfoLayout(out_info)) {
        return 0;
    }

    const std::optional<ProjectInfo> info = project->projectInfo();
    if (!info) {
        return 0;
    }

    copyToNativeBuffer(out_info->name, out_info->name_size, info->Name);
    copyToNativeBuffer(out_info->version, out_info->version_size, info->Version);
    copyToNativeBuffer(out_info->author, out_info->author_size, info->Author);
    copyToNativeBuffer(out_info->description, out_info->description_size, info->Description);
    copyPathToNativeBuffer(out_info->start_scene, out_info->start_scene_size, info->StartScene);
    copyPathToNativeBuffer(out_info->assets_path, out_info->assets_path_size, info->AssetsPath);
    copyToNativeBuffer(out_info->selected_script_plugin_id,
                       out_info->selected_script_plugin_id_size,
                       info->Scripting.SelectedPluginId);
    copyToNativeBuffer(out_info->selected_script_backend_name,
                       out_info->selected_script_backend_name_size,
                       info->Scripting.SelectedBackendName);
    return 1;
}

int nativeSaveProject(void* api_user_data)
{
    ProjectService* project = nativeProject(api_user_data);
    return project != nullptr && project->saveProject() ? 1 : 0;
}

int nativeSceneLabel(void* api_user_data, char* out_label, size_t out_label_size)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr) {
        return 0;
    }

    copyToNativeBuffer(out_label, out_label_size, scene->sceneLabel());
    return 1;
}

size_t nativeSceneEntityCount(void* api_user_data)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr ? scene->entityCount() : 0u;
}

int nativeCanEditScene(void* api_user_data)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && scene->canEditScene() ? 1 : 0;
}

int nativeOpenSceneFile(void* api_user_data, const char* scene_file_path)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && scene_file_path != nullptr &&
                   scene->openSceneFile(std::filesystem::path(nativeString(scene_file_path)))
               ? 1
               : 0;
}

size_t nativeEnumerateSceneEntities(void* api_user_data,
                                    void* user_data,
                                    LunaEditorEnumerateSceneEntityFn enumerate_fn)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr) {
        return 0u;
    }

    const std::vector<SceneEntityInfo> hierarchy = scene->entityHierarchy();
    if (enumerate_fn == nullptr) {
        return hierarchy.size();
    }

    size_t enumerated_count = 0u;
    for (const SceneEntityInfo& entity : hierarchy) {
        char name[256]{};
        char parent_name[256]{};
        LunaEditorSceneEntityInfo native_info{
            .struct_size = sizeof(LunaEditorSceneEntityInfo),
            .api_version = LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION,
            .id = static_cast<uint64_t>(entity.id),
            .parent_id = static_cast<uint64_t>(entity.parent_id),
            .child_count = entity.child_ids.size(),
            .name = name,
            .name_size = sizeof(name),
            .parent_name = parent_name,
            .parent_name_size = sizeof(parent_name),
        };
        copyToNativeBuffer(native_info.name, native_info.name_size, entity.name);

        if (const std::optional<SceneEntityDetails> details = scene->entityDetails(entity.id)) {
            native_info.component_flags = toNativeSceneEntityComponentFlags(details->components);
            copyToNativeBuffer(native_info.parent_name, native_info.parent_name_size, details->parent_name);
        }

        ++enumerated_count;
        if (enumerate_fn(user_data, &native_info) == 0) {
            break;
        }
    }
    return enumerated_count;
}

int nativeEntityExists(void* api_user_data, uint64_t entity_id)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && scene->entityExists(EntityId(entity_id)) ? 1 : 0;
}

int nativeEntityInfo(void* api_user_data, uint64_t entity_id, LunaEditorSceneEntityInfo* out_info)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr) {
        return 0;
    }

    const std::optional<SceneEntityDetails> details = scene->entityDetails(EntityId(entity_id));
    return details && fillNativeSceneEntityInfo(*details, out_info) ? 1 : 0;
}

int nativeIsEntityDescendantOf(void* api_user_data, uint64_t entity_id, uint64_t potential_ancestor_id)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr &&
                   scene->isEntityDescendantOf(EntityId(entity_id), EntityId(potential_ancestor_id))
               ? 1
               : 0;
}

uint64_t nativeCreateEntity(void* api_user_data, const char* name)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr) {
        return 0u;
    }
    return static_cast<uint64_t>(scene->createEntity(std::string(nativeString(name))));
}

uint64_t nativeCreateEntityEx(void* api_user_data, const LunaEditorSceneEntityCreateRequest* request)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr || request == nullptr ||
        request->api_version != LUNA_EDITOR_SCENE_ENTITY_CREATE_REQUEST_API_VERSION ||
        request->struct_size < sizeof(LunaEditorSceneEntityCreateRequest)) {
        return 0u;
    }

    return static_cast<uint64_t>(scene->createEntity(SceneEntityCreateRequest{
        .kind = toEditorSceneEntityCreateKind(request->kind),
        .name = std::string(nativeString(request->name)),
        .parent_id = EntityId(request->parent_id),
        .asset_handle = AssetHandle(request->asset_handle),
    }));
}

int nativeDestroyEntity(void* api_user_data, uint64_t entity_id)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && scene->destroyEntity(EntityId(entity_id)) ? 1 : 0;
}

int nativeReparentEntity(void* api_user_data, uint64_t entity_id, uint64_t new_parent_id, int preserve_world_transform)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr &&
                   scene->reparentEntity(EntityId(entity_id), EntityId(new_parent_id), preserve_world_transform != 0)
               ? 1
               : 0;
}

int nativeSetEntityName(void* api_user_data, uint64_t entity_id, const char* name)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && name != nullptr && scene->setEntityName(EntityId(entity_id), std::string(nativeString(name)))
               ? 1
               : 0;
}

int nativeGetEntityTransform(void* api_user_data, uint64_t entity_id, LunaEditorSceneTransform* out_transform)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr || out_transform == nullptr) {
        return 0;
    }

    const std::optional<SceneEntityDetails> details = scene->entityDetails(EntityId(entity_id));
    if (!details || !details->components.transform) {
        return 0;
    }
    *out_transform = toNativeSceneTransform(details->transform);
    return 1;
}

int nativeSetEntityTransform(void* api_user_data, uint64_t entity_id, const LunaEditorSceneTransform* transform)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && transform != nullptr &&
                   scene->setEntityTransform(EntityId(entity_id), toEditorSceneTransform(*transform))
               ? 1
               : 0;
}

int nativeGetCameraComponent(void* api_user_data, uint64_t entity_id, LunaEditorSceneCameraComponent* out_component)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr || !hasNativeSceneCameraComponentLayout(out_component)) {
        return 0;
    }

    const std::optional<SceneEntityDetails> details = scene->entityDetails(EntityId(entity_id));
    if (!details || !details->camera) {
        return 0;
    }

    *out_component = toNativeSceneCameraComponent(*details->camera);
    return 1;
}

int nativeSetCameraComponent(void* api_user_data, uint64_t entity_id, const LunaEditorSceneCameraComponent* component)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && component != nullptr &&
                   scene->setCameraComponent(EntityId(entity_id), toEditorSceneCameraComponent(*component))
               ? 1
               : 0;
}

int nativeGetLightComponent(void* api_user_data, uint64_t entity_id, LunaEditorSceneLightComponent* out_component)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr || !hasNativeSceneLightComponentLayout(out_component)) {
        return 0;
    }

    const std::optional<SceneEntityDetails> details = scene->entityDetails(EntityId(entity_id));
    if (!details || !details->light) {
        return 0;
    }

    *out_component = toNativeSceneLightComponent(*details->light);
    return 1;
}

int nativeSetLightComponent(void* api_user_data, uint64_t entity_id, const LunaEditorSceneLightComponent* component)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && component != nullptr &&
                   scene->setLightComponent(EntityId(entity_id), toEditorSceneLightComponent(*component))
               ? 1
               : 0;
}

int nativeGetMeshComponent(void* api_user_data, uint64_t entity_id, LunaEditorSceneMeshComponent* out_component)
{
    SceneService* scene = nativeScene(api_user_data);
    if (scene == nullptr || !hasNativeSceneMeshComponentLayout(out_component)) {
        return 0;
    }

    const std::optional<SceneEntityDetails> details = scene->entityDetails(EntityId(entity_id));
    if (!details || !details->mesh) {
        return 0;
    }

    return fillNativeSceneMeshComponent(*details->mesh, out_component) ? 1 : 0;
}

int nativeSetMeshComponent(void* api_user_data, uint64_t entity_id, const LunaEditorSceneMeshComponent* component)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && component != nullptr &&
                   scene->setMeshComponent(EntityId(entity_id), toEditorSceneMeshComponent(*component))
               ? 1
               : 0;
}

int nativeAddComponent(void* api_user_data, uint64_t entity_id, uint32_t component_kind)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && scene->addComponent(EntityId(entity_id), toEditorSceneComponentKind(component_kind)) ? 1 : 0;
}

int nativeRemoveComponent(void* api_user_data, uint64_t entity_id, uint32_t component_kind)
{
    SceneService* scene = nativeScene(api_user_data);
    return scene != nullptr && scene->removeComponent(EntityId(entity_id), toEditorSceneComponentKind(component_kind)) ? 1 : 0;
}

uint64_t nativeSelectedEntityId(void* api_user_data)
{
    SelectionService* selection = nativeSelection(api_user_data);
    return selection != nullptr ? static_cast<uint64_t>(selection->selectedEntityId()) : 0u;
}

void nativeSelectEntity(void* api_user_data, uint64_t entity_id)
{
    if (SelectionService* selection = nativeSelection(api_user_data)) {
        selection->selectEntity(EntityId(entity_id));
    }
}

void nativeClearSelection(void* api_user_data)
{
    if (SelectionService* selection = nativeSelection(api_user_data)) {
        selection->clearSelection();
    }
}

uint64_t nativeDefaultSceneViewport(void* api_user_data)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    return viewport != nullptr ? viewport->defaultSceneViewport() : kInvalidViewportId;
}

uint64_t nativeCreateSceneViewport(void* api_user_data, const char* debug_name)
{
    auto* context = static_cast<NativePluginContext*>(api_user_data);
    if (context == nullptr || context->shell == nullptr) {
        return kInvalidViewportId;
    }

    return context->shell->createSceneViewportForPlugin(context->plugin_id, nativeString(debug_name));
}

void nativeDestroySceneViewport(void* api_user_data, uint64_t viewport_id)
{
    if (ViewportService* viewport = nativeViewport(api_user_data)) {
        viewport->destroySceneViewport(static_cast<ViewportId>(viewport_id));
    }
}

int nativeIsSceneViewportValid(void* api_user_data, uint64_t viewport_id)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    return viewport != nullptr && viewport->isSceneViewportValid(static_cast<ViewportId>(viewport_id)) ? 1 : 0;
}

int nativeSyncSceneViewportEx(void* api_user_data,
                              uint64_t viewport_id,
                              uint32_t framebuffer_width,
                              uint32_t framebuffer_height,
                              LunaEditorViewportPresentation* out_presentation)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    if (viewport == nullptr || out_presentation == nullptr ||
        out_presentation->struct_size < sizeof(LunaEditorViewportPresentation) ||
        out_presentation->api_version != LUNA_EDITOR_VIEWPORT_API_VERSION) {
        return 0;
    }

    *out_presentation = toNativeViewportPresentation(viewport->syncSceneViewport(
        static_cast<ViewportId>(viewport_id),
        UVec2{.x = framebuffer_width, .y = framebuffer_height}));
    return 1;
}

int nativeSceneTextureViewEx(void* api_user_data, uint64_t viewport_id, LunaEditorTextureView* out_texture)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    if (viewport == nullptr || out_texture == nullptr) {
        return 0;
    }

    *out_texture = toNativeTextureView(viewport->sceneTextureView(static_cast<ViewportId>(viewport_id)));
    return 1;
}

int nativeSyncSceneViewport(void* api_user_data,
                            uint32_t framebuffer_width,
                            uint32_t framebuffer_height,
                            LunaEditorViewportPresentation* out_presentation)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    if (viewport == nullptr || out_presentation == nullptr ||
        out_presentation->struct_size < sizeof(LunaEditorViewportPresentation) ||
        out_presentation->api_version != LUNA_EDITOR_VIEWPORT_API_VERSION) {
        return 0;
    }

    *out_presentation = toNativeViewportPresentation(
        viewport->syncSceneViewport(UVec2{.x = framebuffer_width, .y = framebuffer_height}));
    return 1;
}

int nativeSceneTextureView(void* api_user_data, LunaEditorTextureView* out_texture)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    if (viewport == nullptr || out_texture == nullptr) {
        return 0;
    }

    *out_texture = toNativeTextureView(viewport->sceneTextureView());
    return 1;
}

void nativeEditorCameraPosition(void* api_user_data, LunaEditorVec3* out_position)
{
    if (ViewportService* viewport = nativeViewport(api_user_data); viewport != nullptr && out_position != nullptr) {
        *out_position = toNativeVec3(viewport->editorCameraPosition());
    }
}

int nativeGizmoOperationName(void* api_user_data, char* out_value, size_t out_value_size)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    if (viewport == nullptr) {
        return 0;
    }

    copyToNativeBuffer(out_value, out_value_size, viewport->gizmoOperationName());
    return 1;
}

int nativeGizmoModeName(void* api_user_data, char* out_value, size_t out_value_size)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    if (viewport == nullptr) {
        return 0;
    }

    copyToNativeBuffer(out_value, out_value_size, viewport->gizmoModeName());
    return 1;
}

int nativePickDebugVisualizationEnabled(void* api_user_data)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    return viewport != nullptr && viewport->pickDebugVisualizationEnabled() ? 1 : 0;
}

void nativeSetPickDebugVisualizationEnabled(void* api_user_data, int enabled)
{
    if (ViewportService* viewport = nativeViewport(api_user_data)) {
        viewport->setPickDebugVisualizationEnabled(enabled != 0);
    }
}

int nativeEditorGridEnabled(void* api_user_data)
{
    ViewportService* viewport = nativeViewport(api_user_data);
    return viewport != nullptr && viewport->editorGridEnabled() ? 1 : 0;
}

void nativeSetEditorGridEnabled(void* api_user_data, int enabled)
{
    if (ViewportService* viewport = nativeViewport(api_user_data)) {
        viewport->setEditorGridEnabled(enabled != 0);
    }
}

int nativeIsRuntimeViewportEnabled(void* api_user_data)
{
    RuntimeViewportService* runtime_viewport = nativeRuntimeViewport(api_user_data);
    return runtime_viewport != nullptr && runtime_viewport->isRuntimeViewportEnabled() ? 1 : 0;
}

int nativeIsRuntimeViewportRequested(void* api_user_data)
{
    RuntimeViewportService* runtime_viewport = nativeRuntimeViewport(api_user_data);
    return runtime_viewport != nullptr && runtime_viewport->isRuntimeViewportRequested() ? 1 : 0;
}

void nativeSetRuntimeViewportRequested(void* api_user_data, int enabled)
{
    if (RuntimeViewportService* runtime_viewport = nativeRuntimeViewport(api_user_data)) {
        runtime_viewport->setRuntimeViewportRequested(enabled != 0);
    }
}

size_t nativeRuntimeEntityCount(void* api_user_data)
{
    RuntimeViewportService* runtime_viewport = nativeRuntimeViewport(api_user_data);
    return runtime_viewport != nullptr ? runtime_viewport->runtimeEntityCount() : 0u;
}

LunaEditorLogApi makeNativeLogApi(NativePluginContext& context)
{
    return LunaEditorLogApi{
        .struct_size = sizeof(LunaEditorLogApi),
        .api_version = LUNA_EDITOR_LOG_API_VERSION,
        .api_user_data = &context,
        .log = &nativeLog,
    };
}

LunaEditorUiApi makeNativeUiApi(NativePluginContext& context)
{
    return LunaEditorUiApi{
        .struct_size = sizeof(LunaEditorUiApi),
        .api_version = LUNA_EDITOR_UI_API_VERSION,
        .api_user_data = &context,
        .begin_window = &nativeUiBeginWindow,
        .end_window = &nativeUiEndWindow,
        .text = &nativeUiText,
        .text_disabled = &nativeUiTextDisabled,
        .text_wrapped = &nativeUiTextWrapped,
        .bullet_text = &nativeUiBulletText,
        .separator = &nativeUiSeparator,
        .separator_text = &nativeUiSeparatorText,
        .same_line = &nativeUiSameLine,
        .spacing = &nativeUiSpacing,
        .indent = &nativeUiIndent,
        .unindent = &nativeUiUnindent,
        .begin_disabled = &nativeUiBeginDisabled,
        .end_disabled = &nativeUiEndDisabled,
        .set_next_item_width = &nativeUiSetNextItemWidth,
        .content_region_avail = &nativeUiContentRegionAvail,
        .window_framebuffer_scale = &nativeUiWindowFramebufferScale,
        .button = &nativeUiButton,
        .checkbox = &nativeUiCheckbox,
        .color_edit3 = &nativeUiColorEdit3,
        .color_edit4 = &nativeUiColorEdit4,
        .slider_int = &nativeUiSliderInt,
        .slider_float = &nativeUiSliderFloat,
        .drag_int = &nativeUiDragInt,
        .drag_float = &nativeUiDragFloat,
        .drag_float3 = &nativeUiDragFloat3,
        .input_text = &nativeUiInputText,
        .input_text_with_hint = &nativeUiInputTextWithHint,
        .tree_node = &nativeUiTreeNode,
        .tree_node_ex = &nativeUiTreeNodeEx,
        .tree_pop = &nativeUiTreePop,
        .begin_combo = &nativeUiBeginCombo,
        .end_combo = &nativeUiEndCombo,
        .selectable = &nativeUiSelectable,
        .set_item_default_focus = &nativeUiSetItemDefaultFocus,
        .image = &nativeUiImage,
        .is_item_hovered = &nativeUiIsItemHovered,
        .is_item_clicked = &nativeUiIsItemClicked,
        .is_item_double_clicked = &nativeUiIsItemDoubleClicked,
        .is_item_deactivated_after_edit = &nativeUiIsItemDeactivatedAfterEdit,
        .set_tooltip = &nativeUiSetTooltip,
        .invisible_button = &nativeUiInvisibleButton,
        .begin_section = &nativeUiBeginSection,
        .end_section = &nativeUiEndSection,
        .begin_menu = &nativeUiBeginMenu,
        .end_menu = &nativeUiEndMenu,
        .menu_item = &nativeUiMenuItem,
        .open_popup = &nativeUiOpenPopup,
        .begin_popup = &nativeUiBeginPopup,
        .begin_popup_context_item = &nativeUiBeginPopupContextItem,
        .close_current_popup = &nativeUiCloseCurrentPopup,
        .end_popup = &nativeUiEndPopup,
        .begin_drag_drop_source = &nativeUiBeginDragDropSource,
        .set_drag_drop_payload = &nativeUiSetDragDropPayload,
        .end_drag_drop_source = &nativeUiEndDragDropSource,
        .begin_drag_drop_target = &nativeUiBeginDragDropTarget,
        .accept_drag_drop_payload = &nativeUiAcceptDragDropPayload,
        .end_drag_drop_target = &nativeUiEndDragDropTarget,
        .scale = &nativeUiScale,
        .scaled = &nativeUiScaled,
        .begin_table = &nativeUiBeginTable,
        .end_table = &nativeUiEndTable,
        .table_setup_column = &nativeUiTableSetupColumn,
        .table_headers_row = &nativeUiTableHeadersRow,
        .table_next_row = &nativeUiTableNextRow,
        .table_next_column = &nativeUiTableNextColumn,
    };
}

LunaEditorCommandApi makeNativeCommandApi(NativePluginContext& context)
{
    return LunaEditorCommandApi{
        .struct_size = sizeof(LunaEditorCommandApi),
        .api_version = LUNA_EDITOR_COMMAND_API_VERSION,
        .api_user_data = &context,
        .register_command = &nativeRegisterCommand,
        .unregister_command = &nativeUnregisterCommand,
        .execute_command = &nativeExecuteCommand,
        .can_execute_command = &nativeCanExecuteCommand,
        .is_command_checked = &nativeIsCommandChecked,
    };
}

LunaEditorWindowApi makeNativeWindowApi(NativePluginContext& context)
{
    return LunaEditorWindowApi{
        .struct_size = sizeof(LunaEditorWindowApi),
        .api_version = LUNA_EDITOR_WINDOW_API_VERSION,
        .api_user_data = &context,
        .register_window = &nativeRegisterWindow,
        .unregister_window = &nativeUnregisterWindow,
        .is_window_open = &nativeIsWindowOpen,
        .set_window_open = &nativeSetWindowOpen,
    };
}

LunaEditorAssetApi makeNativeAssetApi(NativePluginContext& context)
{
    return LunaEditorAssetApi{
        .struct_size = sizeof(LunaEditorAssetApi),
        .api_version = LUNA_EDITOR_ASSET_API_VERSION,
        .api_user_data = &context,
        .describe_asset = &nativeDescribeAsset,
        .asset_info = &nativeAssetInfo,
        .asset_info_by_path = &nativeAssetInfoByPath,
        .list_assets = &nativeListAssets,
        .asset_exists = &nativeAssetExists,
        .asset_path_exists = &nativeAssetPathExists,
        .find_asset_handle_by_path = &nativeFindAssetHandleByPath,
        .assets_root_path = &nativeAssetsRootPath,
        .resolve_project_asset_path = &nativeResolveProjectAssetPath,
        .make_project_relative_asset_path = &nativeMakeProjectRelativeAssetPath,
        .refresh_assets = &nativeRefreshAssets,
        .asset_revision = &nativeAssetRevision,
        .is_asset_loading = &nativeIsAssetLoading,
        .accepts_asset_type = &nativeAcceptsAssetType,
        .mesh_submesh_count = &nativeMeshSubmeshCount,
        .begin_asset_drag_drop_source = &nativeBeginAssetDragDropSource,
    };
}

LunaEditorPluginAssetApi makeNativePluginAssetApi(NativePluginContext& context)
{
    return LunaEditorPluginAssetApi{
        .struct_size = sizeof(LunaEditorPluginAssetApi),
        .api_version = LUNA_EDITOR_PLUGIN_ASSET_API_VERSION,
        .api_user_data = &context,
        .plugin_root_path = &nativePluginRootPath,
        .asset_root_path = &nativePluginAssetRootPath,
        .resolve_path = &nativePluginAssetResolvePath,
        .exists = &nativePluginAssetExists,
        .read_text = &nativePluginAssetReadText,
        .read_bytes = &nativePluginAssetReadBytes,
        .texture = &nativePluginAssetTexture,
    };
}

LunaEditorMenuApi makeNativeMenuApi(NativePluginContext& context)
{
    return LunaEditorMenuApi{
        .struct_size = sizeof(LunaEditorMenuApi),
        .api_version = LUNA_EDITOR_MENU_API_VERSION,
        .api_user_data = &context,
        .add_menu_item = &nativeAddMenuItem,
        .remove_menu_item = &nativeRemoveMenuItem,
        .remove_menu_items_for_command = &nativeRemoveMenuItemsForCommand,
    };
}

LunaEditorProjectApi makeNativeProjectApi(NativePluginContext& context)
{
    return LunaEditorProjectApi{
        .struct_size = sizeof(LunaEditorProjectApi),
        .api_version = LUNA_EDITOR_PROJECT_API_VERSION,
        .api_user_data = &context,
        .has_project_loaded = &nativeHasProjectLoaded,
        .project_root_path = &nativeProjectRootPath,
        .project_info = &nativeProjectInfo,
        .save_project = &nativeSaveProject,
    };
}

LunaEditorSceneApi makeNativeSceneApi(NativePluginContext& context)
{
    return LunaEditorSceneApi{
        .struct_size = sizeof(LunaEditorSceneApi),
        .api_version = LUNA_EDITOR_SCENE_API_VERSION,
        .api_user_data = &context,
        .scene_label = &nativeSceneLabel,
        .entity_count = &nativeSceneEntityCount,
        .can_edit_scene = &nativeCanEditScene,
        .open_scene_file = &nativeOpenSceneFile,
        .enumerate_entities = &nativeEnumerateSceneEntities,
        .entity_exists = &nativeEntityExists,
        .entity_info = &nativeEntityInfo,
        .is_entity_descendant_of = &nativeIsEntityDescendantOf,
        .create_entity = &nativeCreateEntity,
        .create_entity_ex = &nativeCreateEntityEx,
        .destroy_entity = &nativeDestroyEntity,
        .reparent_entity = &nativeReparentEntity,
        .set_entity_name = &nativeSetEntityName,
        .get_entity_transform = &nativeGetEntityTransform,
        .set_entity_transform = &nativeSetEntityTransform,
        .get_camera_component = &nativeGetCameraComponent,
        .set_camera_component = &nativeSetCameraComponent,
        .get_light_component = &nativeGetLightComponent,
        .set_light_component = &nativeSetLightComponent,
        .get_mesh_component = &nativeGetMeshComponent,
        .set_mesh_component = &nativeSetMeshComponent,
        .add_component = &nativeAddComponent,
        .remove_component = &nativeRemoveComponent,
    };
}

LunaEditorSelectionApi makeNativeSelectionApi(NativePluginContext& context)
{
    return LunaEditorSelectionApi{
        .struct_size = sizeof(LunaEditorSelectionApi),
        .api_version = LUNA_EDITOR_SELECTION_API_VERSION,
        .api_user_data = &context,
        .selected_entity_id = &nativeSelectedEntityId,
        .select_entity = &nativeSelectEntity,
        .clear_selection = &nativeClearSelection,
    };
}

LunaEditorViewportApi makeNativeViewportApi(NativePluginContext& context)
{
    return LunaEditorViewportApi{
        .struct_size = sizeof(LunaEditorViewportApi),
        .api_version = LUNA_EDITOR_VIEWPORT_API_VERSION,
        .api_user_data = &context,
        .sync_scene_viewport = &nativeSyncSceneViewport,
        .scene_texture_view = &nativeSceneTextureView,
        .editor_camera_position = &nativeEditorCameraPosition,
        .gizmo_operation_name = &nativeGizmoOperationName,
        .gizmo_mode_name = &nativeGizmoModeName,
        .pick_debug_visualization_enabled = &nativePickDebugVisualizationEnabled,
        .set_pick_debug_visualization_enabled = &nativeSetPickDebugVisualizationEnabled,
        .editor_grid_enabled = &nativeEditorGridEnabled,
        .set_editor_grid_enabled = &nativeSetEditorGridEnabled,
        .default_scene_viewport = &nativeDefaultSceneViewport,
        .create_scene_viewport = &nativeCreateSceneViewport,
        .destroy_scene_viewport = &nativeDestroySceneViewport,
        .is_scene_viewport_valid = &nativeIsSceneViewportValid,
        .sync_scene_viewport_ex = &nativeSyncSceneViewportEx,
        .scene_texture_view_ex = &nativeSceneTextureViewEx,
    };
}

LunaEditorRuntimeViewportApi makeNativeRuntimeViewportApi(NativePluginContext& context)
{
    return LunaEditorRuntimeViewportApi{
        .struct_size = sizeof(LunaEditorRuntimeViewportApi),
        .api_version = LUNA_EDITOR_RUNTIME_VIEWPORT_API_VERSION,
        .api_user_data = &context,
        .is_runtime_viewport_enabled = &nativeIsRuntimeViewportEnabled,
        .is_runtime_viewport_requested = &nativeIsRuntimeViewportRequested,
        .set_runtime_viewport_requested = &nativeSetRuntimeViewportRequested,
        .runtime_entity_count = &nativeRuntimeEntityCount,
    };
}

} // namespace

EditorPluginManager::EditorPluginManager(EditorShell& shell)
    : m_shell(shell)
{}

EditorPluginManager::~EditorPluginManager()
{
    unloadAll();
}

void EditorPluginManager::registerPackage(EditorPluginPackage package)
{
    if (package.id.empty()) {
        LUNA_EDITOR_WARN("Ignoring editor plugin package with empty id");
        return;
    }

    const auto existing = std::find_if(m_packages.begin(), m_packages.end(), [&](const EditorPluginPackage& item) {
        return item.id == package.id;
    });
    if (existing != m_packages.end()) {
        LUNA_EDITOR_WARN("Ignoring duplicate editor plugin package '{}' from '{}'; already using '{}'",
                         package.id,
                         package.root_path.string(),
                         existing->root_path.string());
        return;
    }

    m_packages.push_back(std::move(package));
}

bool EditorPluginManager::loadRegisteredPackages()
{
    bool all_loaded = true;
    std::unordered_set<std::string> loaded_ids;
    std::vector<bool> attempted(m_packages.size(), false);

    for (std::size_t loaded_count = 0; loaded_count < m_packages.size();) {
        bool progressed = false;

        for (std::size_t index = 0; index < m_packages.size(); ++index) {
            if (attempted[index]) {
                continue;
            }

            EditorPluginPackage& package = m_packages[index];
            if (!package.enabled) {
                attempted[index] = true;
                ++loaded_count;
                progressed = true;
                continue;
            }

            if (!areEditorPluginDependenciesLoaded(package, loaded_ids)) {
                continue;
            }

            attempted[index] = true;
            ++loaded_count;
            progressed = true;

            if (loadPackage(package)) {
                loaded_ids.insert(package.id);
            } else {
                all_loaded = false;
            }
        }

        if (!progressed) {
            for (std::size_t index = 0; index < m_packages.size(); ++index) {
                if (!attempted[index]) {
                    attempted[index] = true;
                    ++loaded_count;
                    all_loaded = false;
                    LUNA_EDITOR_WARN("Editor plugin package '{}' could not load because dependencies were not met",
                                     m_packages[index].id);
                }
            }
        }
    }

    return all_loaded;
}

void EditorPluginManager::unloadAll()
{
    for (auto it = m_native_plugins.rbegin(); it != m_native_plugins.rend(); ++it) {
        if (it->plugin_api.on_unload != nullptr) {
            it->plugin_api.on_unload(it->plugin_api.plugin_user_data, it->host_api.get());
        }
        m_shell.cleanupPluginContributions(it->package.id);
    }
    m_native_plugins.clear();
    m_shell.unloadPlugins();
}

const std::vector<EditorPluginPackage>& EditorPluginManager::packages() const noexcept
{
    return m_packages;
}

bool EditorPluginManager::loadPackage(EditorPluginPackage& package)
{
    switch (package.runtime) {
        case EditorPluginRuntime::BuiltinNative:
            return loadBuiltinPackage(package);
        case EditorPluginRuntime::Native:
            return loadNativePackage(package);
        case EditorPluginRuntime::Lua:
            LUNA_EDITOR_WARN("Editor plugin package '{}' resolved entry '{}' but Lua editor plugin loading is not implemented yet",
                             package.id,
                             package.resolved_entry_path.string());
            return false;
    }

    return false;
}

bool EditorPluginManager::loadBuiltinPackage(EditorPluginPackage& package)
{
    if (!package.create) {
        LUNA_EDITOR_WARN("Editor plugin package '{}' has no factory", package.id);
        return false;
    }

    std::unique_ptr<Plugin> plugin = package.create();
    if (!plugin) {
        LUNA_EDITOR_WARN("Editor plugin package '{}' factory returned null", package.id);
        return false;
    }

    const PluginDescriptor descriptor = plugin->descriptor();
    if (descriptor.id != package.id) {
        LUNA_EDITOR_WARN("Editor plugin package id '{}' does not match plugin descriptor id '{}'",
                         package.id,
                         descriptor.id);
        return false;
    }

    if (!m_shell.loadPlugin(std::move(plugin), package.root_path)) {
        return false;
    }

    return true;
}

bool EditorPluginManager::loadNativePackage(EditorPluginPackage& package)
{
    if (package.resolved_entry_path.empty()) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' has no resolved entry path", package.id);
        return false;
    }
    if (!package.entry_exists) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' entry '{}' does not exist",
                         package.id,
                         package.resolved_entry_path.string());
        return false;
    }

    std::shared_ptr<DynamicLibrary> library = DynamicLibrary::load(package.resolved_entry_path);
    if (!library) {
        return false;
    }

    auto* create_plugin_fn =
        reinterpret_cast<LunaCreateEditorPluginFn>(library->findSymbol(LUNA_EDITOR_CREATE_PLUGIN_SYMBOL));
    if (create_plugin_fn == nullptr) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' does not export {}", package.id, LUNA_EDITOR_CREATE_PLUGIN_SYMBOL);
        return false;
    }

    NativePluginInstance instance{};
    instance.package = package;
    instance.library = std::move(library);
    instance.context = std::make_shared<NativePluginContext>();
    instance.context->shell = &m_shell;
    instance.context->plugin_id = package.id;
    m_shell.registerPluginAssetRoot(package.id, package.root_path);
    const auto cleanup_failed_load = [&]() {
        m_shell.cleanupPluginContributions(package.id);
    };
    instance.host_api = std::make_shared<LunaEditorHostApi>();
    *instance.host_api = LunaEditorHostApi{
        .struct_size = sizeof(LunaEditorHostApi),
        .api_version = LUNA_EDITOR_HOST_API_VERSION,
        .host_user_data = instance.context.get(),
        .log = makeNativeLogApi(*instance.context),
        .ui = makeNativeUiApi(*instance.context),
        .commands = makeNativeCommandApi(*instance.context),
        .windows = makeNativeWindowApi(*instance.context),
        .assets = makeNativeAssetApi(*instance.context),
        .plugin_assets = makeNativePluginAssetApi(*instance.context),
        .menus = makeNativeMenuApi(*instance.context),
        .project = makeNativeProjectApi(*instance.context),
        .scene = makeNativeSceneApi(*instance.context),
        .selection = makeNativeSelectionApi(*instance.context),
        .viewport = makeNativeViewportApi(*instance.context),
        .runtime_viewport = makeNativeRuntimeViewportApi(*instance.context),
    };
    instance.context->host_api = instance.host_api.get();
    instance.plugin_api = LunaEditorPluginApi{
        .struct_size = sizeof(LunaEditorPluginApi),
        .api_version = LUNA_EDITOR_PLUGIN_API_VERSION,
    };

    if (create_plugin_fn(LUNA_EDITOR_HOST_API_VERSION, instance.host_api.get(), &instance.plugin_api) == 0) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' failed to initialize its plugin API", package.id);
        cleanup_failed_load();
        return false;
    }
    if (instance.plugin_api.struct_size != sizeof(LunaEditorPluginApi)) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' returned plugin API struct size {} but editor expects {}",
                         package.id,
                         instance.plugin_api.struct_size,
                         static_cast<uint32_t>(sizeof(LunaEditorPluginApi)));
        cleanup_failed_load();
        return false;
    }
    if (instance.plugin_api.api_version != LUNA_EDITOR_PLUGIN_API_VERSION) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' returned API version {} but editor expects {}",
                         package.id,
                         instance.plugin_api.api_version,
                         static_cast<uint32_t>(LUNA_EDITOR_PLUGIN_API_VERSION));
        cleanup_failed_load();
        return false;
    }
    if (instance.plugin_api.plugin_id == nullptr || instance.plugin_api.plugin_id[0] == '\0') {
        LUNA_EDITOR_WARN("Native editor plugin '{}' returned an empty plugin_id", package.id);
        cleanup_failed_load();
        return false;
    }
    if (package.id != instance.plugin_api.plugin_id) {
        LUNA_EDITOR_WARN("Native editor plugin package id '{}' does not match plugin API id '{}'",
                         package.id,
                         instance.plugin_api.plugin_id);
        cleanup_failed_load();
        return false;
    }
    if (instance.plugin_api.on_load == nullptr || instance.plugin_api.on_unload == nullptr) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' must provide on_load and on_unload callbacks", package.id);
        cleanup_failed_load();
        return false;
    }
    if (instance.plugin_api.on_load(instance.plugin_api.plugin_user_data, instance.host_api.get()) == 0) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' on_load failed", package.id);
        cleanup_failed_load();
        return false;
    }

    LUNA_EDITOR_INFO("Loaded native editor plugin '{}' ({})",
                     package.id,
                     instance.plugin_api.display_name != nullptr ? instance.plugin_api.display_name : package.display_name);
    m_native_plugins.push_back(std::move(instance));
    return true;
}

} // namespace luna::editor
