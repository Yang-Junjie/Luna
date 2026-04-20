#include "ContentBrowserPanel.h"

#include "Asset/AssetDatabase.h"
#include "Core/Log.h"
#include "EditorAssetDragDrop.h"
#include "LunaEditorLayer.h"
#include "Project/ProjectManager.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include <imgui.h>

namespace {

enum class BrowserEntryKind {
    Directory,
    SceneFile,
    AssetFile,
    OtherFile,
};

struct BrowserEntry {
    std::filesystem::path Path;
    std::string Label;
    BrowserEntryKind Kind = BrowserEntryKind::OtherFile;
    luna::AssetHandle Handle = luna::AssetHandle(0);
    luna::AssetType Type = luna::AssetType::None;
};

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

    if (relative_path.empty()) {
        return true;
    }

    if (relative_path.is_absolute()) {
        return false;
    }

    const std::string relative_string = relative_path.generic_string();
    return relative_string != "." && !relative_string.starts_with("..");
}

std::vector<std::filesystem::path> collectChildDirectories(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> directories;

    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        if (it->is_directory(ec) && !ec) {
            directories.push_back(it->path());
        }
    }

    std::sort(directories.begin(), directories.end(), [](const auto& lhs, const auto& rhs) {
        return toLower(lhs.filename().string()) < toLower(rhs.filename().string());
    });
    return directories;
}

std::vector<BrowserEntry> collectDirectoryEntries(const std::filesystem::path& directory)
{
    std::vector<BrowserEntry> entries;

    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        const std::filesystem::path path = it->path();

        if (it->is_directory(ec) && !ec) {
            entries.push_back(BrowserEntry{
                .Path = path,
                .Label = path.filename().string(),
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
        };

        if (isSceneFile(path)) {
            entry.Kind = BrowserEntryKind::SceneFile;
        } else {
            const luna::AssetHandle handle = luna::AssetDatabase::findHandleByFilePath(path);
            if (handle.isValid() && luna::AssetDatabase::exists(handle)) {
                const auto& metadata = luna::AssetDatabase::getAssetMetadata(handle);
                entry.Kind = BrowserEntryKind::AssetFile;
                entry.Handle = handle;
                entry.Type = metadata.Type;
            } else {
                entry.Kind = BrowserEntryKind::OtherFile;
            }
        }

        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const BrowserEntry& lhs, const BrowserEntry& rhs) {
        const auto category_rank = [](BrowserEntryKind kind) {
            switch (kind) {
                case BrowserEntryKind::Directory:
                    return 0;
                case BrowserEntryKind::SceneFile:
                    return 1;
                case BrowserEntryKind::AssetFile:
                    return 2;
                case BrowserEntryKind::OtherFile:
                default:
                    return 3;
            }
        };

        const int lhs_rank = category_rank(lhs.Kind);
        const int rhs_rank = category_rank(rhs.Kind);
        if (lhs_rank != rhs_rank) {
            return lhs_rank < rhs_rank;
        }

        return toLower(lhs.Label) < toLower(rhs.Label);
    });

    return entries;
}

bool matchesSearch(const BrowserEntry& entry, std::string_view search_filter)
{
    if (search_filter.empty()) {
        return true;
    }

    return toLower(entry.Label).find(toLower(std::string(search_filter))) != std::string::npos;
}

const char* entryBadge(const BrowserEntry& entry)
{
    switch (entry.Kind) {
        case BrowserEntryKind::Directory:
            return "DIR";
        case BrowserEntryKind::SceneFile:
            return "SCN";
        case BrowserEntryKind::AssetFile:
            return luna::AssetUtils::AssetTypeToString(entry.Type);
        case BrowserEntryKind::OtherFile:
        default:
            return "FILE";
    }
}

ImVec4 entryColor(const BrowserEntry& entry)
{
    switch (entry.Kind) {
        case BrowserEntryKind::Directory:
            return ImVec4(0.28f, 0.41f, 0.64f, 1.0f);
        case BrowserEntryKind::SceneFile:
            return ImVec4(0.27f, 0.55f, 0.36f, 1.0f);
        case BrowserEntryKind::AssetFile:
            switch (entry.Type) {
                case luna::AssetType::Mesh:
                    return ImVec4(0.51f, 0.34f, 0.16f, 1.0f);
                case luna::AssetType::Material:
                    return ImVec4(0.56f, 0.32f, 0.20f, 1.0f);
                case luna::AssetType::Texture:
                    return ImVec4(0.39f, 0.29f, 0.60f, 1.0f);
                default:
                    return ImVec4(0.33f, 0.33f, 0.35f, 1.0f);
            }
        case BrowserEntryKind::OtherFile:
        default:
            return ImVec4(0.25f, 0.25f, 0.26f, 1.0f);
    }
}

void drawEntryTooltip(const BrowserEntry& entry)
{
    if (!ImGui::IsItemHovered()) {
        return;
    }

    ImGui::BeginTooltip();
    ImGui::TextUnformatted(entry.Label.c_str());
    ImGui::TextDisabled("%s", entry.Path.lexically_normal().string().c_str());
    if (entry.Kind == BrowserEntryKind::AssetFile && entry.Handle.isValid() && luna::AssetDatabase::exists(entry.Handle)) {
        ImGui::Separator();
        ImGui::Text("Type: %s", luna::AssetUtils::AssetTypeToString(entry.Type));
        ImGui::Text("Handle: %s", entry.Handle.toString().c_str());
    }
    ImGui::EndTooltip();
}

} // namespace

