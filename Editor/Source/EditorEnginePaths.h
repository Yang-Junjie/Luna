#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace luna::editor {

struct EditorEnginePaths {
    std::filesystem::path engine_data_root;
    std::filesystem::path engine_resources_root;
    std::filesystem::path sdk_root;
    std::vector<std::filesystem::path> official_editor_plugin_roots;
    std::vector<std::filesystem::path> installed_editor_plugin_roots;
    std::vector<std::filesystem::path> development_editor_plugin_roots;
    std::vector<std::filesystem::path> scripting_plugin_roots;
};

struct EditorStartupOptions {
    std::filesystem::path engine_data_root_override;
    std::vector<std::filesystem::path> editor_plugin_path_overrides;
};

[[nodiscard]] EditorStartupOptions parseEditorStartupOptions(int argc, char** argv);
[[nodiscard]] EditorEnginePaths resolveEditorEnginePaths(const EditorStartupOptions& options);
[[nodiscard]] const char* editorPathListSeparator() noexcept;
[[nodiscard]] std::vector<std::filesystem::path> splitEditorPathList(const std::string& value);

} // namespace luna::editor
