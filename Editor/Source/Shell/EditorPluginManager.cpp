#include "Shell/EditorPluginManager.h"

#include "Core/Log.h"
#include "EditorApi/EditorCommandService.h"
#include "EditorApi/EditorMenuService.h"
#include "EditorApi/EditorUi.h"
#include "EditorApi/EditorWindowService.h"
#include "Plugins/AssetLoading/AssetLoadingPlugin.h"
#include "Plugins/BackendCapabilities/BackendCapabilitiesPlugin.h"
#include "Plugins/BuiltinMaterials/BuiltinMaterialsPlugin.h"
#include "Plugins/ContentBrowser/ContentBrowserPlugin.h"
#include "Plugins/CoreCommands/CoreCommandsPlugin.h"
#include "Plugins/EditorApiSample/EditorApiSamplePlugin.h"
#include "Plugins/Inspector/InspectorPlugin.h"
#include "Plugins/RenderDebug/RenderDebugPlugin.h"
#include "Plugins/RenderFeatures/RenderFeaturesPlugin.h"
#include "Plugins/RenderProfiler/RenderProfilerPlugin.h"
#include "Plugins/SceneHierarchy/SceneHierarchyPlugin.h"
#include "Plugins/SceneSettings/SceneSettingsPlugin.h"
#include "Plugins/SceneStatus/SceneStatusPlugin.h"
#include "Plugins/ScriptPlugins/ScriptPluginsPlugin.h"
#include "Plugins/Viewport/ViewportPlugin.h"
#include "Shell/EditorBuiltinPluginRegistry.h"
#include "Shell/EditorPluginDependencyResolver.h"
#include "Shell/EditorPluginManifest.h"
#include "Shell/EditorShell.h"

#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <utility>

