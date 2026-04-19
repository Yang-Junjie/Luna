#include "Core/Log.h"
#include "ProjectManager.h"
#include "yaml-cpp/yaml.h"

#include <fstream>

namespace luna {
bool ProjectManager::loadProject(const std::filesystem::path& project_file_path)
{
    m_project_root_path = project_file_path.parent_path();
    m_project_file_path = project_file_path;
    m_project_info = ProjectInfo{};

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
    const auto project_file_path = project_root_path / (info.Name + ".lunaproj");
    if (std::filesystem::exists(project_file_path)) {
        LUNA_CORE_ERROR("Project file already exists: {0}", project_file_path.string());
        return false;
    }
    m_project_root_path = project_root_path;
    m_project_file_path = project_file_path;
    m_project_info = info;
    return serializeProject();
}

bool ProjectManager::serializeProject()
{
    if (!m_project_info.has_value() || !m_project_root_path.has_value() || !m_project_file_path.has_value()) {
        LUNA_CORE_ERROR("Cannot serialize project because project state is incomplete");
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

    std::ofstream fout(m_project_file_path.value());
    if (!fout.is_open()) {
        LUNA_CORE_ERROR("Failed to open project file: {0}", m_project_file_path.value().string());
        return false;
    }
    fout << out.c_str();

    LUNA_CORE_INFO("Project file serialized to: {0}", m_project_file_path.value().string());
    return fout.good();
}

bool ProjectManager::deserializeProject()
{
    if (!m_project_file_path.has_value()) {
        LUNA_CORE_ERROR("Cannot deserialize project because project file path is not set");
        return false;
    }

    if (!std::filesystem::exists(m_project_file_path.value())) {
        LUNA_CORE_ERROR("Project file does not exist: {0}", m_project_file_path.value().string());
        return false;
    }
    YAML::Node data = YAML::LoadFile(m_project_file_path.value().string());
    if (!data) {
        LUNA_CORE_ERROR("Failed to load project YAML: {0}", m_project_file_path.value().string());
        return false;
    }

    if (data["Project"]) {
        const auto& project = data["Project"];
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
        m_project_info = std::move(info);
    } else {
        LUNA_CORE_ERROR("Project YAML does not contain a 'Project' root: {0}", m_project_file_path.value().string());
        return false;
    }

    return true;
}
} // namespace luna
