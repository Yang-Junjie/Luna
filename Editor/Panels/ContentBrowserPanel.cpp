#include "Asset/AssetDatabase.h"
#include "Asset/Editor/ImageLoader.h"
#include "ContentBrowserPanel.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "EditorAssetDragDrop.h"
#include "EditorContext.h"
#include "Imgui/ImGuiContext.h"
#include "Project/ProjectManager.h"

#include <cctype>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <Device.h>
#include <imgui.h>
#include <Queue.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace {

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
    std::filesystem::path Path;
    std::string Label;
    std::string LabelLower;
    BrowserEntryKind Kind = BrowserEntryKind::OtherFile;
    luna::AssetHandle Handle = luna::AssetHandle(0);
};

struct DirectoryCache {
    std::filesystem::path Path;
    std::vector<std::filesystem::path> ChildDirectories;
    std::vector<BrowserEntry> Entries;
    bool ChildDirectoriesLoaded = false;
    bool EntriesLoaded = false;
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

int entrySortRank(BrowserEntryKind kind)
{
    return kind == BrowserEntryKind::Directory ? 0 : 1;
}

std::string makeAssetLookupKey(const std::filesystem::path& path)
{
    if (path.empty()) {
        return {};
    }

    std::filesystem::path normalized_path = path.lexically_normal();
    if (normalized_path.is_absolute()) {
        if (const auto project_root = luna::ProjectManager::instance().getProjectRootPath()) {
            std::error_code ec;
            const std::filesystem::path relative_path = std::filesystem::relative(normalized_path, *project_root, ec);
            if (!ec && !relative_path.empty() && !relative_path.is_absolute()) {
                normalized_path = relative_path.lexically_normal();
            }
        }
    }

    return normalized_path.generic_string();
}

bool matchesSearch(const BrowserEntry& entry, std::string_view filter_lower)
{
    return filter_lower.empty() || entry.LabelLower.find(filter_lower) != std::string::npos;
}

void loadChildDirectories(DirectoryCache& cache)
{
    cache.ChildDirectories.clear();

    std::error_code ec;
    for (std::filesystem::directory_iterator
             it(cache.Path, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        if (!it->is_directory(ec) || ec) {
            continue;
        }

        cache.ChildDirectories.push_back(it->path().lexically_normal());
    }

    std::sort(cache.ChildDirectories.begin(), cache.ChildDirectories.end(), [](const auto& lhs, const auto& rhs) {
        return toLower(lhs.filename().string()) < toLower(rhs.filename().string());
    });

    cache.ChildDirectoriesLoaded = true;
}

void loadDirectoryEntries(DirectoryCache& cache, const std::unordered_map<std::string, luna::AssetHandle>& asset_lookup)
{
    cache.Entries.clear();

    std::error_code ec;
    for (std::filesystem::directory_iterator
             it(cache.Path, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        const std::filesystem::path path = it->path().lexically_normal();

        if (it->is_directory(ec) && !ec) {
            cache.Entries.push_back(BrowserEntry{
                .Path = path,
                .Label = path.filename().string(),
                .LabelLower = toLower(path.filename().string()),
                .Kind = BrowserEntryKind::Directory,
            });
            continue;
        }

        if (!it->is_regular_file(ec) || ec || path.extension() == ".meta") {
            continue;
        }

        BrowserEntry entry{
            .Path = path,
            .Label = path.filename().string(),
            .LabelLower = toLower(path.filename().string()),
        };

        if (isSceneFile(path)) {
            entry.Kind = BrowserEntryKind::SceneFile;
        } else if (const auto lookup_it = asset_lookup.find(makeAssetLookupKey(path));
                   lookup_it != asset_lookup.end()) {
            entry.Kind = BrowserEntryKind::AssetFile;
            entry.Handle = lookup_it->second;
        } else {
            entry.Kind = BrowserEntryKind::OtherFile;
        }

        cache.Entries.push_back(std::move(entry));
    }

    std::sort(cache.Entries.begin(), cache.Entries.end(), [](const BrowserEntry& lhs, const BrowserEntry& rhs) {
        const int lhs_rank = entrySortRank(lhs.Kind);
        const int rhs_rank = entrySortRank(rhs.Kind);
        if (lhs_rank != rhs_rank) {
            return lhs_rank < rhs_rank;
        }

        return lhs.LabelLower < rhs.LabelLower;
    });

    cache.EntriesLoaded = true;
}

void drawEntryTooltip(const BrowserEntry& entry)
{
    if (!ImGui::IsItemHovered()) {
        return;
    }

    ImGui::BeginTooltip();
    ImGui::TextUnformatted(entry.Label.c_str());
    ImGui::TextDisabled("%s", entry.Path.lexically_normal().string().c_str());
    ImGui::EndTooltip();
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

std::filesystem::path editorAssetPath(std::string_view file_name)
{
    return std::filesystem::path(LUNA_PROJECT_ROOT) / "Editor" / "EditorAssets" / std::string(file_name);
}

luna::RHI::Ref<luna::RHI::Texture> uploadEditorIconTexture(const std::filesystem::path& path,
                                                           std::string_view debug_name)
{
    const luna::ImageData image = luna::ImageLoader::LoadImageFromFile(path.string());
    if (!image.isValid()) {
        LUNA_EDITOR_WARN("Content Browser failed to load icon '{}'", path.string());
        return {};
    }

    auto& renderer = luna::Application::get().getRenderer();
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
        ((bytes_per_row + kTextureDataPitchAlignment - 1u) / kTextureDataPitchAlignment) * kTextureDataPitchAlignment);
    const uint64_t upload_size = static_cast<uint64_t>(aligned_bytes_per_row) * static_cast<uint64_t>(image.Height);

    const auto staging_buffer = device->CreateBuffer(luna::RHI::BufferBuilder()
                                                         .SetSize(upload_size)
                                                         .SetUsage(luna::RHI::BufferUsageFlags::TransferSrc)
                                                         .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                         .SetName(std::string(debug_name) + "_Upload")
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

    const auto texture = device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(image.Width, image.Height)
            .SetFormat(image.ImageFormat)
            .SetUsage(luna::RHI::TextureUsageFlags::Sampled | luna::RHI::TextureUsageFlags::TransferDst)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName(std::string(debug_name))
            .Build());
    if (!texture) {
        return {};
    }

    const auto upload_commands = device->CreateCommandBufferEncoder();
    if (!upload_commands) {
        return {};
    }

    const luna::RHI::BufferImageCopy copy_region{
        .BufferOffset = 0,
        .BufferRowLength = bytes_per_pixel > 0 ? aligned_bytes_per_row / bytes_per_pixel : image.Width,
        .BufferImageHeight = 0,
        .ImageSubresource =
            {
                .AspectMask = luna::RHI::ImageAspectFlags::Color,
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
    const std::array<luna::RHI::BufferImageCopy, 1> copy_regions{copy_region};

    upload_commands->Begin();
    upload_commands->TransitionImage(texture, luna::RHI::ImageTransition::UndefinedToTransferDst);
    upload_commands->CopyBufferToImage(staging_buffer, texture, luna::RHI::ResourceState::CopyDest, copy_regions);
    upload_commands->TransitionImage(texture, luna::RHI::ImageTransition::TransferDstToShaderRead);
    upload_commands->End();

    graphics_queue->Submit(upload_commands);
    graphics_queue->WaitIdle();
    upload_commands->ReturnToPool();
    return texture;
}

struct EntryTileResult {
    bool Clicked = false;
    bool DoubleClicked = false;
};

EntryTileResult drawEntryTile(const BrowserEntry& entry, ImTextureID icon_texture_id, bool selected, const ImVec2& size)
{
    EntryTileResult result;

    ImGui::BeginGroup();

    const ImVec4 selected_color = ImGui::GetStyleColorVec4(ImGuiCol_Header);
    const ImVec4 hovered_color = ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered);
    const ImVec4 active_color = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);

    ImGui::PushStyleColor(ImGuiCol_Button, selected ? selected_color : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active_color);

    const ImVec2 icon_size(80.0f, 80.0f);
    if (icon_texture_id != 0) {
        result.Clicked = ImGui::ImageButton("##EntryButton", icon_texture_id, icon_size);
    } else {
        result.Clicked = ImGui::Button("##EntryButton", icon_size);
    }
    result.DoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    if (entry.Kind == BrowserEntryKind::AssetFile && entry.Handle.isValid() &&
        luna::AssetDatabase::exists(entry.Handle)) {
        luna::editor::beginAssetDragDropSource(luna::AssetDatabase::getAssetMetadata(entry.Handle),
                                               entry.Label.c_str());
    }

    ImGui::PopStyleColor(3);

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + size.x);
    ImGui::TextUnformatted(entry.Label.c_str());
    ImGui::PopTextWrapPos();

    ImGui::EndGroup();
    result.Clicked = result.Clicked || ImGui::IsItemClicked(ImGuiMouseButton_Left);
    result.DoubleClicked =
        result.DoubleClicked || (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));

    return result;
}

} // namespace