namespace {

std::filesystem::path officialPluginRoot(std::string_view directory_name)
{
    return (std::filesystem::path(LUNA_PROJECT_ROOT) / "Editor" / "Source" / "Plugins" / directory_name)
        .lexically_normal();
}

std::filesystem::path officialPluginsRoot()
{
    return (std::filesystem::path(LUNA_PROJECT_ROOT) / "Editor" / "Source" / "Plugins").lexically_normal();
}

std::filesystem::path sourceEditorPluginsRoot()
{
    return (std::filesystem::path(LUNA_PROJECT_ROOT) / "Plugins" / "Editor").lexically_normal();
}

struct BuiltinFactoryEntry {
    const char* id;
    const char* display_name;
    const char* directory_name;
    luna::editor::EditorBuiltinPluginFactory create;
};

const std::vector<BuiltinFactoryEntry>& officialFactoryEntries()
{
    static const std::vector<BuiltinFactoryEntry> entries{
        {"luna.editor.core-commands", "Core Editor Commands", "CoreCommands", luna::editor::createCoreCommandsPlugin},
        {"luna.editor.content-browser", "Content Browser", "ContentBrowser", luna::editor::createContentBrowserPlugin},
        {"luna.editor.viewport", "Viewport", "Viewport", luna::editor::createViewportPlugin},
        {"luna.editor.scene-hierarchy", "Scene Hierarchy", "SceneHierarchy", luna::editor::createSceneHierarchyPlugin},
        {"luna.editor.inspector", "Inspector", "Inspector", luna::editor::createInspectorPlugin},
        {"luna.editor.scene-status", "Scene Status", "SceneStatus", luna::editor::createSceneStatusPlugin},
        {"luna.editor.scene-settings", "Scene Settings", "SceneSettings", luna::editor::createSceneSettingsPlugin},
        {"luna.editor.asset-loading", "Asset Loading", "AssetLoading", luna::editor::createAssetLoadingPlugin},
        {"luna.editor.backend-capabilities",
         "Backend Capabilities",
         "BackendCapabilities",
         luna::editor::createBackendCapabilitiesPlugin},
        {"luna.editor.render-debug", "Render Debug", "RenderDebug", luna::editor::createRenderDebugPlugin},
        {"luna.editor.render-features", "Render Features", "RenderFeatures", luna::editor::createRenderFeaturesPlugin},
        {"luna.editor.render-profiler", "Render Profiler", "RenderProfiler", luna::editor::createRenderProfilerPlugin},
        {"luna.editor.builtin-materials",
         "Builtin Materials",
         "BuiltinMaterials",
         luna::editor::createBuiltinMaterialsPlugin},
        {"luna.editor.script-plugins", "Script Plugins", "ScriptPlugins", luna::editor::createScriptPluginsPlugin},
        {"luna.editor.api-sample", "Editor API Sample", "EditorApiSample", luna::editor::createEditorApiSamplePlugin},
    };
    return entries;
}

std::vector<std::string> defaultOfficialDependencies(std::string_view plugin_id)
{
    if (plugin_id == "luna.editor.core-commands") {
        return {};
    }
    return {"luna.editor.core-commands"};
}

luna::editor::EditorPluginPackage makeFallbackOfficialPackage(
    std::string id,
    std::string display_name,
    std::string_view directory_name,
    luna::editor::EditorBuiltinPluginFactory create)
{
    std::vector<std::string> dependencies = defaultOfficialDependencies(id);
    return luna::editor::EditorPluginPackage{
        .id = std::move(id),
        .display_name = std::move(display_name),
        .version = "0.1.0",
        .root_path = officialPluginRoot(directory_name),
        .runtime = luna::editor::EditorPluginRuntime::BuiltinNative,
        .dependencies = std::move(dependencies),
        .enabled = true,
        .create = std::move(create),
    };
}

void registerOfficialBuiltinFactories()
{
    static const bool registered = [] {
        for (const BuiltinFactoryEntry& entry : officialFactoryEntries()) {
            (void) luna::editor::EditorBuiltinPluginRegistry::registerFactory(entry.id, entry.create);
        }
        return true;
    }();
    (void) registered;
}

void attachBuiltinFactories(std::vector<luna::editor::EditorPluginPackage>& packages)
{
    for (luna::editor::EditorPluginPackage& package : packages) {
        if (luna::editor::EditorBuiltinPluginFactory factory =
                luna::editor::EditorBuiltinPluginRegistry::findFactory(package.id)) {
            package.create = std::move(factory);
        }
    }
}

void appendMissingOfficialFallbackPackages(std::vector<luna::editor::EditorPluginPackage>& packages)
{
    std::unordered_set<std::string> package_ids;
    for (const luna::editor::EditorPluginPackage& package : packages) {
        package_ids.insert(package.id);
    }

    for (const BuiltinFactoryEntry& entry : officialFactoryEntries()) {
        if (package_ids.find(entry.id) != package_ids.end()) {
            continue;
        }

        LUNA_EDITOR_WARN("Official editor plugin '{}' has no editor-plugin.yaml; using built-in package fallback",
                         entry.id);
        packages.push_back(
            makeFallbackOfficialPackage(entry.id, entry.display_name, entry.directory_name, entry.create));
    }
}

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
        m_shell.unregisterNativePluginContributions(it->package.id);
    }
    m_native_plugins.clear();
    m_builtin_materials_plugin = nullptr;
    m_shell.unloadPlugins();
}

