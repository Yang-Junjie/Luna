#include "EditorEnginePaths.h"

#include "Core/Log.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace {

constexpr std::string_view kEngineDataRootArgument = "--engine-data-root";
constexpr std::string_view kEditorPluginDirArgument = "--editor-plugin-dir";
constexpr std::string_view kEngineDataRootEnv = "LUNA_ENGINE_DATA_ROOT";
constexpr std::string_view kEditorPluginPathEnv = "LUNA_EDITOR_PLUGIN_PATH";

[[nodiscard]] std::filesystem::path sourceRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT).lexically_normal();
}

[[nodiscard]] std::filesystem::path normalizePath(std::filesystem::path path)
{
    return path.lexically_normal();
}

void appendPathIfNotEmpty(std::vector<std::filesystem::path>& paths, std::filesystem::path path)
{
    if (path.empty()) {
        return;
    }

    path = normalizePath(std::move(path));
    const auto duplicate = std::find(paths.begin(), paths.end(), path);
    if (duplicate == paths.end()) {
        paths.push_back(std::move(path));
    }
}

void appendPaths(std::vector<std::filesystem::path>& paths, const std::vector<std::filesystem::path>& values)
{
    for (const std::filesystem::path& value : values) {
        appendPathIfNotEmpty(paths, value);
    }
}

std::optional<std::string> readEnvironmentVariable(std::string_view name)
{
    const std::string name_string(name);
#if defined(_WIN32)
    char* value = nullptr;
    size_t value_size = 0;
    if (_dupenv_s(&value, &value_size, name_string.c_str()) != 0 || value == nullptr) {
        return std::nullopt;
    }

    std::string result(value);
    std::free(value);
    if (result.empty()) {
        return std::nullopt;
    }
    return result;
#else
    const char* value = std::getenv(name_string.c_str());
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

bool argumentStartsWith(std::string_view argument, std::string_view prefix) noexcept
{
    return argument.size() >= prefix.size() && argument.substr(0u, prefix.size()) == prefix;
}

std::filesystem::path defaultUserEngineDataRoot()
{
#if defined(_WIN32)
    if (const std::optional<std::string> app_data = readEnvironmentVariable("APPDATA")) {
        return normalizePath(std::filesystem::path(*app_data) / "Luna");
    }
    if (const std::optional<std::string> user_profile = readEnvironmentVariable("USERPROFILE")) {
        return normalizePath(std::filesystem::path(*user_profile) / "AppData" / "Roaming" / "Luna");
    }
    return normalizePath(sourceRoot() / "build" / "LunaEngineData");
#elif defined(__APPLE__)
    if (const std::optional<std::string> home = readEnvironmentVariable("HOME")) {
        return normalizePath(std::filesystem::path(*home) / "Library" / "Application Support" / "Luna");
    }
    return normalizePath(sourceRoot() / "build" / "LunaEngineData");
#else
    if (const std::optional<std::string> xdg_data_home = readEnvironmentVariable("XDG_DATA_HOME")) {
        return normalizePath(std::filesystem::path(*xdg_data_home) / "luna");
    }
    if (const std::optional<std::string> home = readEnvironmentVariable("HOME")) {
        return normalizePath(std::filesystem::path(*home) / ".local" / "share" / "luna");
    }
    return normalizePath(sourceRoot() / "build" / "LunaEngineData");
#endif
}

} // namespace

namespace luna::editor {

const char* editorPathListSeparator() noexcept
{
#if defined(_WIN32)
    return ";";
#else
    return ":";
#endif
}

std::vector<std::filesystem::path> splitEditorPathList(const std::string& value)
{
    std::vector<std::filesystem::path> paths;
    const char separator = editorPathListSeparator()[0];
    std::string::size_type start = 0;

    while (start <= value.size()) {
        const std::string::size_type end = value.find(separator, start);
        const std::string item =
            value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!item.empty()) {
            appendPathIfNotEmpty(paths, std::filesystem::path(item));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1u;
    }

    return paths;
}

EditorStartupOptions parseEditorStartupOptions(int argc, char** argv)
{
    EditorStartupOptions options{};

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i] != nullptr ? std::string_view(argv[i]) : std::string_view{};

