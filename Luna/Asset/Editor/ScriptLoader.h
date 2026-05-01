#pragma once

#include "Loader.h"
#include "Project/ProjectManager.h"
#include "Script/ScriptAsset.h"

#include <fstream>
#include <iterator>
#include <memory>

namespace luna {

class ScriptLoader final : public Loader {
public:
    std::shared_ptr<Asset> load(const AssetMetadata& meta_data) override
    {
        const auto project_root_path = ProjectManager::instance().getProjectRootPath();
        if (!project_root_path || meta_data.FilePath.empty()) {
            return {};
        }

        const std::filesystem::path script_path = (*project_root_path / meta_data.FilePath).lexically_normal();
        std::ifstream stream(script_path, std::ios::in | std::ios::binary);
        if (!stream.is_open()) {
            return {};
        }

        auto script_asset = std::make_shared<ScriptAsset>();
        script_asset->language = meta_data.GetConfig<std::string>("Language", "Lua");
        script_asset->source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        return script_asset;
    }
};

} // namespace luna
