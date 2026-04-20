#pragma once

#include <array>
#include <filesystem>

namespace luna {

class LunaEditorLayer;

class ContentBrowserPanel {
public:
    explicit ContentBrowserPanel(LunaEditorLayer& editor_layer);

    void onImGuiRender();

private:
    void syncProjectDirectories();
    void drawHeader();
    void drawFolderTree(const std::filesystem::path& directory);
    void drawDirectoryContents();
    bool navigateTo(const std::filesystem::path& directory);
    bool isWithinAssetsRoot(const std::filesystem::path& directory) const;

private:
    LunaEditorLayer* m_editor_layer{nullptr};
    std::filesystem::path m_assets_root;
    std::filesystem::path m_current_directory;
    std::array<char, 128> m_search_buffer{};
};

} // namespace luna