void EditorPluginManager::focusBuiltinMaterial(AssetHandle material_handle)
{
    if (m_builtin_materials_plugin != nullptr && material_handle.isValid()) {
        m_builtin_materials_plugin->focusMaterial(material_handle);
    }
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

    Plugin* plugin_ptr = plugin.get();
    if (!m_shell.loadPlugin(std::move(plugin), package.root_path)) {
        return false;
    }

    rememberLoadedPlugin(package, *plugin_ptr);
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
    instance.host_api = std::make_shared<LunaEditorHostApi>();
    *instance.host_api = LunaEditorHostApi{
        .struct_size = sizeof(LunaEditorHostApi),
        .api_version = LUNA_EDITOR_HOST_API_VERSION,
        .host_user_data = instance.context.get(),
        .log = makeNativeLogApi(*instance.context),
        .ui = makeNativeUiApi(*instance.context),
        .commands = makeNativeCommandApi(*instance.context),
        .windows = makeNativeWindowApi(*instance.context),
    };
    instance.context->host_api = instance.host_api.get();
    instance.plugin_api = LunaEditorPluginApi{
        .struct_size = sizeof(LunaEditorPluginApi),
        .api_version = LUNA_EDITOR_PLUGIN_API_VERSION,
    };

    if (create_plugin_fn(LUNA_EDITOR_HOST_API_VERSION, instance.host_api.get(), &instance.plugin_api) == 0) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' failed to initialize its plugin API", package.id);
        return false;
    }
    if (instance.plugin_api.struct_size != sizeof(LunaEditorPluginApi)) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' returned plugin API struct size {} but editor expects {}",
                         package.id,
                         instance.plugin_api.struct_size,
                         static_cast<uint32_t>(sizeof(LunaEditorPluginApi)));
        return false;
    }
    if (instance.plugin_api.api_version != LUNA_EDITOR_PLUGIN_API_VERSION) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' returned API version {} but editor expects {}",
                         package.id,
                         instance.plugin_api.api_version,
                         static_cast<uint32_t>(LUNA_EDITOR_PLUGIN_API_VERSION));
        return false;
    }
    if (instance.plugin_api.plugin_id == nullptr || instance.plugin_api.plugin_id[0] == '\0') {
        LUNA_EDITOR_WARN("Native editor plugin '{}' returned an empty plugin_id", package.id);
        return false;
    }
    if (package.id != instance.plugin_api.plugin_id) {
        LUNA_EDITOR_WARN("Native editor plugin package id '{}' does not match plugin API id '{}'",
                         package.id,
                         instance.plugin_api.plugin_id);
        return false;
    }
    if (instance.plugin_api.on_load == nullptr || instance.plugin_api.on_unload == nullptr) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' must provide on_load and on_unload callbacks", package.id);
        return false;
    }
    if (instance.plugin_api.on_load(instance.plugin_api.plugin_user_data, instance.host_api.get()) == 0) {
        LUNA_EDITOR_WARN("Native editor plugin '{}' on_load failed", package.id);
        m_shell.unregisterNativePluginContributions(package.id);
        return false;
    }

    LUNA_EDITOR_INFO("Loaded native editor plugin '{}' ({})",
                     package.id,
                     instance.plugin_api.display_name != nullptr ? instance.plugin_api.display_name : package.display_name);
    m_native_plugins.push_back(std::move(instance));
    return true;
}

void EditorPluginManager::rememberLoadedPlugin(const EditorPluginPackage& package, Plugin& plugin)
{
    if (package.id == "luna.editor.builtin-materials") {
        m_builtin_materials_plugin = dynamic_cast<BuiltinMaterialsPlugin*>(&plugin);
    }
}

std::vector<EditorPluginPackage> createEditorPluginPackages()
{
    registerOfficialBuiltinFactories();

    EditorPluginManifestLoader manifest_loader;
    std::vector<EditorPluginPackage> packages = manifest_loader.loadPackagesFromRoot(officialPluginsRoot());
    attachBuiltinFactories(packages);
    appendMissingOfficialFallbackPackages(packages);

    std::vector<EditorPluginPackage> source_packages = manifest_loader.loadPackagesFromRoot(sourceEditorPluginsRoot());
    attachBuiltinFactories(source_packages);
    packages.insert(packages.end(),
                    std::make_move_iterator(source_packages.begin()),
                    std::make_move_iterator(source_packages.end()));

    return packages;
}

} // namespace luna::editor
