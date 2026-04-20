#include "Asset/Editor/MaterialFactory.h"

#include "Asset/AssetDatabase.h"
#include "Asset/Editor/MaterialImporter.h"
#include "Core/Log.h"

#include "yaml-cpp/yaml.h"

#include <fstream>

namespace luna::material_factory_detail {

const char* blendModeToString(Material::BlendMode blend_mode)
{
    switch (blend_mode) {
        case Material::BlendMode::Masked:
            return "Masked";
        case Material::BlendMode::Transparent:
            return "Transparent";
        case Material::BlendMode::Additive:
            return "Additive";
        case Material::BlendMode::Opaque:
        default:
            return "Opaque";
    }
}

void emitVec3(YAML::Emitter& out, const glm::vec3& value)
{
    out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
}

void emitVec4(YAML::Emitter& out, const glm::vec4& value)
{
    out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
}

void emitHandle(YAML::Emitter& out, AssetHandle handle)
{
    out << static_cast<uint64_t>(handle);
}

void emitTextureHandles(YAML::Emitter& out, const MaterialTextureHandleSet& textures)
{
    out << YAML::Key << "Textures" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "BaseColor" << YAML::Value;
    emitHandle(out, textures.BaseColor);
    out << YAML::Key << "Normal" << YAML::Value;
    emitHandle(out, textures.Normal);
    out << YAML::Key << "MetallicRoughness" << YAML::Value;
    emitHandle(out, textures.MetallicRoughness);
    out << YAML::Key << "Emissive" << YAML::Value;
    emitHandle(out, textures.Emissive);
    out << YAML::Key << "Occlusion" << YAML::Value;
    emitHandle(out, textures.Occlusion);
    out << YAML::EndMap;
}

void emitSurface(YAML::Emitter& out, const Material::SurfaceProperties& surface)
{
    out << YAML::Key << "Surface" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "BaseColorFactor" << YAML::Value;
    emitVec4(out, surface.BaseColorFactor);
    out << YAML::Key << "EmissiveFactor" << YAML::Value;
    emitVec3(out, surface.EmissiveFactor);
    out << YAML::Key << "MetallicFactor" << YAML::Value << surface.MetallicFactor;
    out << YAML::Key << "RoughnessFactor" << YAML::Value << surface.RoughnessFactor;
    out << YAML::Key << "NormalScale" << YAML::Value << surface.NormalScale;
    out << YAML::Key << "OcclusionStrength" << YAML::Value << surface.OcclusionStrength;
    out << YAML::Key << "AlphaCutoff" << YAML::Value << surface.AlphaCutoff;
    out << YAML::Key << "DoubleSided" << YAML::Value << surface.DoubleSided;
    out << YAML::Key << "Unlit" << YAML::Value << surface.Unlit;
    out << YAML::Key << "BlendMode" << YAML::Value << blendModeToString(surface.BlendModeValue);
    out << YAML::EndMap;
}

bool writeMaterialFile(const std::filesystem::path& path, const MaterialAssetDescriptor& descriptor)
{
    if (path.empty()) {
        return false;
    }

    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Name" << YAML::Value
        << (descriptor.Name.empty() ? path.stem().string() : descriptor.Name);
    emitTextureHandles(out, descriptor.Textures);
    emitSurface(out, descriptor.Surface);
    out << YAML::EndMap;
    out << YAML::EndMap;

    std::ofstream stream(path);
    if (!stream.is_open()) {
        return false;
    }

    stream << out.c_str();
    return stream.good();
}

void createMaterialMetadata(const std::filesystem::path& path, const MaterialAssetDescriptor& descriptor)
{
    MaterialImporter importer;
    AssetMetadata metadata = importer.import(path);
    metadata.Name = descriptor.Name.empty() ? path.stem().string() : descriptor.Name;
    importer.serializeMetadata(metadata);
    AssetDatabase::set(metadata.Handle, metadata);
}

} // namespace luna::material_factory_detail

namespace luna {

MaterialAssetDescriptor MaterialFactory::makeDefaultDescriptor(std::string name)
{
    MaterialAssetDescriptor descriptor;
    descriptor.Name = std::move(name);
    descriptor.Surface.BaseColorFactor = glm::vec4(1.0f);
    descriptor.Surface.EmissiveFactor = glm::vec3(0.0f);
    descriptor.Surface.MetallicFactor = 0.0f;
    descriptor.Surface.RoughnessFactor = 1.0f;
    descriptor.Surface.NormalScale = 1.0f;
    descriptor.Surface.OcclusionStrength = 1.0f;
    descriptor.Surface.BlendModeValue = Material::BlendMode::Opaque;
    descriptor.Surface.AlphaCutoff = 0.5f;
    descriptor.Surface.DoubleSided = false;
    descriptor.Surface.Unlit = false;
    return descriptor;
}

bool MaterialFactory::createMaterialFile(const std::filesystem::path& path,
                                         const MaterialAssetDescriptor& descriptor,
                                         bool create_metadata)
{
    const std::filesystem::path output_path =
        path.extension() == ".lunamat" ? path : path.parent_path() / (path.stem().string() + ".lunamat");

    if (!material_factory_detail::writeMaterialFile(output_path, descriptor)) {
        LUNA_CORE_ERROR("Failed to create material file '{}'", output_path.string());
        return false;
    }

    if (create_metadata) {
        material_factory_detail::createMaterialMetadata(output_path, descriptor);
    }

    return true;
}

bool MaterialFactory::createDefaultMaterialFile(const std::filesystem::path& path,
                                                std::string name,
                                                bool create_metadata)
{
    return createMaterialFile(path, makeDefaultDescriptor(std::move(name)), create_metadata);
}

} // namespace luna
