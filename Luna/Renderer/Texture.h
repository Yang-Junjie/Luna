#pragma once

// Defines texture assets used by materials and scene environment resources.
// Wraps loaded image data together with sampler settings so upload code can
// create GPU textures without depending on importer-specific logic.

#include "Asset/Asset.h"
#include "Asset/Editor/ImageLoader.h"

#include <cstdint>

#include <memory>
#include <string>
#include <utility>

namespace luna {

class Texture final : public Asset {
public:
    enum class FilterMode : uint8_t {
        Nearest,
        Linear,
    };

    enum class MipFilterMode : uint8_t {
        None,
        Nearest,
        Linear,
    };

    enum class WrapMode : uint8_t {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder,
        MirrorClampToEdge,
    };

    struct SamplerSettings {
        FilterMode MinFilter;
        FilterMode MagFilter;
        MipFilterMode MipFilter;
        WrapMode WrapU;
        WrapMode WrapV;
        WrapMode WrapW;

        constexpr SamplerSettings()
            : MinFilter(FilterMode::Linear),
              MagFilter(FilterMode::Linear),
              MipFilter(MipFilterMode::Linear),
              WrapU(WrapMode::Repeat),
              WrapV(WrapMode::Repeat),
              WrapW(WrapMode::Repeat)
        {}
    };

    struct ImportSettings {
        bool GenerateMipmaps;
        bool SRGB;
        SamplerSettings Sampler;

        constexpr ImportSettings()
            : GenerateMipmaps(true),
              SRGB(false),
              Sampler()
        {}
    };

public:
    Texture() = default;

    Texture(std::string name, ImageData image_data, ImportSettings import_settings = ImportSettings())
        : m_name(std::move(name)),
          m_image_data(std::move(image_data)),
          m_import_settings(import_settings)
    {}

    static std::shared_ptr<Texture>
        create(std::string name, ImageData image_data, ImportSettings import_settings = ImportSettings())
    {
        return std::make_shared<Texture>(std::move(name), std::move(image_data), import_settings);
    }

    AssetType getAssetsType() const override
    {
        return AssetType::Texture;
    }

    const std::string& getName() const
    {
        return m_name;
    }

    const ImageData& getImageData() const
    {
        return m_image_data;
    }

    const ImportSettings& getImportSettings() const
    {
        return m_import_settings;
    }

    const SamplerSettings& getSamplerSettings() const
    {
        return m_import_settings.Sampler;
    }

    bool usesSrgb() const
    {
        return m_import_settings.SRGB;
    }

    bool isValid() const
    {
        return m_image_data.isValid();
    }

private:
    std::string m_name;
    ImageData m_image_data;
    ImportSettings m_import_settings;
};

} // namespace luna
