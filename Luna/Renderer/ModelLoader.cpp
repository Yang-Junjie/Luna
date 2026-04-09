// Copyright(c) 2021, #Momo
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
// 
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and /or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "Renderer/ModelLoader.h"

#include "Vulkan/ArrayUtils.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <filesystem>
#include <optional>
#include <string_view>

#define TINYOBJLOADER_IMPLEMENTATION
#include "third_party/tinyobjloader/tiny_obj_loader.h"

namespace VulkanAbstractionLayer
{
    namespace
    {
        std::string GetAbsolutePathToObjResource(const std::string& objPath, const std::string& relativePath)
        {
            return (std::filesystem::path{ objPath }.parent_path() / relativePath).string();
        }

        ImageData CreateStubTexture(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a)
        {
            return ImageData{ std::vector<uint8_t>{ r, g, b, a }, Format::R8G8B8A8_UNORM, 1, 1 };
        }

        Vector2 ToVector2(const fastgltf::math::fvec2& value)
        {
            return Vector2{ value[0], value[1] };
        }

        Vector3 ToVector3(const fastgltf::math::fvec3& value)
        {
            return Vector3{ value[0], value[1], value[2] };
        }

        std::vector<std::pair<Vector3, Vector3>> ComputeTangentsBitangents(const tinyobj::mesh_t& mesh, const tinyobj::attrib_t& attrib)
        {
            std::vector<std::pair<Vector3, Vector3>> tangentsBitangents;
            tangentsBitangents.resize(attrib.vertices.size() / 3, { Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } });

            ModelData::Index indexOffset = 0;
            for (const size_t faceIndex : mesh.num_face_vertices)
            {
                assert(faceIndex == 3);

                const tinyobj::index_t idx1 = mesh.indices[indexOffset + 0];
                const tinyobj::index_t idx2 = mesh.indices[indexOffset + 1];
                const tinyobj::index_t idx3 = mesh.indices[indexOffset + 2];

                Vector3 position1, position2, position3;
                Vector2 texCoord1, texCoord2, texCoord3;

                position1.x = attrib.vertices[3 * size_t(idx1.vertex_index) + 0];
                position1.y = attrib.vertices[3 * size_t(idx1.vertex_index) + 1];
                position1.z = attrib.vertices[3 * size_t(idx1.vertex_index) + 2];

                position2.x = attrib.vertices[3 * size_t(idx2.vertex_index) + 0];
                position2.y = attrib.vertices[3 * size_t(idx2.vertex_index) + 1];
                position2.z = attrib.vertices[3 * size_t(idx2.vertex_index) + 2];

                position3.x = attrib.vertices[3 * size_t(idx3.vertex_index) + 0];
                position3.y = attrib.vertices[3 * size_t(idx3.vertex_index) + 1];
                position3.z = attrib.vertices[3 * size_t(idx3.vertex_index) + 2];

                texCoord1.x = attrib.texcoords[2 * size_t(idx1.texcoord_index) + 0];
                texCoord1.y = attrib.texcoords[2 * size_t(idx1.texcoord_index) + 1];

                texCoord2.x = attrib.texcoords[2 * size_t(idx2.texcoord_index) + 0];
                texCoord2.y = attrib.texcoords[2 * size_t(idx2.texcoord_index) + 1];

                texCoord3.x = attrib.texcoords[2 * size_t(idx3.texcoord_index) + 0];
                texCoord3.y = attrib.texcoords[2 * size_t(idx3.texcoord_index) + 1];

                const auto tangentBitangent = ComputeTangentSpace(position1, position2, position3, texCoord1, texCoord2, texCoord3);

                tangentsBitangents[idx1.vertex_index].first += tangentBitangent.first;
                tangentsBitangents[idx1.vertex_index].second += tangentBitangent.second;
                tangentsBitangents[idx2.vertex_index].first += tangentBitangent.first;
                tangentsBitangents[idx2.vertex_index].second += tangentBitangent.second;
                tangentsBitangents[idx3.vertex_index].first += tangentBitangent.first;
                tangentsBitangents[idx3.vertex_index].second += tangentBitangent.second;

                indexOffset += static_cast<ModelData::Index>(faceIndex);
            }

