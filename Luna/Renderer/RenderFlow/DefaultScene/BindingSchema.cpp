#include "Renderer/RenderFlow/DefaultScene/BindingSchema.h"
#include "Renderer/RenderFlow/DefaultScene/GpuTypes.h"

#include <algorithm>
#include <array>
#include <Device.h>

namespace luna::render_flow::default_scene {
namespace {

const luna::RHI::ShaderStage kFragmentStage = luna::RHI::ShaderStage::Fragment;
const luna::RHI::ShaderStage kVertexFragmentStages = luna::RHI::ShaderStage::Vertex | luna::RHI::ShaderStage::Fragment;

const std::array<DescriptorBindingSchema, 11> kMaterialBindings{{
    {"BaseColorTexture",
     material_binding::BaseColorTexture,
     luna::RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage},
    {"BaseColorSampler", material_binding::BaseColorSampler, luna::RHI::DescriptorType::Sampler, 1, kFragmentStage},
    {"NormalTexture", material_binding::NormalTexture, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"NormalSampler", material_binding::NormalSampler, luna::RHI::DescriptorType::Sampler, 1, kFragmentStage},
    {"MetallicRoughnessTexture",
     material_binding::MetallicRoughnessTexture,
     luna::RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage},
    {"MetallicRoughnessSampler",
     material_binding::MetallicRoughnessSampler,
     luna::RHI::DescriptorType::Sampler,
     1,
     kFragmentStage},
    {"EmissiveTexture", material_binding::EmissiveTexture, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"EmissiveSampler", material_binding::EmissiveSampler, luna::RHI::DescriptorType::Sampler, 1, kFragmentStage},
    {"OcclusionTexture",
     material_binding::OcclusionTexture,
     luna::RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage},
    {"OcclusionSampler", material_binding::OcclusionSampler, luna::RHI::DescriptorType::Sampler, 1, kFragmentStage},
    {"MaterialParams", material_binding::MaterialParams, luna::RHI::DescriptorType::UniformBuffer, 1, kFragmentStage},
}};

const std::array<DescriptorBindingSchema, 11> kGBufferBindings{{
    {"GBufferBaseColor", gbuffer_binding::BaseColor, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"GBufferNormalMetallic",
     gbuffer_binding::NormalMetallic,
     luna::RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage},
    {"GBufferWorldPositionRoughness",
     gbuffer_binding::WorldPositionRoughness,
     luna::RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage},
    {"GBufferEmissiveAo", gbuffer_binding::EmissiveAo, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"GBufferSampler", gbuffer_binding::Sampler, luna::RHI::DescriptorType::Sampler, 1, kFragmentStage},
    {"PickBuffer", gbuffer_binding::Pick, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"AmbientOcclusion", gbuffer_binding::AmbientOcclusion, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"Reflection", gbuffer_binding::Reflection, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"IndirectDiffuse", gbuffer_binding::IndirectDiffuse, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"IndirectSpecular", gbuffer_binding::IndirectSpecular, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"Velocity", gbuffer_binding::Velocity, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
}};

const std::array<DescriptorBindingSchema, 7> kSceneBindings{{
    {"SceneParams", scene_binding::SceneParams, luna::RHI::DescriptorType::UniformBuffer, 1, kVertexFragmentStages},
    {"EnvironmentTexture",
     scene_binding::EnvironmentTexture,
     luna::RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage},
    {"EnvironmentSampler", scene_binding::EnvironmentSampler, luna::RHI::DescriptorType::Sampler, 1, kFragmentStage},
    {"ShadowMap", scene_binding::ShadowMap, luna::RHI::DescriptorType::SampledImage, 1, kFragmentStage},
    {"ShadowSampler", scene_binding::ShadowSampler, luna::RHI::DescriptorType::Sampler, 1, kFragmentStage},
    {"EnvironmentPrefilterTexture",
     scene_binding::EnvironmentPrefilterTexture,
     luna::RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage},
    {"EnvironmentBrdfLut",
     scene_binding::EnvironmentBrdfLut,
     luna::RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage},
}};

const DescriptorSetSchema kMaterialSchema{
    "Material",
    std::span<const DescriptorBindingSchema>(kMaterialBindings.data(), kMaterialBindings.size()),
};

const DescriptorSetSchema kGBufferSchema{
    "GBuffer",
    std::span<const DescriptorBindingSchema>(kGBufferBindings.data(), kGBufferBindings.size()),
};

const DescriptorSetSchema kSceneSchema{
    "Scene",
    std::span<const DescriptorBindingSchema>(kSceneBindings.data(), kSceneBindings.size()),
};

const std::array<PipelineLayoutSetSchema, 2> kGeometrySets{{
    {DescriptorSetSchemaId::Material, 0},
    {DescriptorSetSchemaId::Scene, 1},
}};

const std::array<PipelineLayoutSetSchema, 1> kShadowSets{{
    {DescriptorSetSchemaId::Scene, 0},
}};

const std::array<PipelineLayoutSetSchema, 2> kLightingSets{{
    {DescriptorSetSchemaId::GBuffer, 0},
    {DescriptorSetSchemaId::Scene, 1},
}};

const std::array<PushConstantSchema, 1> kMeshPushConstants{{
    {"MeshPushConstants",
     luna::RHI::ShaderStage::Vertex,
     0,
     sizeof(render_flow::default_scene_detail::MeshPushConstants)},
}};

const PipelineLayoutSchema kGeometryPipelineLayout{
    "Geometry",
    std::span<const PipelineLayoutSetSchema>(kGeometrySets.data(), kGeometrySets.size()),
    std::span<const PushConstantSchema>(kMeshPushConstants.data(), kMeshPushConstants.size()),
};

const PipelineLayoutSchema kShadowPipelineLayout{
    "Shadow",
    std::span<const PipelineLayoutSetSchema>(kShadowSets.data(), kShadowSets.size()),
    std::span<const PushConstantSchema>(kMeshPushConstants.data(), kMeshPushConstants.size()),
};

const PipelineLayoutSchema kLightingPipelineLayout{
    "Lighting",
    std::span<const PipelineLayoutSetSchema>(kLightingSets.data(), kLightingSets.size()),
    {},
};

const PipelineLayoutSchema kTransparentPipelineLayout{
    "Transparent",
    std::span<const PipelineLayoutSetSchema>(kGeometrySets.data(), kGeometrySets.size()),
    std::span<const PushConstantSchema>(kMeshPushConstants.data(), kMeshPushConstants.size()),
};

} // namespace

const DescriptorSetSchema& descriptorSetSchema(DescriptorSetSchemaId schema_id) noexcept
{
    switch (schema_id) {
        case DescriptorSetSchemaId::Material:
            return kMaterialSchema;
        case DescriptorSetSchemaId::GBuffer:
            return kGBufferSchema;
        case DescriptorSetSchemaId::Scene:
            return kSceneSchema;
    }
    return kMaterialSchema;
}

const DescriptorSetSchema& materialDescriptorSetSchema() noexcept
{
    return kMaterialSchema;
}

const DescriptorSetSchema& gbufferDescriptorSetSchema() noexcept
{
    return kGBufferSchema;
}

const DescriptorSetSchema& sceneDescriptorSetSchema() noexcept
{
    return kSceneSchema;
}

const PipelineLayoutSchema& geometryPipelineLayoutSchema() noexcept
{
    return kGeometryPipelineLayout;
}

const PipelineLayoutSchema& shadowPipelineLayoutSchema() noexcept
{
    return kShadowPipelineLayout;
}

const PipelineLayoutSchema& lightingPipelineLayoutSchema() noexcept
{
    return kLightingPipelineLayout;
}

const PipelineLayoutSchema& transparentPipelineLayoutSchema() noexcept
{
    return kTransparentPipelineLayout;
}

luna::RHI::DescriptorSetLayoutCreateInfo makeDescriptorSetLayoutCreateInfo(const DescriptorSetSchema& schema)
{
    luna::RHI::DescriptorSetLayoutCreateInfo create_info;
    create_info.Bindings.reserve(schema.bindings.size());
    for (const DescriptorBindingSchema& binding : schema.bindings) {
        create_info.Bindings.push_back(luna::RHI::DescriptorSetLayoutBinding{
            .Binding = binding.binding,
            .Type = binding.type,
            .Count = binding.count,
            .StageFlags = binding.stages,
        });
    }
    return create_info;
}

namespace {

const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>&
    descriptorSetLayoutForSchema(DescriptorSetSchemaId schema_id, const DescriptorSetLayoutRefs& layouts) noexcept
{
    switch (schema_id) {
        case DescriptorSetSchemaId::Material:
            return layouts.material;
        case DescriptorSetSchemaId::GBuffer:
            return layouts.gbuffer;
        case DescriptorSetSchemaId::Scene:
            return layouts.scene;
    }
    return layouts.material;
}

} // namespace

luna::RHI::PipelineLayoutCreateInfo makePipelineLayoutCreateInfo(const PipelineLayoutSchema& schema,
                                                                 const DescriptorSetLayoutRefs& layouts)
{
    luna::RHI::PipelineLayoutCreateInfo create_info;
    uint32_t set_count = 0;
    for (const PipelineLayoutSetSchema& set : schema.sets) {
        set_count = std::max(set_count, set.set_index + 1u);
    }

    create_info.SetLayouts.resize(set_count);
    for (const PipelineLayoutSetSchema& set : schema.sets) {
        if (set.set_index < create_info.SetLayouts.size()) {
            create_info.SetLayouts[set.set_index] = descriptorSetLayoutForSchema(set.schema_id, layouts);
        }
    }

    create_info.PushConstantRanges.reserve(schema.push_constants.size());
    for (const PushConstantSchema& push_constant : schema.push_constants) {
        create_info.PushConstantRanges.push_back(luna::RHI::PushConstantRange{
            .StageFlags = push_constant.stages,
            .Offset = push_constant.offset,
            .Size = push_constant.size,
        });
    }

    return create_info;
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout>
    createDescriptorSetLayoutFromSchema(const luna::RHI::Ref<luna::RHI::Device>& device,
                                        const DescriptorSetSchema& schema)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(makeDescriptorSetLayoutCreateInfo(schema));
}

luna::RHI::Ref<luna::RHI::PipelineLayout>
    createPipelineLayoutFromSchema(const luna::RHI::Ref<luna::RHI::Device>& device,
                                   const PipelineLayoutSchema& schema,
                                   const DescriptorSetLayoutRefs& layouts)
{
    if (!device) {
        return {};
    }

    return device->CreatePipelineLayout(makePipelineLayoutCreateInfo(schema, layouts));
}

} // namespace luna::render_flow::default_scene
