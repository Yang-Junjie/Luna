#pragma once

#include "Renderer/ImageLoader.h"

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <vector>

namespace luna::rhi {

struct ModelData {
    struct Vertex {
        glm::vec3 Position{0.0f, 0.0f, 0.0f};
        glm::vec2 TexCoord{0.0f, 0.0f};
        glm::vec3 Normal{0.0f, 0.0f, 1.0f};
        glm::vec3 Tangent{1.0f, 0.0f, 0.0f};
        glm::vec3 Bitangent{0.0f, 1.0f, 0.0f};
    };

    struct Material {
        enum class AlphaMode : uint8_t {
            Opaque,
            Mask,
            Blend,
        };

        std::string Name;
        ImageData BaseColorTexture;
        ImageData NormalTexture;
        ImageData MetallicRoughnessTexture;
        ImageData EmissiveTexture;
        ImageData OcclusionTexture;
        glm::vec4 BaseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec3 EmissiveFactor{0.0f, 0.0f, 0.0f};
        float MetallicFactor{0.0f};
        float RoughnessFactor{1.0f};
        float NormalScale{1.0f};
        float OcclusionStrength{1.0f};
        AlphaMode AlphaModeValue{AlphaMode::Opaque};
        float AlphaCutoff{0.5f};
        bool DoubleSided{false};
        bool Unlit{false};
    };

    using Index = uint32_t;

    struct Shape {
        std::string Name;
        std::vector<Vertex> Vertices;
        std::vector<Index> Indices;
        uint32_t MaterialIndex{UINT32_MAX};
    };

    std::vector<Shape> Shapes;
    std::vector<Material> Materials;
};

class ModelLoader {
public:
    static ModelData LoadFromObj(const std::string& filepath);
    static ModelData LoadFromGltf(const std::string& filepath);
    static ModelData Load(const std::string& filepath);
};

} // namespace luna::rhi
