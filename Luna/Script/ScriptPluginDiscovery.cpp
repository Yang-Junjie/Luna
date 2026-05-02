#include "Core/Log.h"
#include "ScriptPluginDiscovery.h"
#include "yaml-cpp/yaml.h"

#include <cctype>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
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

bool isManifestFile(const std::filesystem::path& path)
{
    const std::string filename = toLower(path.filename().string());
    return filename == "plugin.yaml" || filename == "plugin.yml";
}

const char* scopeToString(luna::ScriptPluginScope scope)
{
    switch (scope) {
        case luna::ScriptPluginScope::Engine:
            return "engine";
        case luna::ScriptPluginScope::Project:
            return "project";
        default:
            return "unknown";
    }
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
        LUNA_CORE_WARN("Skipped 'Plugin.Entry' in '{}' because it must be a string or platform map",
                       manifest_path.string());
        return std::nullopt;
    }

    std::optional<std::filesystem::path> default_entry;
    for (const auto& entry : entry_node) {
        const std::string key = entry.first.as<std::string>();
        if (!entry.second.IsScalar()) {
            LUNA_CORE_WARN("Skipped non-scalar 'Plugin.Entry.{}' in '{}'", key, manifest_path.string());
            continue;
        }

        const std::filesystem::path value = entry.second.as<std::string>();
        if (platformKeyMatchesCurrentPlatform(key)) {
            return value;
        }
        if (toLower(key) == "default") {
            default_entry = value;
        }
    }

    return default_entry;
}

std::optional<luna::ScriptPluginCandidate> loadManifest(const std::filesystem::path& manifest_path,
                                                        luna::ScriptPluginScope scope)
{
    try {
        const YAML::Node data = YAML::LoadFile(manifest_path.string());
        const YAML::Node plugin = data["Plugin"];
        if (!plugin || !plugin.IsMap()) {
            LUNA_CORE_WARN("Skipped script plugin manifest '{}' because it does not contain a 'Plugin' map",
                           manifest_path.string());
            return std::nullopt;
        }

        luna::ScriptPluginCandidate candidate{};
        candidate.Scope = scope;
        candidate.ManifestPath = manifest_path.lexically_normal();
        candidate.PluginRootPath = manifest_path.parent_path().lexically_normal();

        if (plugin["Id"]) {
            candidate.Manifest.PluginId = plugin["Id"].as<std::string>();
        }
        if (plugin["DisplayName"]) {
            candidate.Manifest.DisplayName = plugin["DisplayName"].as<std::string>();
        }
        if (plugin["Type"]) {
            candidate.Manifest.Type = plugin["Type"].as<std::string>();
        }
        if (plugin["Language"]) {
            candidate.Manifest.Language = plugin["Language"].as<std::string>();
        }
        if (plugin["Backend"]) {
            candidate.Manifest.BackendName = plugin["Backend"].as<std::string>();
        }
        if (plugin["Version"]) {
            candidate.Manifest.Version = plugin["Version"].as<std::string>();
        }
        if (plugin["HostApiVersion"]) {
            candidate.Manifest.HostApiVersion = plugin["HostApiVersion"].as<uint32_t>();
        }
        if (plugin["Entry"]) {
            if (std::optional<std::filesystem::path> entry =
                    readEntryPathForCurrentPlatform(plugin["Entry"], manifest_path)) {
                candidate.Manifest.Entry = std::move(*entry);
            }
        }

        if (!candidate.Manifest.Type.empty() && toLower(candidate.Manifest.Type) != "script") {
            return std::nullopt;
        }

        if (candidate.Manifest.PluginId.empty()) {
            LUNA_CORE_WARN("Skipped script plugin manifest '{}' because 'Plugin.Id' is empty", manifest_path.string());
            return std::nullopt;
        }

        if (candidate.Manifest.DisplayName.empty()) {
            candidate.Manifest.DisplayName = candidate.Manifest.PluginId;
        }

        if (candidate.Manifest.Language.empty()) {
            LUNA_CORE_WARN("Skipped script plugin manifest '{}' because 'Plugin.Language' is empty",
                           manifest_path.string());
            return std::nullopt;
        }

        if (candidate.Manifest.BackendName.empty()) {
            LUNA_CORE_WARN("Skipped script plugin manifest '{}' because 'Plugin.Backend' is empty",
                           manifest_path.string());
            return std::nullopt;
        }

        if (!candidate.Manifest.Entry.empty()) {
            candidate.ResolvedEntryPath =
                candidate.Manifest.Entry.is_absolute()
                    ? candidate.Manifest.Entry.lexically_normal()
                    : (candidate.PluginRootPath / candidate.Manifest.Entry).lexically_normal();

            std::error_code exists_ec;
            candidate.EntryExists = std::filesystem::exists(candidate.ResolvedEntryPath, exists_ec) && !exists_ec;
        }

        return candidate;
    } catch (const std::exception& exception) {
        LUNA_CORE_WARN("Failed to parse script plugin manifest '{}': {}", manifest_path.string(), exception.what());
        return std::nullopt;
    }
}

