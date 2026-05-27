#include "Core/Log.h"
#include "Shell/EditorPluginManifest.h"
#include "yaml-cpp/yaml.h"

#include <cctype>

#include <algorithm>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

std::string toLower(std::string_view value)
{
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return normalized;
}

bool isEditorPluginManifestFile(const std::filesystem::path& path)
{
    const std::string filename = toLower(path.filename().string());
    return filename == "editor-plugin.yaml" || filename == "editor-plugin.yml";
}

bool platformKeyMatchesCurrentPlatform(std::string_view key)
{
    const std::string normalized_key = toLower(key);
#if defined(_WIN32)
    return normalized_key == "windows" || normalized_key == "win32" || normalized_key == "win64";
#elif defined(__APPLE__)
    return normalized_key == "macos" || normalized_key == "mac" || normalized_key == "darwin";
#elif defined(__ANDROID__)
    return normalized_key == "android";
#elif defined(__linux__)
    return normalized_key == "linux";
#else
    return normalized_key == "default";
#endif
}

bool architectureKeyMatchesCurrentArchitecture(std::string_view key)
{
    const std::string normalized_key = toLower(key);
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return normalized_key == "x64" || normalized_key == "x86_64" || normalized_key == "amd64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return normalized_key == "arm64" || normalized_key == "aarch64";
#elif defined(_M_IX86) || defined(__i386__)
    return normalized_key == "x86" || normalized_key == "i386";
#else
    return normalized_key == "default";
#endif
}

std::optional<luna::editor::EditorPluginRuntime> readRuntime(const YAML::Node& runtime_node,
                                                             const std::filesystem::path& manifest_path)
{
    if (!runtime_node) {
        return luna::editor::EditorPluginRuntime::BuiltinNative;
    }
    if (!runtime_node.IsScalar()) {
        LUNA_EDITOR_WARN("Skipped editor plugin manifest '{}' because 'EditorPlugin.Runtime' must be a string",
                         manifest_path.string());
        return std::nullopt;
    }

    const std::string runtime = toLower(runtime_node.as<std::string>());
    if (runtime == "builtinnative" || runtime == "builtin-native" || runtime == "builtin_native") {
        return luna::editor::EditorPluginRuntime::BuiltinNative;
    }
    if (runtime == "native") {
        return luna::editor::EditorPluginRuntime::Native;
    }
    if (runtime == "lua") {
        return luna::editor::EditorPluginRuntime::Lua;
    }

    LUNA_EDITOR_WARN(
        "Skipped editor plugin manifest '{}' because runtime '{}' is unknown", manifest_path.string(), runtime);
    return std::nullopt;
}

std::optional<luna::editor::EditorPluginCategory> readCategory(const YAML::Node& category_node,
                                                               const std::filesystem::path& manifest_path)
{
    if (!category_node) {
        return luna::editor::EditorPluginCategory::Tool;
    }
    if (!category_node.IsScalar()) {
        LUNA_EDITOR_WARN("Skipped editor plugin manifest '{}' because 'EditorPlugin.Category' must be a string",
                         manifest_path.string());
        return std::nullopt;
    }

    const std::string category = toLower(category_node.as<std::string>());
    if (category == "core") {
        return luna::editor::EditorPluginCategory::Core;
    }
    if (category == "tool" || category == "tools") {
        return luna::editor::EditorPluginCategory::Tool;
    }
    if (category == "diagnostics" || category == "diagnostic" || category == "debug") {
        return luna::editor::EditorPluginCategory::Diagnostics;
    }

    LUNA_EDITOR_WARN(
        "Skipped editor plugin manifest '{}' because category '{}' is unknown", manifest_path.string(), category);
    return std::nullopt;
}

