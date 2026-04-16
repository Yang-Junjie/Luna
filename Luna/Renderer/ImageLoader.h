#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <Core.h>
#include <string>
#include <vector>

namespace luna::rhi {

struct ImageData {
    std::vector<uint8_t> ByteData;
    Cacao::Format ImageFormat{Cacao::Format::UNDEFINED};
    uint32_t Width{0};
    uint32_t Height{0};
    std::vector<std::vector<uint8_t>> MipLevels;

    bool isValid() const
    {
        return !ByteData.empty() && Width > 0 && Height > 0 && ImageFormat != Cacao::Format::UNDEFINED;
    }
};

struct CubemapData {
    std::array<std::vector<uint8_t>, 6> Faces;
    Cacao::Format FaceFormat{Cacao::Format::UNDEFINED};
    uint32_t FaceWidth{0};
    uint32_t FaceHeight{0};
};

class ImageLoader {
public:
    static ImageData LoadImageFromFile(const std::string& filepath);
    static ImageData LoadImageFromMemory(const uint8_t* data, std::size_t size, const std::string& mimeType = {});
    static CubemapData LoadCubemapImageFromFile(const std::string& filepath);
};

} // namespace luna::rhi
