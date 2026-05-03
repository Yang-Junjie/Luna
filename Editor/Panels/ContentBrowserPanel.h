#pragma once

#include <array>
#include <filesystem>
#include <memory>

namespace luna {

class EditorContext;
struct ContentBrowserPanelState;

class ContentBrowserPanel {
public:
    explicit ContentBrowserPanel(EditorContext& editor_context);
    ~ContentBrowserPanel();

    void onImGuiRender();
    void requestRefresh();

private:
    void syncProjectDirectories();
    void drawHeader();
    void drawFolderTree(const std::filesystem::path& directory);
    void drawDirectoryContents();
    bool navigateTo(const std::filesystem::path& directory);
    bool isWithinAssetsRoot(const std::filesystem::path& directory) const;

private:
    EditorContext* m_editor_context{nullptr};
    std::filesystem::path m_assets_root;
    std::filesystem::path m_current_directory;
    std::array<char, 128> m_search_buffer{};
    std::unique_ptr<ContentBrowserPanelState> m_state;
};

} // namespace luna
