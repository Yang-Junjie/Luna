#pragma once

// Descriptor and pipeline layout schema for DefaultScene pipelines.
// This is the single source of truth for descriptor set layout creation, pipeline set layout
// composition, push constants, and write binding slots.

#include "Renderer/RenderFlow/ShaderBindingContract.h"

#include <cstdint>

#include <DescriptorSetLayout.h>
#include <PipelineLayout.h>
#include <span>

namespace luna::RHI {
class Device;
class PipelineLayout;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

namespace material_binding {
inline constexpr uint32_t BaseColorTexture = 0;
inline constexpr uint32_t BaseColorSampler = 1;
inline constexpr uint32_t NormalTexture = 2;
inline constexpr uint32_t NormalSampler = 3;
inline constexpr uint32_t MetallicRoughnessTexture = 4;
inline constexpr uint32_t MetallicRoughnessSampler = 5;
inline constexpr uint32_t EmissiveTexture = 6;
inline constexpr uint32_t EmissiveSampler = 7;
inline constexpr uint32_t OcclusionTexture = 8;
inline constexpr uint32_t OcclusionSampler = 9;
inline constexpr uint32_t MaterialParams = 10;
} // namespace material_binding

namespace gbuffer_binding {
inline constexpr uint32_t BaseColor = 0;
inline constexpr uint32_t NormalMetallic = 1;
inline constexpr uint32_t WorldPositionRoughness = 2;
inline constexpr uint32_t EmissiveAo = 3;
inline constexpr uint32_t Sampler = 4;
inline constexpr uint32_t Pick = 5;
inline constexpr uint32_t AmbientOcclusion = 6;
inline constexpr uint32_t Reflection = 7;
inline constexpr uint32_t IndirectDiffuse = 8;
inline constexpr uint32_t IndirectSpecular = 9;
inline constexpr uint32_t Velocity = 10;
} // namespace gbuffer_binding

namespace scene_binding {
inline constexpr uint32_t SceneParams = 0;
inline constexpr uint32_t EnvironmentTexture = 1;
inline constexpr uint32_t EnvironmentSampler = 2;
inline constexpr uint32_t ShadowMap = 3;
inline constexpr uint32_t ShadowSampler = 4;
inline constexpr uint32_t EnvironmentPrefilterTexture = 5;
inline constexpr uint32_t EnvironmentBrdfLut = 6;
} // namespace scene_binding

struct DescriptorBindingSchema {
    const char* name{nullptr};
    uint32_t binding{0};
    luna::RHI::DescriptorType type{luna::RHI::DescriptorType::UniformBuffer};
    uint32_t count{1};
    luna::RHI::ShaderStage stages{luna::RHI::ShaderStage::AllGraphics};
    const char* shader_name{nullptr};
};

struct DescriptorSetSchema {
    const char* name{nullptr};
    std::span<const DescriptorBindingSchema> bindings;
};

enum class DescriptorSetSchemaId : uint8_t {
    Material,
    GBuffer,
    Scene,
};

struct PushConstantSchema {
    const char* name{nullptr};
    luna::RHI::ShaderStage stages{luna::RHI::ShaderStage::None};
    uint32_t offset{0};
    uint32_t size{0};
};

struct PipelineLayoutSetSchema {
    DescriptorSetSchemaId schema_id{DescriptorSetSchemaId::Material};
    uint32_t set_index{0};
};

struct DescriptorSetLayoutRefs {
    luna::RHI::Ref<luna::RHI::DescriptorSetLayout> material;
    luna::RHI::Ref<luna::RHI::DescriptorSetLayout> gbuffer;
    luna::RHI::Ref<luna::RHI::DescriptorSetLayout> scene;
};

struct PipelineLayoutSchema {
    const char* name{nullptr};
    std::span<const PipelineLayoutSetSchema> sets;
    std::span<const PushConstantSchema> push_constants;
};

[[nodiscard]] const DescriptorSetSchema& descriptorSetSchema(DescriptorSetSchemaId schema_id) noexcept;
[[nodiscard]] const DescriptorSetSchema& materialDescriptorSetSchema() noexcept;
[[nodiscard]] const DescriptorSetSchema& gbufferDescriptorSetSchema() noexcept;
[[nodiscard]] const DescriptorSetSchema& sceneDescriptorSetSchema() noexcept;

[[nodiscard]] const PipelineLayoutSchema& geometryPipelineLayoutSchema() noexcept;
[[nodiscard]] const PipelineLayoutSchema& shadowPipelineLayoutSchema() noexcept;
[[nodiscard]] const PipelineLayoutSchema& lightingPipelineLayoutSchema() noexcept;
[[nodiscard]] const PipelineLayoutSchema& transparentPipelineLayoutSchema() noexcept;

[[nodiscard]] luna::RHI::DescriptorSetLayoutCreateInfo
    makeDescriptorSetLayoutCreateInfo(const DescriptorSetSchema& schema);

[[nodiscard]] luna::RHI::PipelineLayoutCreateInfo makePipelineLayoutCreateInfo(const PipelineLayoutSchema& schema,
                                                                               const DescriptorSetLayoutRefs& layouts);

[[nodiscard]] luna::render_flow::ShaderBindingContract
    makePipelineShaderBindingContract(const PipelineLayoutSchema& schema,
                                      luna::render_flow::ShaderBindingAddressMode address_mode);

[[nodiscard]] luna::RHI::Ref<luna::RHI::DescriptorSetLayout>
    createDescriptorSetLayoutFromSchema(const luna::RHI::Ref<luna::RHI::Device>& device,
                                        const DescriptorSetSchema& schema);

[[nodiscard]] luna::RHI::Ref<luna::RHI::PipelineLayout>
    createPipelineLayoutFromSchema(const luna::RHI::Ref<luna::RHI::Device>& device,
                                   const PipelineLayoutSchema& schema,
                                   const DescriptorSetLayoutRefs& layouts);

} // namespace luna::render_flow::default_scene
