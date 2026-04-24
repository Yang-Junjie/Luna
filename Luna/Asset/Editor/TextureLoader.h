#pragma once

#include "Loader.h"
#include "Renderer/Texture.h"

#include <filesystem>
#include <memory>
#include <string>

namespace luna {

class TextureLoader final : public Loader {
public:
    std::shared_ptr<Asset> load(const AssetMetadata& meta_data) override;

    static std::shared_ptr<Texture> loadFromFile(const std::filesystem::path& path, std::string asset_name = {});
    static std::shared_ptr<Texture> loadFromMetadata(const std::filesystem::path& path, const AssetMetadata& meta_data);
};

} // namespace luna
