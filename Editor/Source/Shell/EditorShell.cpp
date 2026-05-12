#include "Shell/EditorShell.h"

#include "EditorApi/EditorApi.h"
#include "Core/Log.h"
#include "EditorStyle.h"
#include "LunaEditorLayer.h"
#include "Scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::string toString(std::string_view value)
{
    return std::string(value.data(), value.size());
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
    const size_t part_count = (std::min)(count, parts.size());
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

    void separator() override
    {
        ImGui::Separator();
    }

    void sameLine() override
    {
        ImGui::SameLine();
    }

    void spacing() override
    {
        ImGui::Spacing();
    }

    bool button(std::string_view label, Vec2 size) override
    {
        const std::string label_string = toString(label);
        return ImGui::Button(label_string.c_str(), ImVec2{size.x, size.y});
    }

    bool checkbox(std::string_view label, bool& value) override
    {
        const std::string label_string = toString(label);
        return ImGui::Checkbox(label_string.c_str(), &value);
    }

    bool sliderInt(std::string_view label, int& value, int min_value, int max_value) override
    {
        const std::string label_string = toString(label);
        return ImGui::SliderInt(label_string.c_str(), &value, min_value, max_value);
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
        const float scaled_init_width_or_weight =
            hasTableColumnFlag(flags, TableColumnFlag::WidthFixed) ? scaleEditorUi(init_width_or_weight)
                                                                   : init_width_or_weight;
        ImGui::TableSetupColumn(label_string.c_str(),
                                toImGuiTableColumnFlags(flags),
                                scaled_init_width_or_weight);
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

    bool isRuntimeViewportEnabled() const noexcept override
    {
        return m_editor_layer != nullptr && m_editor_layer->isRuntimeViewportEnabled();
    }

    bool canEditScene() const noexcept override
    {
        return !isRuntimeViewportEnabled();
    }

    EntityId createEntity(std::string name) override
    {
        if (m_editor_layer == nullptr || !canEditScene()) {
            return EntityId(0);
        }

        Entity entity = m_editor_layer->createEntity(std::move(name));
        return entity ? entity.getUUID() : EntityId(0);
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
            if (m_ui->beginWindow(descriptor.id, descriptor.title, &it->second.open, descriptor.flags)) {
                descriptor.draw(context);
            }
            m_ui->endWindow();
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
          scene_service(editor_layer),
          selection_service(editor_layer),
          rendering_service(editor_layer),
          viewport_service(editor_layer),
          history_service(editor_layer),
          command_service(shell),
          menu_service(command_service),
          window_service(shell, ui)
    {}

    EditorUi ui;
    EditorSceneService scene_service;
    EditorSelectionService selection_service;
    EditorRenderingService rendering_service;
    EditorViewportService viewport_service;
    EditorHistoryService history_service;
    EditorCommandService command_service;
    EditorMenuService menu_service;
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

ViewportService& EditorShell::viewport()
{
    return m_impl->viewport_service;
}

bool EditorShell::loadPlugin(std::unique_ptr<Plugin> plugin)
{
    if (!plugin) {
        return false;
    }

    const PluginDescriptor descriptor = plugin->descriptor();
    if (descriptor.id.empty()) {
        LUNA_EDITOR_WARN("Ignoring editor plugin with empty id");
        return false;
    }

    if (!plugin->onLoad(*this)) {
        LUNA_EDITOR_WARN("Editor plugin '{}' failed to load", descriptor.id);
        plugin->onUnload(*this);
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
            (*it)->onUnload(*this);
        }
    }
    m_impl->plugins.clear();
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