namespace luna {

ContentBrowserPanel::ContentBrowserPanel(LunaEditorLayer& editor_layer)
    : m_editor_layer(&editor_layer)
{}

void ContentBrowserPanel::onImGuiRender()
{
    syncProjectDirectories();

    ImGui::SetNextWindowSize(ImVec2(880.0f, 540.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Content Browser");

    if (m_assets_root.empty()) {
        ImGui::TextUnformatted("No project loaded.");
        ImGui::TextDisabled("Open a .lunaproj from the Project menu to browse project assets.");
        ImGui::End();
        return;
    }

    drawHeader();
    ImGui::Separator();

    if (ImGui::BeginTable("##ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextColumn();
        ImGui::BeginChild("##ContentBrowserFolders");
        drawFolderTree(m_assets_root);
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("##ContentBrowserEntries");
        drawDirectoryContents();
        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::End();
}

void ContentBrowserPanel::syncProjectDirectories()
{
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();

    if (!project_root || !project_info) {
        m_assets_root.clear();
        m_current_directory.clear();
        return;
    }

    const std::filesystem::path resolved_assets_root = (*project_root / project_info->AssetsPath).lexically_normal();
    if (m_assets_root != resolved_assets_root) {
        m_assets_root = resolved_assets_root;
        m_current_directory = resolved_assets_root;
        m_search_buffer[0] = '\0';
    }

    if (m_current_directory.empty() || !isWithinAssetsRoot(m_current_directory)) {
        m_current_directory = m_assets_root;
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_current_directory, ec) || ec) {
        m_current_directory = m_assets_root;
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

    std::error_code ec;
    const std::filesystem::path relative_directory = std::filesystem::relative(m_current_directory, m_assets_root, ec);
    if (!ec && !relative_directory.empty() && relative_directory != ".") {
        std::filesystem::path partial = m_assets_root;
        for (const auto& part : relative_directory) {
            partial /= part;
            ImGui::SameLine();
            ImGui::TextUnformatted("/");
            ImGui::SameLine();
            const std::string label = part.string();
            if (ImGui::Button(label.c_str())) {
                navigateTo(partial);
            }
        }
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - 240.0f));
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##ContentBrowserSearch", "Filter current directory", m_search_buffer.data(), m_search_buffer.size());

    ImGui::TextDisabled("Current: %s", m_current_directory.lexically_normal().string().c_str());
}

void ContentBrowserPanel::drawFolderTree(const std::filesystem::path& directory)
{
    const std::vector<std::filesystem::path> subdirectories = collectChildDirectories(directory);
    const bool has_children = !subdirectories.empty();
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
    const bool opened =
        ImGui::TreeNodeEx(directory.lexically_normal().string().c_str(), flags, "%s", label.c_str());

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        navigateTo(directory);
    }

    if (opened && has_children) {
        for (const auto& child_directory : subdirectories) {
            drawFolderTree(child_directory);
        }
        ImGui::TreePop();
    }
}

void ContentBrowserPanel::drawDirectoryContents()
{
    const std::vector<BrowserEntry> all_entries = collectDirectoryEntries(m_current_directory);

    std::vector<BrowserEntry> filtered_entries;
    filtered_entries.reserve(all_entries.size());
    const std::string_view filter = m_search_buffer.data();
    for (const BrowserEntry& entry : all_entries) {
        if (matchesSearch(entry, filter)) {
            filtered_entries.push_back(entry);
        }
    }

    if (filtered_entries.empty()) {
        ImGui::TextUnformatted("No matching files or folders.");
        return;
    }

    const float desired_cell_width = 140.0f;
    const float available_width = ImGui::GetContentRegionAvail().x;
    const int column_count = (std::max)(1, static_cast<int>(available_width / desired_cell_width));

    if (!ImGui::BeginTable("##ContentBrowserGrid", column_count, ImGuiTableFlags_SizingStretchSame)) {
        return;
    }

    for (const BrowserEntry& entry : filtered_entries) {
        ImGui::TableNextColumn();
        ImGui::PushID(entry.Path.lexically_normal().string().c_str());

        ImGui::BeginGroup();

        const ImVec4 color = entryColor(entry);
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x + 0.08f, color.y + 0.08f, color.z + 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
        ImGui::Button(entryBadge(entry), ImVec2(-FLT_MIN, 72.0f));
        ImGui::PopStyleColor(3);

        if (entry.Kind == BrowserEntryKind::AssetFile && entry.Handle.isValid() && AssetDatabase::exists(entry.Handle)) {
            editor::beginAssetDragDropSource(AssetDatabase::getAssetMetadata(entry.Handle), entry.Label.c_str());
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(entry.Label.c_str());
        ImGui::PopTextWrapPos();
        ImGui::TextDisabled("%s", entryBadge(entry));
        ImGui::EndGroup();

        const bool hovered = ImGui::IsItemHovered();
        const bool double_clicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        if (entry.Kind == BrowserEntryKind::Directory && double_clicked) {
            navigateTo(entry.Path);
        } else if (entry.Kind == BrowserEntryKind::SceneFile && double_clicked && m_editor_layer != nullptr) {
            m_editor_layer->openSceneFile(entry.Path);
        }

        drawEntryTooltip(entry);
        ImGui::PopID();
    }

    ImGui::EndTable();
}

bool ContentBrowserPanel::navigateTo(const std::filesystem::path& directory)
{
    const std::filesystem::path normalized_directory = directory.lexically_normal();
    if (!isWithinAssetsRoot(normalized_directory)) {
        LUNA_EDITOR_WARN("Content Browser rejected navigation outside assets root: '{}'",
                         normalized_directory.string());
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(normalized_directory, ec) || ec || !std::filesystem::is_directory(normalized_directory, ec)) {
        LUNA_EDITOR_WARN("Content Browser failed to navigate to '{}': not a valid directory",
                         normalized_directory.string());
        return false;
    }

    m_current_directory = normalized_directory;
    LUNA_EDITOR_INFO("Content Browser current directory: '{}'", m_current_directory.string());
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
