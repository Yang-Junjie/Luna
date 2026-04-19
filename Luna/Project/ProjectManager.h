#pragma once
#include "ProjectInfo.h"

#include <filesystem>
#include <optional>

namespace luna {
class ProjectManager {
public:
    // open a .lunaproj file and load the project
    bool loadProject(const std::filesystem::path& project_file_path);

    std::optional<std::filesystem::path> getProjectRootPath();

    const std::optional<ProjectInfo>& getProjectInfo();
    void setProjectInfo(const ProjectInfo& info);

    bool createProject(const std::filesystem::path& project_root_path, const ProjectInfo& info);

private:
    bool serializeProject();
    bool deserializeProject();
    std::optional<ProjectInfo> m_project_info{std::nullopt};
    std::optional<std::filesystem::path> m_project_root_path{std::nullopt};
};
} // namespace luna