namespace luna {

struct ContentBrowserPanelState {
    std::unordered_map<std::filesystem::path, DirectoryCache> DirectoryCaches;
    std::unordered_map<std::string, AssetHandle> AssetLookup;
    std::vector<std::size_t> VisibleEntryIndices;
    std::filesystem::path VisibleEntriesDirectory;
    std::filesystem::path SelectedEntry;
    RHI::Ref<RHI::Texture> DirectoryIconTexture;
    RHI::Ref<RHI::Texture> FileIconTexture;
    ImTextureID DirectoryIconTextureId = 0;
    ImTextureID FileIconTextureId = 0;
    std::string SearchFilter;
    std::string SearchFilterLower;
    bool RefreshRequested = true;
    bool VisibleEntriesDirty = true;
};

} // namespace luna

namespace {

std::unordered_map<std::string, luna::AssetHandle> buildAssetLookup()
{
    std::unordered_map<std::string, luna::AssetHandle> lookup;
    const auto& database = luna::AssetDatabase::getDatabase();
    lookup.reserve(database.size());

    for (const auto& [handle, metadata] : database) {
        lookup.emplace(makeAssetLookupKey(metadata.FilePath), handle);
    }

    return lookup;
}

DirectoryCache& ensureDirectoryCache(luna::ContentBrowserPanelState& state,
                                     const std::filesystem::path& directory,
                                     DirectoryScanMode scan_mode)
{
    const std::filesystem::path normalized_directory = directory.lexically_normal();
    auto [it, inserted] = state.DirectoryCaches.try_emplace(normalized_directory);
    DirectoryCache& cache = it->second;
    if (inserted || cache.Path.empty()) {
        cache.Path = normalized_directory;
    }

    if (wantsChildren(scan_mode) && !cache.ChildDirectoriesLoaded) {
        loadChildDirectories(cache);
    }
    if (wantsEntries(scan_mode) && !cache.EntriesLoaded) {
        loadDirectoryEntries(cache, state.AssetLookup);
    }

    return cache;
}

void rebuildVisibleEntries(luna::ContentBrowserPanelState& state, const std::filesystem::path& directory)
{
    DirectoryCache& cache = ensureDirectoryCache(state, directory, DirectoryScanMode::Entries);
    state.VisibleEntryIndices.clear();
    state.VisibleEntryIndices.reserve(cache.Entries.size());

    for (std::size_t index = 0; index < cache.Entries.size(); ++index) {
        if (matchesSearch(cache.Entries[index], state.SearchFilterLower)) {
            state.VisibleEntryIndices.push_back(index);
        }
    }

    state.VisibleEntriesDirectory = directory.lexically_normal();
    state.VisibleEntriesDirty = false;
}

void ensureIconsLoaded(luna::ContentBrowserPanelState& state)
{
    if (!state.DirectoryIconTexture) {
        state.DirectoryIconTexture =
            uploadEditorIconTexture(editorAssetPath("DirectoryIcon.png"), "ContentBrowser_DirectoryIcon");
    }
    if (!state.FileIconTexture) {
        state.FileIconTexture = uploadEditorIconTexture(editorAssetPath("FileIcon.png"), "ContentBrowser_FileIcon");
    }

    if (state.DirectoryIconTextureId == 0 && state.DirectoryIconTexture) {
        state.DirectoryIconTextureId = luna::ImGuiRhiContext::GetTextureId(state.DirectoryIconTexture);
    }
    if (state.FileIconTextureId == 0 && state.FileIconTexture) {
        state.FileIconTextureId = luna::ImGuiRhiContext::GetTextureId(state.FileIconTexture);
    }
}

ImTextureID entryIconTextureId(const luna::ContentBrowserPanelState& state, BrowserEntryKind kind)
{
    return kind == BrowserEntryKind::Directory ? state.DirectoryIconTextureId : state.FileIconTextureId;
}

} // namespace