        if (argument == kEngineDataRootArgument) {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                LUNA_EDITOR_WARN("Missing value after '{}'", kEngineDataRootArgument);
                continue;
            }
            options.engine_data_root_override = std::filesystem::path(argv[++i]);
            continue;
        }
        constexpr std::string_view engine_data_root_prefix = "--engine-data-root=";
        if (argumentStartsWith(argument, engine_data_root_prefix)) {
            options.engine_data_root_override =
                std::filesystem::path(argument.substr(engine_data_root_prefix.size()));
            continue;
        }

        if (argument == kEditorPluginDirArgument) {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                LUNA_EDITOR_WARN("Missing value after '{}'", kEditorPluginDirArgument);
                continue;
            }
            appendPathIfNotEmpty(options.editor_plugin_path_overrides, std::filesystem::path(argv[++i]));
            continue;
        }
        constexpr std::string_view editor_plugin_dir_prefix = "--editor-plugin-dir=";
        if (argumentStartsWith(argument, editor_plugin_dir_prefix)) {
            appendPathIfNotEmpty(options.editor_plugin_path_overrides,
                                 std::filesystem::path(argument.substr(editor_plugin_dir_prefix.size())));
            continue;
        }
    }

    return options;
}

EditorEnginePaths resolveEditorEnginePaths(const EditorStartupOptions& options)
{
    EditorEnginePaths paths{};

    if (!options.engine_data_root_override.empty()) {
        paths.engine_data_root = normalizePath(options.engine_data_root_override);
    } else if (const std::optional<std::string> env_root = readEnvironmentVariable(kEngineDataRootEnv)) {
        paths.engine_data_root = normalizePath(std::filesystem::path(*env_root));
    } else {
        paths.engine_data_root = defaultUserEngineDataRoot();
    }

    paths.engine_resources_root = normalizePath(paths.engine_data_root / "Resources");
    paths.sdk_root = normalizePath(paths.engine_data_root / "SDK" / "Editor");

    appendPathIfNotEmpty(paths.official_editor_plugin_roots,
                         paths.engine_data_root / "Plugins" / "Editor" / "Official");
    appendPathIfNotEmpty(paths.installed_editor_plugin_roots,
                         paths.engine_data_root / "Plugins" / "Editor" / "Installed");
    appendPathIfNotEmpty(paths.scripting_plugin_roots, paths.engine_data_root / "Plugins" / "Scripting");

    appendPaths(paths.development_editor_plugin_roots, options.editor_plugin_path_overrides);
    if (const std::optional<std::string> env_plugin_path = readEnvironmentVariable(kEditorPluginPathEnv)) {
        appendPaths(paths.development_editor_plugin_roots, splitEditorPathList(*env_plugin_path));
    }

    appendPathIfNotEmpty(paths.official_editor_plugin_roots,
                         sourceRoot() / "Plugins" / "Editor" / "Official");
    appendPathIfNotEmpty(paths.development_editor_plugin_roots, sourceRoot() / "Plugins" / "Editor");
    appendPathIfNotEmpty(paths.scripting_plugin_roots, sourceRoot() / "Plugins" / "Scripting");

    LUNA_EDITOR_INFO("Luna engine data root: '{}'", paths.engine_data_root.string());
    for (const std::filesystem::path& root : paths.official_editor_plugin_roots) {
        LUNA_EDITOR_INFO("Official editor plugin root: '{}'", root.string());
    }
    for (const std::filesystem::path& root : paths.installed_editor_plugin_roots) {
        LUNA_EDITOR_INFO("Installed editor plugin root: '{}'", root.string());
    }
    for (const std::filesystem::path& root : paths.development_editor_plugin_roots) {
        LUNA_EDITOR_INFO("Development editor plugin root: '{}'", root.string());
    }

    return paths;
}

} // namespace luna::editor