            for (auto& [tangent, bitangent] : tangentsBitangents)
            {
                if (tangent != Vector3{ 0.0f, 0.0f, 0.0f })
                {
                    tangent = Normalize(tangent);
                }
                if (bitangent != Vector3{ 0.0f, 0.0f, 0.0f })
                {
                    bitangent = Normalize(bitangent);
                }
            }
            return tangentsBitangents;
        }

        std::vector<std::pair<Vector3, Vector3>> ComputeTangentsBitangents(ArrayView<ModelData::Index> indices, ArrayView<const Vector3> positions, ArrayView<const Vector2> texCoords)
        {
            std::vector<std::pair<Vector3, Vector3>> tangentsBitangents;
            tangentsBitangents.resize(positions.size(), { Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } });

            assert(indices.size() % 3 == 0);
            for (size_t i = 0; i < indices.size(); i += 3)
            {
                const auto& position1 = positions[indices[i + 0]];
                const auto& position2 = positions[indices[i + 1]];
                const auto& position3 = positions[indices[i + 2]];

                const auto& texCoord1 = texCoords[indices[i + 0]];
                const auto& texCoord2 = texCoords[indices[i + 1]];
                const auto& texCoord3 = texCoords[indices[i + 2]];

                const auto tangentBitangent = ComputeTangentSpace(position1, position2, position3, texCoord1, texCoord2, texCoord3);

                tangentsBitangents[indices[i + 0]].first += tangentBitangent.first;
                tangentsBitangents[indices[i + 0]].second += tangentBitangent.second;
                tangentsBitangents[indices[i + 1]].first += tangentBitangent.first;
                tangentsBitangents[indices[i + 1]].second += tangentBitangent.second;
                tangentsBitangents[indices[i + 2]].first += tangentBitangent.first;
                tangentsBitangents[indices[i + 2]].second += tangentBitangent.second;
            }