namespace luna {

ContentBrowserPanel::ContentBrowserPanel(EditorContext& editor_context)
    : m_editor_context(&editor_context),
      m_state(std::make_unique<ContentBrowserPanelState>())
{}

ContentBrowserPanel::~ContentBrowserPanel() = default;

void ContentBrowserPanel::onImGuiRender()
{
    if (m_state == nullptr) {
        return;
    }

    ensureIconsLoaded(*m_state);
    syncProjectDirectories();

    ImGui::SetNextWindowSize(ImVec2(880.0f, 540.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Content Browser")) {
        ImGui::End();
        return;
    }

    if (m_assets_root.empty()) {
        ImGui::TextUnformatted("No project loaded.");
        ImGui::TextDisabled("Open a .lunaproj from the Project menu to browse project assets.");
        ImGui::End();
        return;
    }

    if (m_state->VisibleEntriesDirty || m_state->VisibleEntriesDirectory != m_current_directory.lexically_normal()) {
        rebuildVisibleEntries(*m_state, m_current_directory);
    }

    drawHeader();
    ImGui::Separator();

    if (ImGui::BeginTable("##ContentBrowserLayout",
                          2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextColumn();
        if (ImGui::BeginChild("##Folders", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            drawFolderTree(m_assets_root);
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        if (ImGui::BeginChild("##Files", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            drawDirectoryContents();
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::End();
}

void ContentBrowserPanel::requestRefresh()
{
    if (m_state == nullptr) {
        return;
    }

    m_state->RefreshRequested = true;
    m_state->VisibleEntriesDirty = true;
    m_state->VisibleEntriesDirectory.clear();
}

void ContentBrowserPanel::syncProjectDirectories()
{
    if (m_state == nullptr) {
        return;
    }

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();

    if (!project_root || !project_info) {
        m_assets_root.clear();
        m_current_directory.clear();
        m_search_buffer[0] = '\0';
        m_state->SearchFilter.clear();
        m_state->SearchFilterLower.clear();
        m_state->SelectedEntry.clear();
        m_state->VisibleEntryIndices.clear();
        m_state->VisibleEntriesDirectory.clear();
        m_state->DirectoryCaches.clear();
        m_state->AssetLookup.clear();
        m_state->RefreshRequested = false;
        m_state->VisibleEntriesDirty = false;
        return;
    }

    const std::filesystem::path resolved_assets_root = (*project_root / project_info->AssetsPath).lexically_normal();
    if (m_assets_root != resolved_assets_root) {
        m_assets_root = resolved_assets_root;
        m_current_directory = resolved_assets_root;
        m_search_buffer[0] = '\0';
        m_state->SearchFilter.clear();
        m_state->SearchFilterLower.clear();
        m_state->SelectedEntry.clear();
        m_state->RefreshRequested = true;
        m_state->VisibleEntriesDirty = true;
        m_state->VisibleEntriesDirectory.clear();
    }

    if (m_state->RefreshRequested) {
        m_state->DirectoryCaches.clear();
        m_state->AssetLookup = buildAssetLookup();
        m_state->RefreshRequested = false;
        m_state->VisibleEntriesDirty = true;
        m_state->VisibleEntriesDirectory.clear();
    }

    if (m_current_directory.empty() || !isWithinAssetsRoot(m_current_directory)) {
        m_current_directory = m_assets_root;
        m_state->VisibleEntriesDirty = true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_current_directory, ec) || ec ||
        !std::filesystem::is_directory(m_current_directory, ec)) {
        m_current_directory = m_assets_root;
        m_state->VisibleEntriesDirty = true;
    }
}

void ContentBrowserPanel::drawHeader()
{
    if (m_current_directory != m_assets_root) {
        if (ImGui::ArrowButton("##ContentBrowserBack", ImGuiDir_Left)) {
            navigateTo(m_current_directory.parent_path());
        }
        ImGui::SameLine();
    }

    if (ImGui::Button("Assets")) {
        navigateTo(m_assets_root);
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        requestRefresh();
        syncProjectDirectories();
        rebuildVisibleEntries(*m_state, m_current_directory);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputTextWithHint(
            "##ContentBrowserSearch", "Search current folder", m_search_buffer.data(), m_search_buffer.size())) {
        m_state->SearchFilter = m_search_buffer.data();
        m_state->SearchFilterLower = toLower(m_state->SearchFilter);
        m_state->VisibleEntriesDirty = true;
    }

    ImGui::TextDisabled("%s", currentDirectoryLabel(m_assets_root, m_current_directory).c_str());
}

void ContentBrowserPanel::drawFolderTree(const std::filesystem::path& directory)
{
    if (m_state == nullptr) {
        return;
    }

    DirectoryCache& cache = ensureDirectoryCache(*m_state, directory, DirectoryScanMode::Children);
    const bool has_children = !cache.ChildDirectories.empty();
    const bool selected = directory == m_current_directory;
    const bool on_current_branch = isSameOrDescendant(directory, m_current_directory);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (on_current_branch) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    const std::string label = directory == m_assets_root ? "Assets" : directory.filename().string();
    const bool opened = ImGui::TreeNodeEx(directory.lexically_normal().string().c_str(), flags, "%s", label.c_str());

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        navigateTo(directory);
    }

    if (opened && has_children) {
        for (const auto& child_directory : cache.ChildDirectories) {
            drawFolderTree(child_directory);
        }
        ImGui::TreePop();
    }
}

void ContentBrowserPanel::drawDirectoryContents()
{
    if (m_state == nullptr) {
        return;
    }

    DirectoryCache& directory_cache = ensureDirectoryCache(*m_state, m_current_directory, DirectoryScanMode::Entries);
    if (m_state->VisibleEntriesDirty || m_state->VisibleEntriesDirectory != m_current_directory.lexically_normal()) {
        rebuildVisibleEntries(*m_state, m_current_directory);
    }

    const auto& visible_entries = m_state->VisibleEntryIndices;
    if (visible_entries.empty()) {
        ImGui::TextDisabled("Empty.");
        return;
    }

    const float available_width = ImGui::GetContentRegionAvail().x;
    const float tile_width = 104.0f;
    const float tile_height = 88.0f;
    const int column_count = (std::max) (1, static_cast<int>(available_width / tile_width));

    if (!ImGui::BeginTable(
            "##ContentBrowserGrid", column_count, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings)) {
        return;
    }

    for (int column = 0; column < column_count; ++column) {
        ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, tile_width);
    }

    const int row_count = static_cast<int>((visible_entries.size() + static_cast<std::size_t>(column_count) - 1) /
                                           static_cast<std::size_t>(column_count));
    ImGuiListClipper clipper;
    clipper.Begin(row_count);

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None, tile_height);

            for (int column = 0; column < column_count; ++column) {
                ImGui::TableSetColumnIndex(column);

                const std::size_t entry_index = static_cast<std::size_t>(row) * static_cast<std::size_t>(column_count) +
                                                static_cast<std::size_t>(column);
                if (entry_index >= visible_entries.size()) {
                    continue;
                }

                BrowserEntry& entry = directory_cache.Entries[visible_entries[entry_index]];
                ImGui::PushID(entry.Path.lexically_normal().string().c_str());

                const EntryTileResult tile_result = drawEntryTile(entry,
                                                                  entryIconTextureId(*m_state, entry.Kind),
                                                                  m_state->SelectedEntry == entry.Path,
                                                                  ImVec2(tile_width - 8.0f, tile_height - 8.0f));
                if (tile_result.Clicked) {
                    m_state->SelectedEntry = entry.Path;
                }

                if (tile_result.DoubleClicked) {
                    if (entry.Kind == BrowserEntryKind::Directory) {
                        navigateTo(entry.Path);
                    } else if (entry.Kind == BrowserEntryKind::SceneFile && m_editor_context != nullptr) {
                        m_editor_context->openSceneFile(entry.Path);
                    }
                }

                drawEntryTooltip(entry);
                ImGui::PopID();
            }
        }
    }

    ImGui::EndTable();
}

bool ContentBrowserPanel::navigateTo(const std::filesystem::path& directory)
{
    if (m_state == nullptr) {
        return false;
    }

    const std::filesystem::path normalized_directory = directory.lexically_normal();
    if (!isWithinAssetsRoot(normalized_directory)) {
        LUNA_EDITOR_WARN("Content Browser rejected navigation outside assets root: '{}'",
                         normalized_directory.string());
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(normalized_directory, ec) || ec ||
        !std::filesystem::is_directory(normalized_directory, ec)) {
        LUNA_EDITOR_WARN("Content Browser failed to navigate to '{}': not a valid directory",
                         normalized_directory.string());
        return false;
    }

    if (m_current_directory == normalized_directory) {
        return true;
    }

    m_current_directory = normalized_directory;
    m_state->SelectedEntry.clear();
    m_state->VisibleEntriesDirty = true;
    return true;
}

bool ContentBrowserPanel::isWithinAssetsRoot(const std::filesystem::path& directory) const
{
    if (m_assets_root.empty() || directory.empty()) {
        return false;
    }

    const std::filesystem::path normalized_root = m_assets_root.lexically_normal();
    const std::filesystem::path normalized_directory = directory.lexically_normal();
    return normalized_directory == normalized_root || isRelativeTo(normalized_directory, normalized_root) ||
           isSameOrDescendant(normalized_root, normalized_directory);
}

} // namespace luna
