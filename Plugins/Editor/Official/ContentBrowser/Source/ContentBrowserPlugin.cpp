#include "ContentBrowserPlugin.h"

#include "EditorApi/EditorApi.h"
#include "Shell/EditorBuiltinPluginRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.content-browser";
constexpr const char* kWindowId = "luna.editor.content-browser.window";

enum class DirectoryScanMode : uint8_t {
    None = 0,
    Children = 1 << 0,
    Entries = 1 << 1,
};

enum class BrowserEntryKind : uint8_t {
    Directory,
    SceneFile,
    AssetFile,
    OtherFile,
};

struct BrowserEntry {
    std::filesystem::path path;
    std::string label;
    std::string label_lower;
    BrowserEntryKind kind{BrowserEntryKind::OtherFile};
    luna::editor::AssetInfo asset;
};

struct DirectoryCache {
    std::filesystem::path path;
    std::vector<std::filesystem::path> child_directories;
    std::vector<BrowserEntry> entries;
    bool child_directories_loaded{false};
    bool entries_loaded{false};
};

struct ContentBrowserState {
    std::filesystem::path assets_root;
    std::filesystem::path current_directory;
    std::filesystem::path visible_entries_directory;
    std::filesystem::path selected_entry;
    std::unordered_map<std::filesystem::path, DirectoryCache> directory_caches;
    std::vector<std::size_t> visible_entry_indices;
    std::string search_filter;
    std::string search_filter_lower;
    std::string status_message;
    luna::editor::TextureView directory_icon;
    luna::editor::TextureView file_icon;
    uint64_t asset_revision{0};
    bool refresh_requested{true};
    bool visible_entries_dirty{true};
};

bool wantsChildren(DirectoryScanMode mode)
{
    return (static_cast<uint8_t>(mode) & static_cast<uint8_t>(DirectoryScanMode::Children)) != 0;
}

