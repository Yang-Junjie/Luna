#pragma once

#include "Project/ProjectInfo.h"

#include <filesystem>
#include <optional>

namespace luna::editor {

class ProjectService {
public:
    virtual ~ProjectService() = default;

    [[nodiscard]] virtual bool hasProjectLoaded() const = 0;
    [[nodiscard]] virtual std::optional<std::filesystem::path> projectRootPath() const = 0;
    [[nodiscard]] virtual std::optional<::luna::ProjectInfo> projectInfo() const = 0;
    virtual void setProjectInfo(const ::luna::ProjectInfo& info) = 0;
    virtual bool saveProject() = 0;
};

} // namespace luna::editor