void appendDiscoveredPlugins(std::vector<luna::ScriptPluginCandidate>& candidates,
                             std::unordered_map<std::string, size_t>& indices_by_plugin_id,
                             const std::filesystem::path& plugins_root,
                             luna::ScriptPluginScope scope)
{
    if (plugins_root.empty()) {
        return;
    }

    std::error_code exists_ec;
    if (!std::filesystem::exists(plugins_root, exists_ec) || exists_ec) {
        if (exists_ec) {
            LUNA_CORE_WARN(
                "Failed to check script plugin directory '{}': {}", plugins_root.string(), exists_ec.message());
        }
        return;
    }

    std::error_code iterator_ec;
    std::filesystem::recursive_directory_iterator iterator(
        plugins_root, std::filesystem::directory_options::skip_permission_denied, iterator_ec);
    if (iterator_ec) {
        LUNA_CORE_WARN("Failed to scan script plugins in '{}': {}", plugins_root.string(), iterator_ec.message());
        return;
    }

    for (const std::filesystem::recursive_directory_iterator end; iterator != end; iterator.increment(iterator_ec)) {
        if (iterator_ec) {
            LUNA_CORE_WARN(
                "Failed to advance script plugin scan under '{}': {}", plugins_root.string(), iterator_ec.message());
            iterator_ec.clear();
            continue;
        }

        const auto& entry = *iterator;
        std::error_code entry_ec;
        if (!entry.is_regular_file(entry_ec) || entry_ec) {
            continue;
        }

        const std::filesystem::path manifest_path = entry.path().lexically_normal();
        if (!isManifestFile(manifest_path)) {
            continue;
        }

        std::optional<luna::ScriptPluginCandidate> candidate = loadManifest(manifest_path, scope);
        if (!candidate.has_value()) {
            continue;
        }

        const std::string normalized_id = toLower(candidate->Manifest.PluginId);
        const auto existing = indices_by_plugin_id.find(normalized_id);
        if (existing == indices_by_plugin_id.end()) {
            indices_by_plugin_id.emplace(normalized_id, candidates.size());
            candidates.push_back(std::move(*candidate));
            continue;
        }

        luna::ScriptPluginCandidate& current = candidates[existing->second];
        if (current.Scope == luna::ScriptPluginScope::Engine && candidate->Scope == luna::ScriptPluginScope::Project) {
            LUNA_CORE_INFO("Project script plugin '{}' overrides engine plugin manifest '{}'",
                           candidate->Manifest.PluginId,
                           current.ManifestPath.string());
            current = std::move(*candidate);
            continue;
        }

        LUNA_CORE_WARN("Skipped duplicate script plugin '{}' from '{}'; already using '{}'",
                       candidate->Manifest.PluginId,
                       candidate->ManifestPath.string(),
                       current.ManifestPath.string());
    }
}

} // namespace

namespace luna {

std::vector<ScriptPluginCandidate> ScriptPluginDiscovery::discover(const Options& options) const
{
    std::vector<ScriptPluginCandidate> candidates;
    std::unordered_map<std::string, size_t> indices_by_plugin_id;

    appendDiscoveredPlugins(candidates, indices_by_plugin_id, options.EnginePluginsRoot, ScriptPluginScope::Engine);
    if (options.ProjectPluginsRoot.has_value()) {
        appendDiscoveredPlugins(
            candidates, indices_by_plugin_id, options.ProjectPluginsRoot.value(), ScriptPluginScope::Project);
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.Manifest.DisplayName < rhs.Manifest.DisplayName;
    });

    LUNA_CORE_DEBUG("Discovered {} script plugin candidate(s)", candidates.size());
    for (const auto& candidate : candidates) {
        LUNA_CORE_DEBUG("Script plugin '{}' language='{}' backend='{}' scope='{}' manifest='{}'",
                        candidate.Manifest.PluginId,
                        candidate.Manifest.Language,
                        candidate.Manifest.BackendName,
                        scopeToString(candidate.Scope),
                        candidate.ManifestPath.string());
    }

    return candidates;
}

} // namespace luna
