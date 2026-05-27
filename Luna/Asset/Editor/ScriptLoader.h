#pragma once

#include "Core/Log.h"
#include "Loader.h"
#include "Project/ProjectManager.h"
#include "Script/ScriptAsset.h"
#include "Script/ScriptPluginManager.h"

#include <cctype>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <string_view>

namespace luna {

namespace script_loader_detail {

inline bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs)
{
    return lhs.size() == rhs.size() &&
           std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](char left, char right) {
               return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
           });
}

} // namespace script_loader_detail

class ScriptLoader final : public Loader {
public:
    std::shared_ptr<Asset> load(const AssetMetadata& meta_data) override
    {
        const auto project_root_path = ProjectManager::instance().getProjectRootPath();
        if (!project_root_path || meta_data.FilePath.empty()) {
            return {};
        }

        const auto project_info = ProjectManager::instance().getProjectInfo();
        const ScriptPluginSelectionResult selection =
            ScriptPluginManager::instance().resolveAndLoadProjectSelection(project_info ? &*project_info : nullptr);
        if (!selection.isResolved() || selection.Candidate == nullptr) {
            LUNA_CORE_ERROR("Failed to load script asset '{}': {}",
                            meta_data.FilePath.generic_string(),
                            selection.StatusMessage.empty() ? "no usable script plugin is selected"
                                                            : selection.StatusMessage);
            return {};
        }

        const std::string expected_language = selection.Candidate->Manifest.Language;
        const std::string metadata_language = meta_data.GetConfig<std::string>("Language", "");
        if (metadata_language.empty()) {
            LUNA_CORE_ERROR("Failed to load script asset '{}': metadata does not declare a script language",
                            meta_data.FilePath.generic_string());
            return {};
        }

        if (!script_loader_detail::equalsIgnoreCase(metadata_language, expected_language)) {
            LUNA_CORE_ERROR("Failed to load script asset '{}': metadata language '{}' does not match selected project "
                            "script language '{}'",
                            meta_data.FilePath.generic_string(),
                            metadata_language,
                            expected_language);
            return {};
        }

        const std::filesystem::path script_path = (*project_root_path / meta_data.FilePath).lexically_normal();
        std::ifstream stream(script_path, std::ios::in | std::ios::binary);
        if (!stream.is_open()) {
            LUNA_CORE_ERROR("Failed to open script asset file '{}'", script_path.string());
            return {};
        }

        auto script_asset = std::make_shared<ScriptAsset>();
        script_asset->language = metadata_language;
        script_asset->source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        return script_asset;
    }
};

} // namespace luna
