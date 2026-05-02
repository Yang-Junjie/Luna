#include "Core/Log.h"
#include "ProjectManager.h"
#include "yaml-cpp/yaml.h"

#include <fstream>

namespace luna {
bool ProjectManager::loadProject(const std::filesystem::path& project_file_path)
{
    const auto previous_root_path = m_project_root_path;
    const auto previous_file_path = m_project_file_path;
    const auto previous_project_info = m_project_info;

    const auto normalized_path = project_file_path.lexically_normal();
    m_project_root_path = normalized_path.parent_path();
    m_project_file_path = normalized_path;
    m_project_info.reset();

    if (!deserializeProject()) {
        m_project_root_path = previous_root_path;
        m_project_file_path = previous_file_path;
        m_project_info = previous_project_info;
        return false;
    }

    return true;
}

std::optional<std::filesystem::path> ProjectManager::getProjectRootPath() const
{
    return m_project_root_path;
}

const std::optional<ProjectInfo>& ProjectManager::getProjectInfo() const
{
    return m_project_info;
}

void ProjectManager::setProjectInfo(const ProjectInfo& info)
{
    m_project_info = info;
}

bool ProjectManager::saveProject()
{
    return serializeProject();
}

bool ProjectManager::createProject(const std::filesystem::path& project_root_path, const ProjectInfo& info)
{
    const auto previous_root_path = m_project_root_path;
    const auto previous_file_path = m_project_file_path;
    const auto previous_project_info = m_project_info;

    const auto normalized_root_path = project_root_path.lexically_normal();
    const auto project_file_path = normalized_root_path / (info.Name + ".lunaproj");

    std::error_code exists_ec;
    const bool project_file_exists = std::filesystem::exists(project_file_path, exists_ec);
    if (exists_ec) {
        LUNA_CORE_ERROR("Failed to check project file '{}': {}", project_file_path.string(), exists_ec.message());
        return false;
    }
    if (project_file_exists) {
        LUNA_CORE_ERROR("Project file already exists: {0}", project_file_path.string());
        return false;
    }

    m_project_root_path = normalized_root_path;
    m_project_file_path = project_file_path;
    m_project_info = info;

    if (!serializeProject()) {
        m_project_root_path = previous_root_path;
        m_project_file_path = previous_file_path;
        m_project_info = previous_project_info;
        return false;
    }

    return true;
}

bool ProjectManager::serializeProject()
{
    if (!m_project_info.has_value() || !m_project_root_path.has_value() || !m_project_file_path.has_value()) {
        LUNA_CORE_ERROR("Cannot serialize project because project state is incomplete");
        return false;
    }

    const ProjectInfo& project_info = *m_project_info;
    const std::filesystem::path& project_file_path = *m_project_file_path;

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Project" << YAML::Value;
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << project_info.Name;
        out << YAML::Key << "Version" << YAML::Value << project_info.Version;
        out << YAML::Key << "Author" << YAML::Value << project_info.Author;
        out << YAML::Key << "Description" << YAML::Value << project_info.Description;
        out << YAML::Key << "StartScene" << YAML::Value << project_info.StartScene.generic_string();
        out << YAML::Key << "Assets" << YAML::Value << project_info.AssetsPath.generic_string();
        out << YAML::Key << "Scripting" << YAML::Value;
        {
            out << YAML::BeginMap;
            out << YAML::Key << "SelectedPluginId" << YAML::Value << project_info.Scripting.SelectedPluginId;
            out << YAML::Key << "SelectedBackendName" << YAML::Value << project_info.Scripting.SelectedBackendName;
            out << YAML::EndMap;
        }
    }
    out << YAML::EndMap;
    out << YAML::EndMap;

    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to emit project YAML for '{}': {}", project_file_path.string(), out.GetLastError());
        return false;
    }

    if (!project_file_path.parent_path().empty()) {
        std::error_code create_ec;
        std::filesystem::create_directories(project_file_path.parent_path(), create_ec);
        if (create_ec) {
            LUNA_CORE_ERROR("Failed to create project directory '{}': {}",
                            project_file_path.parent_path().string(),
                            create_ec.message());
            return false;
        }
    }

    std::ofstream fout(project_file_path, std::ios::out | std::ios::trunc);
    if (!fout.is_open()) {
        LUNA_CORE_ERROR("Failed to open project file: {0}", project_file_path.string());
        return false;
    }
    fout << out.c_str();
    fout.flush();
    if (!fout.good()) {
        LUNA_CORE_ERROR("Failed to write project file: {0}", project_file_path.string());
        return false;
    }

    LUNA_CORE_INFO("Project file serialized to: {0}", project_file_path.string());
    return true;
}

bool ProjectManager::deserializeProject()
{
    if (!m_project_file_path.has_value()) {
        LUNA_CORE_ERROR("Cannot deserialize project because project file path is not set");
        return false;
    }

    const std::filesystem::path& project_file_path = *m_project_file_path;
    std::error_code exists_ec;
    const bool project_file_exists = std::filesystem::exists(project_file_path, exists_ec);
    if (exists_ec) {
        LUNA_CORE_ERROR("Failed to check project file '{}': {}", project_file_path.string(), exists_ec.message());
        return false;
    }
    if (!project_file_exists) {
        LUNA_CORE_ERROR("Project file does not exist: {0}", project_file_path.string());
        return false;
    }
    YAML::Node data;
    try {
        data = YAML::LoadFile(project_file_path.string());
    } catch (const YAML::Exception& error) {
        const char* message = error.what();
        LUNA_CORE_ERROR("{}", message);
        return false;
    }

    if (!data) {
        LUNA_CORE_ERROR("Failed to load project YAML: {0}", project_file_path.string());
        return false;
    }

    YAML::Node project;
    try {
        project = data["Project"];
    } catch (const YAML::Exception& error) {
        const char* message = error.what();
        LUNA_CORE_ERROR("{}", message);
        return false;
    }

    if (project) {
        try {
            ProjectInfo info{};
            if (project["Name"]) {
                info.Name = project["Name"].as<std::string>();
            }
            if (project["Version"]) {
                info.Version = project["Version"].as<std::string>();
            }
            if (project["Author"]) {
                info.Author = project["Author"].as<std::string>();
            }
            if (project["Description"]) {
                info.Description = project["Description"].as<std::string>();
            }
            if (project["StartScene"]) {
                info.StartScene = project["StartScene"].as<std::string>();
            }
            if (project["Assets"]) {
                info.AssetsPath = project["Assets"].as<std::string>();
            }
            if (const YAML::Node scripting = project["Scripting"]) {
                if (scripting["SelectedPluginId"]) {
                    info.Scripting.SelectedPluginId = scripting["SelectedPluginId"].as<std::string>();
                }
                if (scripting["SelectedBackendName"]) {
                    info.Scripting.SelectedBackendName = scripting["SelectedBackendName"].as<std::string>();
                } else if (scripting["Backend"]) {
                    info.Scripting.SelectedBackendName = scripting["Backend"].as<std::string>();
                }
            }
            m_project_info = std::move(info);
            return true;
        } catch (const YAML::Exception& error) {
            const char* message = error.what();
            LUNA_CORE_ERROR("{}", message);
            return false;
        }
    }

    LUNA_CORE_ERROR("Project YAML does not contain a 'Project' root: {0}", project_file_path.string());
    return false;
}
} // namespace luna