bool wantsEntries(DirectoryScanMode mode)
{
    return (static_cast<uint8_t>(mode) & static_cast<uint8_t>(DirectoryScanMode::Entries)) != 0;
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isSceneFile(const std::filesystem::path& path)
{
    return toLower(path.extension().string()) == ".lunascene";
}

bool isSameOrDescendant(const std::filesystem::path& parent, const std::filesystem::path& candidate)
{
    const std::filesystem::path normalized_parent = parent.lexically_normal();
    const std::filesystem::path normalized_candidate = candidate.lexically_normal();

    auto parent_it = normalized_parent.begin();
    auto candidate_it = normalized_candidate.begin();
    for (; parent_it != normalized_parent.end(); ++parent_it, ++candidate_it) {
        if (candidate_it == normalized_candidate.end() || *candidate_it != *parent_it) {
            return false;
        }
    }

    return true;
}

bool isRelativeTo(const std::filesystem::path& candidate, const std::filesystem::path& parent)
{
    std::error_code ec;
    const std::filesystem::path relative_path = std::filesystem::relative(candidate, parent, ec);
    if (ec) {
        return false;
    }

    if (relative_path.empty() || relative_path == ".") {
        return true;
    }

    if (relative_path.is_absolute()) {
        return false;
    }

    return !relative_path.generic_string().starts_with("..");
}

bool isWithinAssetsRoot(const ContentBrowserState& state, const std::filesystem::path& directory)
{
    if (state.assets_root.empty() || directory.empty()) {
        return false;
    }

    const std::filesystem::path normalized_root = state.assets_root.lexically_normal();
    const std::filesystem::path normalized_directory = directory.lexically_normal();
    return normalized_directory == normalized_root || isRelativeTo(normalized_directory, normalized_root) ||
           isSameOrDescendant(normalized_root, normalized_directory);
}

bool matchesSearch(const BrowserEntry& entry, std::string_view filter_lower)
{
    return filter_lower.empty() || entry.label_lower.find(filter_lower) != std::string::npos;
}

int entrySortRank(BrowserEntryKind kind)
{
    return kind == BrowserEntryKind::Directory ? 0 : 1;
}

const char* entryKindLabel(const BrowserEntry& entry)
{
    switch (entry.kind) {
        case BrowserEntryKind::Directory:
            return "Folder";
        case BrowserEntryKind::SceneFile:
            return "Scene";
        case BrowserEntryKind::AssetFile:
            return entry.asset.detail.empty() ? "Asset" : entry.asset.detail.c_str();
        case BrowserEntryKind::OtherFile:
            return "File";
    }

    return "File";
}

std::string currentDirectoryLabel(const std::filesystem::path& assets_root,
                                  const std::filesystem::path& current_directory)
{
    if (assets_root.empty() || current_directory.empty()) {
        return "Assets";
    }

    std::error_code ec;
    const std::filesystem::path relative_path = std::filesystem::relative(current_directory, assets_root, ec);
    if (ec || relative_path.empty() || relative_path == ".") {
        return "Assets";
    }

    return "Assets/" + relative_path.generic_string();
}

std::string pathRelativeToAssetsLabel(const std::filesystem::path& assets_root, const std::filesystem::path& path)
{
    if (assets_root.empty() || path.empty()) {
        return {};
    }

    std::error_code ec;
    const std::filesystem::path relative_path = std::filesystem::relative(path, assets_root, ec);
    if (ec || relative_path.empty() || relative_path == ".") {
        return path.filename().string();
    }

    return relative_path.generic_string();
}

void requestRefresh(ContentBrowserState& state)
{
    state.refresh_requested = true;
    state.visible_entries_dirty = true;
    state.visible_entries_directory.clear();
}

void loadChildDirectories(DirectoryCache& cache)
{
    cache.child_directories.clear();

    std::error_code ec;
    for (std::filesystem::directory_iterator
             it(cache.path, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        if (!it->is_directory(ec) || ec) {
            continue;
        }

        cache.child_directories.push_back(it->path().lexically_normal());
    }

    std::sort(cache.child_directories.begin(), cache.child_directories.end(), [](const auto& lhs, const auto& rhs) {
        return toLower(lhs.filename().string()) < toLower(rhs.filename().string());
    });

    cache.child_directories_loaded = true;
}

void loadDirectoryEntries(luna::editor::Host& host, DirectoryCache& cache)
{
    cache.entries.clear();

    std::error_code ec;
    for (std::filesystem::directory_iterator
             it(cache.path, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        const std::filesystem::path path = it->path().lexically_normal();

        if (it->is_directory(ec) && !ec) {
            cache.entries.push_back(BrowserEntry{
                .path = path,
                .label = path.filename().string(),
                .label_lower = toLower(path.filename().string()),
                .kind = BrowserEntryKind::Directory,
            });
            continue;
        }

        if (!it->is_regular_file(ec) || ec || path.extension() == ".meta") {
            continue;
        }

        BrowserEntry entry{
            .path = path,
            .label = path.filename().string(),
            .label_lower = toLower(path.filename().string()),
        };

        if (isSceneFile(path)) {
            entry.kind = BrowserEntryKind::SceneFile;
        } else if (const std::optional<luna::editor::AssetInfo> asset = host.assets().assetInfoByPath(path)) {
            entry.kind = BrowserEntryKind::AssetFile;
            entry.asset = *asset;
        } else {
            entry.kind = BrowserEntryKind::OtherFile;
        }

        cache.entries.push_back(std::move(entry));
    }

    std::sort(cache.entries.begin(), cache.entries.end(), [](const BrowserEntry& lhs, const BrowserEntry& rhs) {
        const int lhs_rank = entrySortRank(lhs.kind);
        const int rhs_rank = entrySortRank(rhs.kind);
        if (lhs_rank != rhs_rank) {
            return lhs_rank < rhs_rank;
        }

        return lhs.label_lower < rhs.label_lower;
    });

    cache.entries_loaded = true;
}

DirectoryCache& ensureDirectoryCache(ContentBrowserState& state,
                                     luna::editor::Host& host,
                                     const std::filesystem::path& directory,
                                     DirectoryScanMode scan_mode)
{
    const std::filesystem::path normalized_directory = directory.lexically_normal();
    auto [it, inserted] = state.directory_caches.try_emplace(normalized_directory);
    DirectoryCache& cache = it->second;
    if (inserted || cache.path.empty()) {
        cache.path = normalized_directory;
    }

    if (wantsChildren(scan_mode) && !cache.child_directories_loaded) {
        loadChildDirectories(cache);
    }
    if (wantsEntries(scan_mode) && !cache.entries_loaded) {
        loadDirectoryEntries(host, cache);
    }

    return cache;
}

void rebuildVisibleEntries(ContentBrowserState& state,
                           luna::editor::Host& host,
                           const std::filesystem::path& directory)
{
    DirectoryCache& cache = ensureDirectoryCache(state, host, directory, DirectoryScanMode::Entries);
    state.visible_entry_indices.clear();
    state.visible_entry_indices.reserve(cache.entries.size());

    for (std::size_t index = 0; index < cache.entries.size(); ++index) {
        if (matchesSearch(cache.entries[index], state.search_filter_lower)) {
            state.visible_entry_indices.push_back(index);
        }
    }

    state.visible_entries_directory = directory.lexically_normal();
    state.visible_entries_dirty = false;
}

bool navigateTo(ContentBrowserState& state, const std::filesystem::path& directory)
{
    const std::filesystem::path normalized_directory = directory.lexically_normal();
    if (!isWithinAssetsRoot(state, normalized_directory)) {
        state.status_message = "Rejected navigation outside Assets.";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(normalized_directory, ec) || ec ||
        !std::filesystem::is_directory(normalized_directory, ec)) {
        state.status_message = "Target folder does not exist.";
        return false;
    }

    if (state.current_directory == normalized_directory) {
        return true;
    }

    state.current_directory = normalized_directory;
    state.selected_entry.clear();
    state.visible_entries_dirty = true;
    return true;
}

void resetProjectState(ContentBrowserState& state)
{
    state.assets_root.clear();
    state.current_directory.clear();
    state.visible_entries_directory.clear();
    state.selected_entry.clear();
    state.directory_caches.clear();
    state.visible_entry_indices.clear();
    state.search_filter.clear();
    state.search_filter_lower.clear();
    state.refresh_requested = false;
    state.visible_entries_dirty = false;
}

void syncProjectDirectories(ContentBrowserState& state, luna::editor::Host& host)
{
    const std::optional<std::filesystem::path> assets_root = host.assets().assetsRootPath();
    if (!assets_root) {
        resetProjectState(state);
        return;
    }

    const std::filesystem::path normalized_assets_root = assets_root->lexically_normal();
    const uint64_t asset_revision = host.assets().assetRevision();
    if (state.assets_root != normalized_assets_root) {
        state.assets_root = normalized_assets_root;
        state.current_directory = normalized_assets_root;
        state.selected_entry.clear();
        state.search_filter.clear();
        state.search_filter_lower.clear();
        requestRefresh(state);
    }

    if (state.asset_revision != asset_revision) {
        state.asset_revision = asset_revision;
        requestRefresh(state);
    }

    if (state.refresh_requested) {
        state.directory_caches.clear();
        state.refresh_requested = false;
        state.visible_entries_dirty = true;
        state.visible_entries_directory.clear();
    }

    if (state.current_directory.empty() || !isWithinAssetsRoot(state, state.current_directory)) {
        state.current_directory = state.assets_root;
        state.visible_entries_dirty = true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(state.current_directory, ec) || ec ||
        !std::filesystem::is_directory(state.current_directory, ec)) {
        state.current_directory = state.assets_root;
        state.visible_entries_dirty = true;
    }
}

void drawFolderTree(ContentBrowserState& state, luna::editor::Host& host, const std::filesystem::path& directory)
{
    luna::editor::Ui& ui = host.ui();
    DirectoryCache& cache = ensureDirectoryCache(state, host, directory, DirectoryScanMode::Children);
    const bool has_children = !cache.child_directories.empty();
    const bool selected = directory == state.current_directory;
    const bool on_current_branch = isSameOrDescendant(directory, state.current_directory);

    luna::editor::TreeNodeFlags flags = luna::editor::TreeNodeFlag::OpenOnArrow |
                                        luna::editor::TreeNodeFlag::SpanAvailWidth;
    if (!has_children) {
        flags = flags | luna::editor::TreeNodeFlag::Leaf | luna::editor::TreeNodeFlag::NoTreePushOnOpen;
    }
    if (selected) {
        flags = flags | luna::editor::TreeNodeFlag::Selected;
    }
    if (on_current_branch) {
        flags = flags | luna::editor::TreeNodeFlag::DefaultOpen;
    }

    const std::string label = directory == state.assets_root ? "Assets" : directory.filename().string();
    const bool opened = ui.treeNodeEx(directory.lexically_normal().generic_string(), label, flags);

    if (ui.isItemClicked(luna::editor::MouseButton::Left)) {
        navigateTo(state, directory);
    }

    if (opened && has_children) {
        for (const auto& child_directory : cache.child_directories) {
            drawFolderTree(state, host, child_directory);
        }
        ui.treePop();
    }
}

void drawEntryTooltip(luna::editor::Ui& ui, const ContentBrowserState& state, const BrowserEntry& entry)
{
    if (!ui.isItemHovered()) {
        return;
    }

    ui.setTooltip(entry.label + "\n" + pathRelativeToAssetsLabel(state.assets_root, entry.path));
}

void drawDirectoryContents(ContentBrowserState& state, luna::editor::Host& host)
{
    luna::editor::Ui& ui = host.ui();
    DirectoryCache& directory_cache =
        ensureDirectoryCache(state, host, state.current_directory, DirectoryScanMode::Entries);
    if (state.visible_entries_dirty ||
        state.visible_entries_directory != state.current_directory.lexically_normal()) {
        rebuildVisibleEntries(state, host, state.current_directory);
    }

    const auto& visible_entries = state.visible_entry_indices;
    if (visible_entries.empty()) {
        ui.textDisabled("Empty.");
        return;
    }

    const luna::editor::TableFlags table_flags =
        luna::editor::TableFlag::RowBg | luna::editor::TableFlag::BordersInnerH |
        luna::editor::TableFlag::SizingStretchProp;
    if (!ui.beginTable("##ContentBrowserEntries", 3, table_flags)) {
        return;
    }

    ui.tableSetupColumn("Name", static_cast<luna::editor::TableColumnFlags>(
                                    luna::editor::TableColumnFlag::WidthStretch),
                        0.45f);
    ui.tableSetupColumn("Type", static_cast<luna::editor::TableColumnFlags>(
                                    luna::editor::TableColumnFlag::WidthFixed),
                        96.0f);
    ui.tableSetupColumn("Path", static_cast<luna::editor::TableColumnFlags>(
                                    luna::editor::TableColumnFlag::WidthStretch),
                        0.55f);
    ui.tableHeadersRow();

    for (std::size_t visible_index : visible_entries) {
        BrowserEntry& entry = directory_cache.entries[visible_index];
        const std::string entry_id = entry.path.lexically_normal().generic_string();
        const bool selected = state.selected_entry == entry.path;

        ui.tableNextRow();
        ui.tableNextColumn();
        const luna::editor::TextureView icon =
            entry.kind == BrowserEntryKind::Directory ? state.directory_icon : state.file_icon;
        if (icon.valid()) {
            ui.image(icon, ui.scaled(luna::editor::Vec2{.x = 16.0f, .y = 16.0f}));
            ui.sameLine();
        }
        if (ui.selectable(entry.label + "###ContentBrowserEntry" + entry_id, selected)) {
            state.selected_entry = entry.path;
        }

        const bool double_clicked = ui.isItemDoubleClicked(luna::editor::MouseButton::Left);
        drawEntryTooltip(ui, state, entry);
        if (entry.kind == BrowserEntryKind::AssetFile && entry.asset.handle.isValid()) {
            host.assets().beginAssetDragDropSource(entry.asset.handle, entry.label);
        }

        if (double_clicked) {
            if (entry.kind == BrowserEntryKind::Directory) {
                navigateTo(state, entry.path);
            } else if (entry.kind == BrowserEntryKind::SceneFile) {
                if (!host.scene().openSceneFile(entry.path)) {
                    state.status_message = "Failed to open scene: " + entry.path.filename().string();
                }
            }
        }

        ui.tableNextColumn();
        ui.textDisabled(entryKindLabel(entry));

        ui.tableNextColumn();
        ui.textDisabled(pathRelativeToAssetsLabel(state.assets_root, entry.path));
    }

    ui.endTable();
}

void drawHeader(ContentBrowserState& state, luna::editor::Host& host)
{
    luna::editor::Ui& ui = host.ui();
    if (state.current_directory != state.assets_root) {
        if (ui.button("<##ContentBrowserBack", luna::editor::Vec2{.x = 28.0f, .y = 0.0f},
                      luna::editor::ButtonVariant::Subtle)) {
            navigateTo(state, state.current_directory.parent_path());
        }
        ui.sameLine();
    }

    if (ui.button("Assets")) {
        navigateTo(state, state.assets_root);
    }

    ui.sameLine();
    if (ui.button("Refresh", luna::editor::ButtonVariant::Subtle)) {
        const luna::editor::AssetRefreshResult refresh_result = host.assets().refreshAssets();
        state.asset_revision = refresh_result.revision;
        state.status_message = refresh_result.message;
        requestRefresh(state);
        syncProjectDirectories(state, host);
        rebuildVisibleEntries(state, host, state.current_directory);
    }

    ui.sameLine();
    ui.setNextItemWidth(240.0f);
    if (ui.inputTextWithHint("##ContentBrowserSearch", "Search current folder", state.search_filter, 128)) {
        state.search_filter_lower = toLower(state.search_filter);
        state.visible_entries_dirty = true;
    }

    ui.textDisabled(currentDirectoryLabel(state.assets_root, state.current_directory));
    if (!state.status_message.empty()) {
        ui.textDisabled(state.status_message);
    }
}

void drawContentBrowserWindow(ContentBrowserState& state, luna::editor::WindowDrawContext& context)
{
    luna::editor::Host& host = context.host();
    luna::editor::Ui& ui = context.ui();

    if (!state.directory_icon.valid()) {
        state.directory_icon = host.pluginAssets().texture(kPluginId, "icons/DirectoryIcon.png");
    }
    if (!state.file_icon.valid()) {
        state.file_icon = host.pluginAssets().texture(kPluginId, "icons/FileIcon.png");
    }

    syncProjectDirectories(state, host);
    if (state.assets_root.empty()) {
        ui.text("No project loaded.");
        ui.textDisabled("Open a .lunaproj from the Project menu to browse project assets.");
        return;
    }

    drawHeader(state, host);
    ui.separator();

    const luna::editor::TableFlags layout_flags =
        luna::editor::TableFlag::BordersInnerV | luna::editor::TableFlag::SizingStretchProp |
        luna::editor::TableFlag::ScrollY;
    if (!ui.beginTable("##ContentBrowserLayout", 2, layout_flags, ui.contentRegionAvail())) {
        return;
    }

    ui.tableSetupColumn("Folders", static_cast<luna::editor::TableColumnFlags>(
                                       luna::editor::TableColumnFlag::WidthFixed),
                        240.0f);
    ui.tableSetupColumn("Files", static_cast<luna::editor::TableColumnFlags>(
                                     luna::editor::TableColumnFlag::WidthStretch),
                        1.0f);
    ui.tableNextRow();

    ui.tableNextColumn();
    drawFolderTree(state, host, state.assets_root);

    ui.tableNextColumn();
    drawDirectoryContents(state, host);

    ui.endTable();
}

class ContentBrowserPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Content Browser",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Content Browser",
            .default_open = true,
            .default_size = luna::editor::Vec2{.x = 880.0f, .y = 540.0f},
            .draw =
                [this](luna::editor::WindowDrawContext& context) {
                    drawContentBrowserWindow(m_state, context);
                },
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }

private:
    ContentBrowserState m_state;
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createContentBrowserPlugin()
{
    return std::make_unique<ContentBrowserPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kContentBrowserPluginRegistration{
    kPluginId,
    createContentBrowserPlugin,
};

} // namespace

} // namespace luna::editor
