#include "Renderer/RenderFlow/DefaultScene/BindingSchema.h"
#include "Renderer/RenderFlow/DefaultScene/GpuTypes.h"

#include <algorithm>
#include <array>
#include <Device.h>

namespace luna::render_flow::default_scene {
namespace {

const RHI::ShaderStage kFragmentStage = RHI::ShaderStage::Fragment;
const RHI::ShaderStage kVertexFragmentStages = RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment;

const std::array<DescriptorBindingSchema, 11> kMaterialBindings{{
    {"BaseColorTexture",
     material_binding::BaseColorTexture,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gBaseColorTexture"},
    {"BaseColorSampler",
     material_binding::BaseColorSampler,
     RHI::DescriptorType::Sampler,
     1,
     kFragmentStage,
     "gBaseColorSampler"},
    {"NormalTexture",
     material_binding::NormalTexture,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gNormalTexture"},
    {"NormalSampler",
     material_binding::NormalSampler,
     RHI::DescriptorType::Sampler,
     1,
     kFragmentStage,
     "gNormalSampler"},
    {"MetallicRoughnessTexture",
     material_binding::MetallicRoughnessTexture,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gMetallicRoughnessTexture"},
    {"MetallicRoughnessSampler",
     material_binding::MetallicRoughnessSampler,
     RHI::DescriptorType::Sampler,
     1,
     kFragmentStage,
     "gMetallicRoughnessSampler"},
    {"EmissiveTexture",
     material_binding::EmissiveTexture,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gEmissiveTexture"},
    {"EmissiveSampler",
     material_binding::EmissiveSampler,
     RHI::DescriptorType::Sampler,
     1,
     kFragmentStage,
     "gEmissiveSampler"},
    {"OcclusionTexture",
     material_binding::OcclusionTexture,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gOcclusionTexture"},
    {"OcclusionSampler",
     material_binding::OcclusionSampler,
     RHI::DescriptorType::Sampler,
     1,
     kFragmentStage,
     "gOcclusionSampler"},
    {"MaterialParams",
     material_binding::MaterialParams,
     RHI::DescriptorType::UniformBuffer,
     1,
     kFragmentStage,
     "gMaterialParams"},
}};

const std::array<DescriptorBindingSchema, 11> kGBufferBindings{{
    {"GBufferBaseColor",
     gbuffer_binding::BaseColor,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gGBufferBaseColor"},
    {"GBufferNormalMetallic",
     gbuffer_binding::NormalMetallic,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gGBufferNormalMetallic"},
    {"GBufferWorldPositionRoughness",
     gbuffer_binding::WorldPositionRoughness,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gGBufferWorldPositionRoughness"},
    {"GBufferEmissiveAo",
     gbuffer_binding::EmissiveAo,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gGBufferEmissiveAo"},
    {"GBufferSampler",
     gbuffer_binding::Sampler,
     RHI::DescriptorType::Sampler,
     1,
     kFragmentStage,
     "gGBufferSampler"},
    {"PickBuffer", gbuffer_binding::Pick, RHI::DescriptorType::SampledImage, 1, kFragmentStage, "gPickBuffer"},
    {"AmbientOcclusion",
     gbuffer_binding::AmbientOcclusion,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gAmbientOcclusionTexture"},
    {"Reflection",
     gbuffer_binding::Reflection,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gReflectionTexture"},
    {"IndirectDiffuse",
     gbuffer_binding::IndirectDiffuse,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gIndirectDiffuseTexture"},
    {"IndirectSpecular",
     gbuffer_binding::IndirectSpecular,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gIndirectSpecularTexture"},
    {"Velocity",
     gbuffer_binding::Velocity,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gVelocityTexture"},
}};

const std::array<DescriptorBindingSchema, 7> kSceneBindings{{
    {"SceneParams",
     scene_binding::SceneParams,
     RHI::DescriptorType::UniformBuffer,
     1,
     kVertexFragmentStages,
     "gSceneParams"},
    {"EnvironmentTexture",
     scene_binding::EnvironmentTexture,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gEnvironmentTexture"},
    {"EnvironmentSampler",
     scene_binding::EnvironmentSampler,
     RHI::DescriptorType::Sampler,
     1,
     kFragmentStage,
     "gEnvironmentSampler"},
    {"ShadowMap", scene_binding::ShadowMap, RHI::DescriptorType::SampledImage, 1, kFragmentStage, "gShadowMap"},
    {"ShadowSampler",
     scene_binding::ShadowSampler,
     RHI::DescriptorType::Sampler,
     1,
     kFragmentStage,
     "gShadowSampler"},
    {"EnvironmentPrefilterTexture",
     scene_binding::EnvironmentPrefilterTexture,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gEnvironmentPrefilterTexture"},
    {"EnvironmentBrdfLut",
     scene_binding::EnvironmentBrdfLut,
     RHI::DescriptorType::SampledImage,
     1,
     kFragmentStage,
     "gEnvironmentBrdfLut"},
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
     RHI::ShaderStage::Vertex,
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

RHI::DescriptorSetLayoutCreateInfo makeDescriptorSetLayoutCreateInfo(const DescriptorSetSchema& schema)
{
    RHI::DescriptorSetLayoutCreateInfo create_info;
    create_info.Bindings.reserve(schema.bindings.size());
    for (const DescriptorBindingSchema& binding : schema.bindings) {
        create_info.Bindings.push_back(RHI::DescriptorSetLayoutBinding{
            .Binding = binding.binding,
            .Type = binding.type,
            .Count = binding.count,
            .StageFlags = binding.stages,
        });
    }
    return create_info;
}

namespace {

const RHI::Ref<RHI::DescriptorSetLayout>&
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

RHI::PipelineLayoutCreateInfo makePipelineLayoutCreateInfo(const PipelineLayoutSchema& schema,
                                                                 const DescriptorSetLayoutRefs& layouts)
{
    RHI::PipelineLayoutCreateInfo create_info;
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
        create_info.PushConstantRanges.push_back(RHI::PushConstantRange{
            .StageFlags = push_constant.stages,
            .Offset = push_constant.offset,
            .Size = push_constant.size,
        });
    }

    return create_info;
}

luna::render_flow::ShaderBindingContract
    makePipelineShaderBindingContract(const PipelineLayoutSchema& schema,
                                      luna::render_flow::ShaderBindingAddressMode address_mode)
{
    luna::render_flow::ShaderBindingContract contract =
        luna::render_flow::makeShaderBindingContract(schema.name ? schema.name : "");

    uint32_t flattened_register_base = 0;
    for (const PipelineLayoutSetSchema& set : schema.sets) {
        const DescriptorSetSchema& set_schema = descriptorSetSchema(set.schema_id);
        contract.bindings.reserve(contract.bindings.size() + set_schema.bindings.size());

        uint32_t max_binding = 0;
        bool has_bindings = false;
        for (const DescriptorBindingSchema& binding : set_schema.bindings) {
            uint32_t contract_set = set.set_index;
            uint32_t contract_binding = binding.binding;
            if (address_mode == luna::render_flow::ShaderBindingAddressMode::FlattenedRegisterSpace) {
                contract_set = 0;
                contract_binding = flattened_register_base + binding.binding;
            }

            contract.bindings.push_back(luna::render_flow::ShaderBindingRequirement{
                .name = binding.name ? binding.name : "",
                .shader_name = binding.shader_name ? binding.shader_name : "",
                .set_name = set_schema.name ? set_schema.name : "",
                .logical_set = set.set_index,
                .logical_binding = binding.binding,
                .set = contract_set,
                .binding = contract_binding,
                .type = binding.type,
                .count = binding.count,
                .stages = binding.stages,
            });

            max_binding = std::max(max_binding, binding.binding + binding.count - 1u);
            has_bindings = true;
        }

        if (address_mode == luna::render_flow::ShaderBindingAddressMode::FlattenedRegisterSpace && has_bindings) {
            flattened_register_base += max_binding + 1u;
        }
    }

    contract.push_constants.reserve(schema.push_constants.size());
    for (const PushConstantSchema& push_constant : schema.push_constants) {
        contract.push_constants.push_back(luna::render_flow::ShaderPushConstantRequirement{
            .name = push_constant.name ? push_constant.name : "",
            .offset = push_constant.offset,
            .size = push_constant.size,
            .stages = push_constant.stages,
        });
    }

    return contract;
}

RHI::Ref<RHI::DescriptorSetLayout>
    createDescriptorSetLayoutFromSchema(const RHI::Ref<RHI::Device>& device,
                                        const DescriptorSetSchema& schema)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(makeDescriptorSetLayoutCreateInfo(schema));
}

RHI::Ref<RHI::PipelineLayout>
    createPipelineLayoutFromSchema(const RHI::Ref<RHI::Device>& device,
                                   const PipelineLayoutSchema& schema,
                                   const DescriptorSetLayoutRefs& layouts)
{
    if (!device) {
        return {};
    }

    return device->CreatePipelineLayout(makePipelineLayoutCreateInfo(schema, layouts));
}

} // namespace luna::render_flow::default_scene
