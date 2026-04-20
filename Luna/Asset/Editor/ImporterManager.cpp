#include "../AssetDatabase.h"
#include "ImporterManager.h"
#include "MaterialImporter.h"
#include "MeshImporter.h"
#include "Project/ProjectManager.h"
#include "TextureImporter.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace luna {

void ImporterManager::init()
{
    importers.clear();
    extensionToImporter.clear();

    auto register_importer = [](std::unique_ptr<Importer> importer) {
        for (const auto& ext : importer->getSupportedExtensions()) {
            extensionToImporter[importer_detail::normalizeExtension(ext)] = importer.get();
        }
        importers.push_back(std::move(importer));
    };

    register_importer(std::make_unique<MeshImporter>());
    register_importer(std::make_unique<MaterialImporter>());
    register_importer(std::make_unique<TextureImporter>());
}

void ImporterManager::import()
{
    init();

    const auto project_info = ProjectManager::instance().getProjectInfo();
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    if (!project_info || !project_root) {
        return;
    }

    const auto root = (*project_root / project_info->AssetsPath).lexically_normal();
    if (!std::filesystem::exists(root)) {
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const auto& path = entry.path();

        if (!entry.is_regular_file() || path.extension() == ".meta") {
            continue;
        }

        const std::string ext = importer_detail::normalizeExtension(path);

        if (extensionToImporter.contains(ext)) {
            auto* importer = extensionToImporter[ext];

            std::filesystem::path metaPath = path.string() + ".meta";

            if (std::filesystem::exists(metaPath)) {
                AssetMetadata metadata = importer->deserializeMetadata(metaPath);
                AssetDatabase::set(metadata.Handle, metadata);
            } else {
                AssetMetadata metadata = importer->import(path);
                importer->serializeMetadata(metadata);
                AssetDatabase::set(metadata.Handle, metadata);
            }
        }
    }
}

Importer* ImporterManager::getImporter(std::string extension)
{
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (const auto it = extensionToImporter.find(extension); it != extensionToImporter.end()) {
        return it->second;
    }

    return nullptr;
}

} // namespace luna
