#include "Core/log.h"
#include "ProjectManager.h"
#include "yaml-cpp/yaml.h"

#include <fstream>

namespace luna {
bool ProjectManager::loadProject(const std::filesystem::path& project_file_path)
{
    m_project_root_path = project_file_path.parent_path();
    m_project_info = std::nullopt; // Clear existing project info

    return deserializeProject();
}

std::optional<std::filesystem::path> ProjectManager::getProjectRootPath()
{
    return m_project_root_path;
}

const std::optional<ProjectInfo>& ProjectManager::getProjectInfo()
{
    return m_project_info;
}

void ProjectManager::setProjectInfo(const ProjectInfo& info)
{
    m_project_info = info;
}

bool ProjectManager::createProject(const std::filesystem::path& project_root_path, const ProjectInfo& info)
{
    const auto& project_file_path = (project_root_path / (info.Name + ".lunaproj"));
    if (std::filesystem::exists(project_file_path)) {
        LUNA_CORE_ERROR("Project file already exists: {0}", project_file_path.string());
        return false;
    }
    m_project_root_path = project_root_path;
    m_project_info = info;
    return serializeProject();
}

bool ProjectManager::serializeProject()
{
    if (!m_project_info.has_value() || !m_project_root_path.has_value()) {
        LUNA_CORE_ERROR("Cannot serialize project because project info or root path is not set");
        return false;
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Project" << YAML::Value;
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << m_project_info.value().Name;
        out << YAML::Key << "Version" << YAML::Value << m_project_info.value().Version;
        out << YAML::Key << "Author" << YAML::Value << m_project_info.value().Author;
        out << YAML::Key << "Description" << YAML::Value << m_project_info.value().Description;

        out << YAML::Key << "StartScene" << YAML::Value << m_project_info.value().StartScene.string();
        out << YAML::Key << "Assets" << YAML::Value << m_project_info.value().AssetsPath.string();
    }
    out << YAML::EndMap;

    const auto& project_file_path = (m_project_root_path.value() / (m_project_info.value().Name + ".lunaproj"));
    std::ofstream fout(project_file_path);
    if (!fout.is_open()) {
        LUNA_CORE_ERROR("Failed to open project file: {0}", project_file_path.string());
        return false;
    }
    fout << out.c_str();

    LUNA_CORE_INFO("Project file serialized to: {0}", project_file_path.string());
    return fout.good();
}

bool ProjectManager::deserializeProject()
{
    const auto& project_file_path = (m_project_root_path.value() / (m_project_info.value().Name + ".lunaproj"));
    if (!std::filesystem::exists(project_file_path)) {
        LUNA_CORE_ERROR("Project file does not exist: {0}", project_file_path.string());
        return false;
    }
    YAML::Node data = YAML::LoadFile(project_file_path.string());
    if (!data) {
        LUNA_CORE_ERROR("Failed to load project YAML: {0}", project_file_path.string());
        return false;
    }

    if (data["Project"]) {
        const auto& project = data["Project"];
        m_project_info.value().Name = project["Name"].as<std::string>();
        m_project_info.value().Version = project["Version"].as<std::string>();
        m_project_info.value().Author = project["Author"].as<std::string>();
        m_project_info.value().Description = project["Description"].as<std::string>();
        m_project_info.value().StartScene = project["StartScene"].as<std::string>();
        m_project_info.value().AssetsPath = project["Assets"].as<std::string>();
    }
    return true;
}
} // namespace luna