std::vector<std::string> readDependencies(const YAML::Node& dependencies_node,
                                          const std::filesystem::path& manifest_path)
{
    std::vector<std::string> dependencies;
    if (!dependencies_node) {
        return dependencies;
    }

    if (dependencies_node.IsScalar()) {
        const std::string dependency = dependencies_node.as<std::string>();
        if (!dependency.empty()) {
            dependencies.push_back(dependency);
        }
        return dependencies;
    }

    if (!dependencies_node.IsSequence()) {
        LUNA_EDITOR_WARN("Ignored 'EditorPlugin.Dependencies' in '{}' because it must be a string or sequence",
                         manifest_path.string());
        return dependencies;
    }

    for (const YAML::Node& dependency_node : dependencies_node) {
        if (!dependency_node || !dependency_node.IsScalar()) {
            LUNA_EDITOR_WARN("Ignored non-scalar dependency in '{}'", manifest_path.string());
            continue;
        }

        const std::string dependency = dependency_node.as<std::string>();
        if (!dependency.empty()) {
            dependencies.push_back(dependency);
        }
    }

    return dependencies;
}

std::optional<std::filesystem::path>
    readEntryValue(const YAML::Node& entry_value, const std::filesystem::path& manifest_path, std::string_view key_path)
{
    if (!entry_value) {
        return std::nullopt;
    }
    if (entry_value.IsScalar()) {
        return std::filesystem::path(entry_value.as<std::string>());
    }
    if (!entry_value.IsMap()) {
        LUNA_EDITOR_WARN("Skipped 'EditorPlugin.Entry.{}' in '{}' because it must be a string or architecture map",
                         key_path,
                         manifest_path.string());
        return std::nullopt;
    }

    std::optional<std::filesystem::path> default_entry;
    for (const auto& entry : entry_value) {
        const std::string key = entry.first.as<std::string>();
        if (!entry.second.IsScalar()) {
            LUNA_EDITOR_WARN(
                "Skipped non-scalar 'EditorPlugin.Entry.{}.{}' in '{}'", key_path, key, manifest_path.string());
            continue;
        }

        const std::filesystem::path value = entry.second.as<std::string>();
        if (architectureKeyMatchesCurrentArchitecture(key)) {
            return value;
        }
        if (toLower(key) == "default") {
            default_entry = value;
        }
    }

    return default_entry;
}

std::optional<std::filesystem::path> readEntryPathForCurrentPlatform(const YAML::Node& entry_node,
                                                                     const std::filesystem::path& manifest_path)
{
    if (!entry_node) {
        return std::nullopt;
    }

    if (entry_node.IsScalar()) {
        return std::filesystem::path(entry_node.as<std::string>());
    }

    if (!entry_node.IsMap()) {
        LUNA_EDITOR_WARN("Skipped 'EditorPlugin.Entry' in '{}' because it must be a string or platform map",
                         manifest_path.string());
        return std::nullopt;
    }

    std::optional<std::filesystem::path> default_entry;
    for (const auto& entry : entry_node) {
        const std::string key = entry.first.as<std::string>();
        if (platformKeyMatchesCurrentPlatform(key)) {
            return readEntryValue(entry.second, manifest_path, key);
        }
        if (toLower(key) == "default") {
            default_entry = readEntryValue(entry.second, manifest_path, key);
        }
    }

    return default_entry;
}

} // namespace

