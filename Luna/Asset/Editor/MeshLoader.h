#pragma once

#include "Loader.h"
#include "Renderer/Mesh.h"

#include <filesystem>
#include <memory>
#include <string>

namespace luna {

class MeshLoader final : public Loader {
public:
    std::shared_ptr<Asset> load(const AssetMetadata& meta_data) override;
    static std::shared_ptr<Mesh> loadFromFile(const std::filesystem::path& path, std::string asset_name = {});

private:
    static std::shared_ptr<Mesh> loadFromObj(const std::filesystem::path& path, std::string asset_name);
    static std::shared_ptr<Mesh> loadFromFbx(const std::filesystem::path& path, std::string asset_name);
    static std::shared_ptr<Mesh> loadFromGltf(const std::filesystem::path& path, std::string asset_name);
};

} // namespace luna