            for (auto& [tangent, bitangent] : tangentsBitangents)
            {
                if (tangent != Vector3{ 0.0f, 0.0f, 0.0f })
                {
                    tangent = Normalize(tangent);
                }
                if (bitangent != Vector3{ 0.0f, 0.0f, 0.0f })
                {
                    bitangent = Normalize(bitangent);
                }
            }
            return tangentsBitangents;
        }

        const std::byte* GetBufferData(const fastgltf::Buffer& buffer)
        {
            return std::visit(fastgltf::visitor {
                [](const fastgltf::sources::Array& source) -> const std::byte*
                {
                    return source.bytes.data();
                },
                [](const fastgltf::sources::ByteView& source) -> const std::byte*
                {
                    return source.bytes.data();
                },
                [](const auto&) -> const std::byte*
                {
                    return nullptr;
                },
            }, buffer.data);
        }

        ImageData LoadFastGltfImageSource(const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& basePath)
        {
            return std::visit(fastgltf::visitor {
                [&](const fastgltf::sources::URI& source) -> ImageData
                {
                    if (!source.uri.isLocalPath())
                    {
                        return {};
                    }
                    return ImageLoader::LoadImageFromFile((basePath / source.uri.fspath()).string());
                },
                [&](const fastgltf::sources::Array& source) -> ImageData
                {
                    return ImageLoader::LoadImageFromMemory(reinterpret_cast<const uint8_t*>(source.bytes.data()), source.bytes.size(), std::string(fastgltf::getMimeTypeString(source.mimeType)));
                },
                [&](const fastgltf::sources::ByteView& source) -> ImageData
                {
                    return ImageLoader::LoadImageFromMemory(reinterpret_cast<const uint8_t*>(source.bytes.data()), source.bytes.size(), std::string(fastgltf::getMimeTypeString(source.mimeType)));
                },
                [&](const fastgltf::sources::BufferView& source) -> ImageData
                {
                    if (source.bufferViewIndex >= asset.bufferViews.size())
                    {
                        return {};
                    }

                    const auto& view = asset.bufferViews[source.bufferViewIndex];
                    if (view.bufferIndex >= asset.buffers.size())
                    {
                        return {};
                    }

                    const std::byte* bufferData = GetBufferData(asset.buffers[view.bufferIndex]);
                    if (bufferData == nullptr)
                    {
                        return {};
                    }

                    return ImageLoader::LoadImageFromMemory(
                        reinterpret_cast<const uint8_t*>(bufferData + view.byteOffset),
                        view.byteLength,
                        std::string(fastgltf::getMimeTypeString(source.mimeType)));
                },
                [](const auto&) -> ImageData
                {
                    return {};
                },
            }, image.data);
        }

        std::optional<std::size_t> GetTextureImageIndex(const fastgltf::Texture& texture)
        {
            if (texture.ddsImageIndex.has_value())
            {
                return texture.ddsImageIndex;
            }
            if (texture.imageIndex.has_value())
            {
                return texture.imageIndex;
            }
            if (texture.webpImageIndex.has_value())
            {
                return texture.webpImageIndex;
            }
            if (texture.basisuImageIndex.has_value())
            {
                return texture.basisuImageIndex;
            }
            return std::nullopt;
        }

        template <typename OptionalTextureInfoType>
        ImageData LoadMaterialTexture(const fastgltf::Asset& asset, const OptionalTextureInfoType& textureInfo, const std::filesystem::path& basePath)
        {
            if (!textureInfo.has_value() || textureInfo->textureIndex >= asset.textures.size())
            {
                return {};
            }

            const auto imageIndex = GetTextureImageIndex(asset.textures[textureInfo->textureIndex]);
            if (!imageIndex.has_value() || *imageIndex >= asset.images.size())
            {
                return {};
            }

            return LoadFastGltfImageSource(asset, asset.images[*imageIndex], basePath);
        }

        bool IsGLTFModel(const std::string& filepath)
        {
            const auto extension = std::filesystem::path{ filepath }.extension();
            return extension == ".gltf" || extension == ".glb";
        }

        bool IsObjModel(const std::string& filepath)
        {
            return std::filesystem::path{ filepath }.extension() == ".obj";
        }
    }

    ModelData ModelLoader::LoadFromObj(const std::string& filepath)
    {
        ModelData result;

        tinyobj::ObjReaderConfig readerConfig;
        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(filepath, readerConfig))
        {
            return result;
        }

        const auto& attrib = reader.GetAttrib();
        const auto& shapes = reader.GetShapes();
        const auto& materials = reader.GetMaterials();

        result.Materials.reserve(materials.size());
        for (const auto& material : materials)
        {
            auto& resultMaterial = result.Materials.emplace_back();
            resultMaterial.Name = material.name;

            if (!material.diffuse_texname.empty())
            {
                resultMaterial.AlbedoTexture = ImageLoader::LoadImageFromFile(GetAbsolutePathToObjResource(filepath, material.diffuse_texname));
            }
            else
            {
                resultMaterial.AlbedoTexture = CreateStubTexture(255, 255, 255, 255);
            }

            if (!material.normal_texname.empty())
            {
                resultMaterial.NormalTexture = ImageLoader::LoadImageFromFile(GetAbsolutePathToObjResource(filepath, material.normal_texname));
            }
            else
            {
                resultMaterial.NormalTexture = CreateStubTexture(127, 127, 255, 255);
            }

            resultMaterial.MetallicRoughness = CreateStubTexture(0, 255, 0, 255);
        }

        result.Shapes.reserve(shapes.size());
        for (const auto& shape : shapes)
        {
            auto& resultShape = result.Shapes.emplace_back();
            resultShape.Name = shape.name;

            if (!shape.mesh.material_ids.empty())
            {
                resultShape.MaterialIndex = shape.mesh.material_ids.front();
            }

            std::vector<std::pair<Vector3, Vector3>> tangentsBitangents;
            if (!attrib.normals.empty() && !attrib.texcoords.empty())
            {
                tangentsBitangents = ComputeTangentsBitangents(shape.mesh, attrib);
            }

            resultShape.Vertices.reserve(shape.mesh.indices.size());
            resultShape.Indices.reserve(shape.mesh.indices.size());

            ModelData::Index indexOffset = 0;
            for (const size_t faceIndex : shape.mesh.num_face_vertices)
            {
                assert(faceIndex == 3);
                for (size_t v = 0; v < faceIndex; ++v, ++indexOffset)
                {
                    const tinyobj::index_t idx = shape.mesh.indices[indexOffset];
                    auto& vertex = resultShape.Vertices.emplace_back();
                    resultShape.Indices.push_back(indexOffset);

                    vertex.Position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                    vertex.Position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                    vertex.Position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                    vertex.TexCoord.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    vertex.TexCoord.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

                    vertex.Normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    vertex.Normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    vertex.Normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];

                    if (!tangentsBitangents.empty())
                    {
                        vertex.Tangent = tangentsBitangents[idx.vertex_index].first;
                        vertex.Bitangent = tangentsBitangents[idx.vertex_index].second;
                    }
                }
            }
        }

        return result;
    }

    ModelData ModelLoader::LoadFromGltf(const std::string& filepath)
    {
        ModelData result;

        const std::filesystem::path path{ filepath };
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None)
        {
            return result;
        }

        fastgltf::Parser parser(
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_mesh_quantization |
            fastgltf::Extensions::MSFT_texture_dds |
            fastgltf::Extensions::EXT_texture_webp);

        auto asset = parser.loadGltf(
            data.get(),
            path.parent_path(),
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::LoadExternalImages |
            fastgltf::Options::GenerateMeshIndices,
            fastgltf::Category::Buffers |
            fastgltf::Category::BufferViews |
            fastgltf::Category::Accessors |
            fastgltf::Category::Images |
            fastgltf::Category::Textures |
            fastgltf::Category::Materials |
            fastgltf::Category::Meshes);

        if (asset.error() != fastgltf::Error::None)
        {
            return result;
        }

        result.Materials.reserve(asset->materials.size());
        for (const auto& material : asset->materials)
        {
            auto& resultMaterial = result.Materials.emplace_back();
            resultMaterial.Name = material.name;
            resultMaterial.RoughnessScale = material.pbrData.roughnessFactor;
            resultMaterial.MetallicScale = material.pbrData.metallicFactor;

            resultMaterial.AlbedoTexture = LoadMaterialTexture(asset.get(), material.pbrData.baseColorTexture, path.parent_path());
            if (resultMaterial.AlbedoTexture.ByteData.empty())
            {
                resultMaterial.AlbedoTexture = CreateStubTexture(255, 255, 255, 255);
            }

            resultMaterial.NormalTexture = LoadMaterialTexture(asset.get(), material.normalTexture, path.parent_path());
            if (resultMaterial.NormalTexture.ByteData.empty())
            {
                resultMaterial.NormalTexture = CreateStubTexture(127, 127, 255, 255);
            }

            resultMaterial.MetallicRoughness = LoadMaterialTexture(asset.get(), material.pbrData.metallicRoughnessTexture, path.parent_path());
            if (resultMaterial.MetallicRoughness.ByteData.empty())
            {
                resultMaterial.MetallicRoughness = CreateStubTexture(0, 255, 0, 255);
            }
        }

        for (const auto& mesh : asset->meshes)
        {
            for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
            {
                const auto& primitive = mesh.primitives[primitiveIndex];
                if (primitive.type != fastgltf::PrimitiveType::Triangles)
                {
                    continue;
                }

                const auto positionAttribute = primitive.findAttribute("POSITION");
                if (positionAttribute == primitive.attributes.end())
                {
                    continue;
                }

                auto& shape = result.Shapes.emplace_back();
                shape.Name = mesh.name.empty()
                    ? "shape_" + std::to_string(result.Shapes.size())
                    : std::string(mesh.name) + "_" + std::to_string(primitiveIndex);
                if (primitive.materialIndex.has_value())
                {
                    shape.MaterialIndex = static_cast<uint32_t>(*primitive.materialIndex);
                }

                const auto& positionAccessor = asset->accessors[positionAttribute->accessorIndex];
                std::vector<fastgltf::math::fvec3> positions(positionAccessor.count);
                fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset.get(), positionAccessor, positions.data());

                std::vector<fastgltf::math::fvec2> texCoords;
                if (const auto texCoordAttribute = primitive.findAttribute("TEXCOORD_0"); texCoordAttribute != primitive.attributes.end())
                {
                    const auto& texCoordAccessor = asset->accessors[texCoordAttribute->accessorIndex];
                    texCoords.resize(texCoordAccessor.count);
                    fastgltf::copyFromAccessor<fastgltf::math::fvec2>(asset.get(), texCoordAccessor, texCoords.data());
                }

                std::vector<fastgltf::math::fvec3> normals;
                if (const auto normalAttribute = primitive.findAttribute("NORMAL"); normalAttribute != primitive.attributes.end())
                {
                    const auto& normalAccessor = asset->accessors[normalAttribute->accessorIndex];
                    normals.resize(normalAccessor.count);
                    fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset.get(), normalAccessor, normals.data());
                }

                if (primitive.indicesAccessor.has_value())
                {
                    const auto& indexAccessor = asset->accessors[*primitive.indicesAccessor];
                    shape.Indices.resize(indexAccessor.count);
                    fastgltf::copyFromAccessor<uint32_t>(asset.get(), indexAccessor, shape.Indices.data());
                }
                else
                {
                    shape.Indices.resize(positions.size());
                    for (size_t i = 0; i < shape.Indices.size(); ++i)
                    {
                        shape.Indices[i] = static_cast<ModelData::Index>(i);
                    }
                }

                shape.Vertices.resize(positions.size());
                std::vector<Vector3> vertexPositions(shape.Vertices.size());
                std::vector<Vector2> vertexTexCoords(shape.Vertices.size());
                const bool hasTexCoords = texCoords.size() == shape.Vertices.size();
                const bool hasNormals = normals.size() == shape.Vertices.size();

                for (size_t i = 0; i < shape.Vertices.size(); ++i)
                {
                    auto& vertex = shape.Vertices[i];
                    vertex.Position = ToVector3(positions[i]);
                    vertexPositions[i] = vertex.Position;

                    if (hasTexCoords)
                    {
                        vertex.TexCoord = ToVector2(texCoords[i]);
                        vertexTexCoords[i] = vertex.TexCoord;
                    }

                    if (hasNormals)
                    {
                        vertex.Normal = ToVector3(normals[i]);
                    }
                }

                if (hasTexCoords && !shape.Indices.empty())
                {
                    const auto tangentsBitangents = ComputeTangentsBitangents(shape.Indices, vertexPositions, vertexTexCoords);
                    for (size_t i = 0; i < shape.Vertices.size() && i < tangentsBitangents.size(); ++i)
                    {
                        shape.Vertices[i].Tangent = tangentsBitangents[i].first;
                        shape.Vertices[i].Bitangent = tangentsBitangents[i].second;
                    }
                }
            }
        }

        return result;
    }

    ModelData ModelLoader::Load(const std::string& filepath)
    {
        if (IsGLTFModel(filepath))
        {
            return ModelLoader::LoadFromGltf(filepath);
        }
        if (IsObjModel(filepath))
        {
            return ModelLoader::LoadFromObj(filepath);
        }
        assert(false);
        return {};
    }
}