namespace luna::editor {

std::optional<EditorPluginPackage>
    EditorPluginManifestLoader::loadPackage(const std::filesystem::path& manifest_path) const
{
    try {
        const YAML::Node data = YAML::LoadFile(manifest_path.string());
        const YAML::Node plugin = data["EditorPlugin"];
        if (!plugin || !plugin.IsMap()) {
            LUNA_EDITOR_WARN("Skipped editor plugin manifest '{}' because it does not contain an 'EditorPlugin' map",
                             manifest_path.string());
            return std::nullopt;
        }

        EditorPluginPackage package{};
        package.root_path = manifest_path.parent_path().lexically_normal();

        if (plugin["Id"]) {
            package.id = plugin["Id"].as<std::string>();
        }
        if (plugin["DisplayName"]) {
            package.display_name = plugin["DisplayName"].as<std::string>();
        }
        if (plugin["Version"]) {
            package.version = plugin["Version"].as<std::string>();
        }
        if (plugin["Enabled"]) {
            package.enabled = plugin["Enabled"].as<bool>();
        }
        if (const std::optional<EditorPluginRuntime> runtime = readRuntime(plugin["Runtime"], manifest_path)) {
            package.runtime = *runtime;
        } else {
            return std::nullopt;
        }
        if (const std::optional<EditorPluginCategory> category = readCategory(plugin["Category"], manifest_path)) {
            package.category = *category;
        } else {
            return std::nullopt;
        }
        package.dependencies = readDependencies(plugin["Dependencies"], manifest_path);
        if (std::optional<std::filesystem::path> entry =
                readEntryPathForCurrentPlatform(plugin["Entry"], manifest_path)) {
            package.entry_path = std::move(*entry);
        }

        if (package.id.empty()) {
            LUNA_EDITOR_WARN("Skipped editor plugin manifest '{}' because 'EditorPlugin.Id' is empty",
                             manifest_path.string());
            return std::nullopt;
        }
        if (package.display_name.empty()) {
            package.display_name = package.id;
        }
        if (package.version.empty()) {
            package.version = "0.1.0";
        }
        if ((package.runtime == EditorPluginRuntime::Native || package.runtime == EditorPluginRuntime::Lua) &&
            package.entry_path.empty()) {
            LUNA_EDITOR_WARN(
                "Skipped editor plugin manifest '{}' because 'EditorPlugin.Entry' is required for this runtime",
                manifest_path.string());
            return std::nullopt;
        }
        if (!package.entry_path.empty()) {
            package.resolved_entry_path = package.entry_path.is_absolute()
                                              ? package.entry_path.lexically_normal()
                                              : (package.root_path / package.entry_path).lexically_normal();

            std::error_code exists_ec;
            package.entry_exists = std::filesystem::exists(package.resolved_entry_path, exists_ec) && !exists_ec;
        }

        return package;
    } catch (const std::exception& exception) {
        LUNA_EDITOR_WARN("Failed to parse editor plugin manifest '{}': {}", manifest_path.string(), exception.what());
        return std::nullopt;
    }
}

std::vector<EditorPluginPackage>
    EditorPluginManifestLoader::loadPackagesFromRoot(const std::filesystem::path& root_path) const
{
    std::vector<EditorPluginPackage> packages;
    if (root_path.empty()) {
        return packages;
    }

    std::error_code exists_ec;
    if (!std::filesystem::exists(root_path, exists_ec) || exists_ec) {
        if (exists_ec) {
            LUNA_EDITOR_WARN(
                "Failed to check editor plugin directory '{}': {}", root_path.string(), exists_ec.message());
        }
        return packages;
    }

    std::error_code iterator_ec;
    std::filesystem::recursive_directory_iterator iterator(
        root_path, std::filesystem::directory_options::skip_permission_denied, iterator_ec);
    if (iterator_ec) {
        LUNA_EDITOR_WARN("Failed to scan editor plugins in '{}': {}", root_path.string(), iterator_ec.message());
        return packages;
    }

    for (const std::filesystem::recursive_directory_iterator end; iterator != end; iterator.increment(iterator_ec)) {
        if (iterator_ec) {
            LUNA_EDITOR_WARN(
                "Failed to advance editor plugin scan under '{}': {}", root_path.string(), iterator_ec.message());
            iterator_ec.clear();
            continue;
        }

        const auto& entry = *iterator;
        std::error_code entry_ec;
        if (!entry.is_regular_file(entry_ec) || entry_ec) {
            continue;
        }

        const std::filesystem::path manifest_path = entry.path().lexically_normal();
        if (!isEditorPluginManifestFile(manifest_path)) {
            continue;
        }

        if (std::optional<EditorPluginPackage> package = loadPackage(manifest_path)) {
            packages.push_back(std::move(*package));
        }
    }

    std::sort(packages.begin(), packages.end(), [](const EditorPluginPackage& lhs, const EditorPluginPackage& rhs) {
        return lhs.display_name < rhs.display_name;
    });

    return packages;
}

} // namespace luna::editor
