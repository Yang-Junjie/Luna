#ifndef CACAO_SPARSE_TEXTURE_H
#define CACAO_SPARSE_TEXTURE_H
#include "Core.h"
#include "Texture.h"
#include <memory>
#include <vector>

namespace Cacao
{
    struct TileRegion
    {
        uint32_t X = 0, Y = 0, Z = 0;
        uint32_t Width = 1, Height = 1, Depth = 1;
        uint32_t MipLevel = 0;
        uint32_t ArrayLayer = 0;
    };

    struct SparseTextureCreateInfo
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;
        uint32_t MipLevels = 1;
        uint32_t ArrayLayers = 1;
        Format TexFormat = Format::R8G8B8A8_UNORM;
        TextureUsageFlags Usage = TextureUsageFlags::Sampled;
    };

    struct TileMapping
    {
        TileRegion Region;
        uint64_t MemoryOffset = 0;
        bool Mapped = true;
    };

    class CACAO_API SparseTexture
    {
    public:
        virtual ~SparseTexture() = default;
        virtual Ref<Texture> GetTexture() const = 0;
        virtual uint32_t GetTileWidth() const = 0;
        virtual uint32_t GetTileHeight() const = 0;
        virtual void UpdateTileMappings(const std::vector<TileMapping>& mappings) = 0;
        virtual uint64_t GetTileMemorySize() const = 0;
    };
}

#endif
