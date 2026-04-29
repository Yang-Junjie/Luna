#pragma once

#include "Renderer/RenderFlow/RenderFlowTypes.h"

#include <cstdint>
#include <glm/vec4.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace luna {
class RenderWorld;
}

namespace luna::render_flow {

class RenderFlowBuilder;
class RenderPassBlackboard;

enum class RenderFeatureGraphResourceKind : uint8_t {
    Texture,
    Buffer,
};

enum class RenderFeatureGraphResourceFlags : uint32_t {
    None = 0,
    Optional = 1 << 0,
    External = 1 << 1,
};

inline RenderFeatureGraphResourceFlags operator|(RenderFeatureGraphResourceFlags lhs,
                                                 RenderFeatureGraphResourceFlags rhs) noexcept
{
    return static_cast<RenderFeatureGraphResourceFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RenderFeatureGraphResourceFlags& operator|=(RenderFeatureGraphResourceFlags& lhs,
                                                   RenderFeatureGraphResourceFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool operator&(RenderFeatureGraphResourceFlags lhs, RenderFeatureGraphResourceFlags rhs) noexcept
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

struct RenderFeatureGraphResource {
    std::string_view name;
    RenderFeatureGraphResourceKind kind{RenderFeatureGraphResourceKind::Texture};
    RenderFeatureGraphResourceFlags flags{RenderFeatureGraphResourceFlags::None};
};

enum class RenderPassResourceAccess : uint8_t {
    Read,
    Write,
    ReadWrite,
};

struct RenderPassResourceUsage {
    std::string_view name;
    RenderFeatureGraphResourceKind kind{RenderFeatureGraphResourceKind::Texture};
    RenderPassResourceAccess access{RenderPassResourceAccess::Read};
    RenderFeatureGraphResourceFlags flags{RenderFeatureGraphResourceFlags::None};
};

inline bool readsResource(RenderPassResourceAccess access) noexcept
{
    return access == RenderPassResourceAccess::Read || access == RenderPassResourceAccess::ReadWrite;
}

inline bool writesResource(RenderPassResourceAccess access) noexcept
{
    return access == RenderPassResourceAccess::Write || access == RenderPassResourceAccess::ReadWrite;
}

struct RenderFeaturePassInfo {
    std::string name;
    std::vector<RenderPassResourceUsage> resources;
};

struct RenderFeatureStatusEntry {
    std::string name;
    bool ready{false};
};

struct RenderFeatureDiagnostics {
    bool binding_contract_valid{true};
    std::string binding_contract_summary;
    bool pipeline_resources_valid{true};
    std::string pipeline_resources_summary;
    std::vector<RenderFeatureStatusEntry> pipeline_resources;
    bool persistent_resources_valid{true};
    std::string persistent_resources_summary;
    std::vector<RenderFeatureStatusEntry> persistent_resources;
    bool history_resources_valid{true};
    std::string history_resources_summary;
    std::vector<RenderFeatureStatusEntry> history_resources;
};

struct RenderFeatureInfo {
    std::string_view name;
    std::string_view display_name;
    std::string_view category;
    bool enabled{true};
    bool runtime_toggleable{false};
    bool supported{true};
    bool active{true};
    std::string support_summary;
    bool graph_contract_valid{true};
    std::string graph_contract_summary;
    bool pass_contract_valid{true};
    std::string pass_contract_summary;
    std::vector<RenderFeatureGraphResource> graph_inputs;
    std::vector<RenderFeatureGraphResource> graph_outputs;
    std::vector<RenderFeaturePassInfo> passes;
    RenderFeatureDiagnostics diagnostics;
};

enum class RenderFeatureSceneInputFlags : uint32_t {
    None = 0,
    SceneColor = 1 << 0,
    Depth = 1 << 1,
    GBufferBaseColor = 1 << 2,
    GBufferNormalMetallic = 1 << 3,
    GBufferWorldPositionRoughness = 1 << 4,
    GBufferEmissiveAo = 1 << 5,
    Velocity = 1 << 6,
    ShadowMap = 1 << 7,
};

inline RenderFeatureSceneInputFlags operator|(RenderFeatureSceneInputFlags lhs,
                                              RenderFeatureSceneInputFlags rhs) noexcept
{
    return static_cast<RenderFeatureSceneInputFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RenderFeatureSceneInputFlags& operator|=(RenderFeatureSceneInputFlags& lhs,
                                                RenderFeatureSceneInputFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool operator&(RenderFeatureSceneInputFlags lhs, RenderFeatureSceneInputFlags rhs) noexcept
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

enum class RenderFeatureResourceFlags : uint32_t {
    None = 0,
    GraphicsPipeline = 1 << 0,
    ComputePipeline = 1 << 1,
    SampledTexture = 1 << 2,
    StorageTexture = 1 << 3,
    ColorAttachment = 1 << 4,
    DepthAttachment = 1 << 5,
    UniformBuffer = 1 << 6,
    StorageBuffer = 1 << 7,
    Sampler = 1 << 8,
};

inline RenderFeatureResourceFlags operator|(RenderFeatureResourceFlags lhs, RenderFeatureResourceFlags rhs) noexcept
{
    return static_cast<RenderFeatureResourceFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RenderFeatureResourceFlags& operator|=(RenderFeatureResourceFlags& lhs,
                                              RenderFeatureResourceFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool operator&(RenderFeatureResourceFlags lhs, RenderFeatureResourceFlags rhs) noexcept
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

enum class RenderFeatureLightingOutputFlags : uint32_t {
    None = 0,
    AmbientOcclusion = 1 << 0,
    Reflection = 1 << 1,
    IndirectDiffuse = 1 << 2,
    IndirectSpecular = 1 << 3,
};

inline RenderFeatureLightingOutputFlags operator|(RenderFeatureLightingOutputFlags lhs,
                                                  RenderFeatureLightingOutputFlags rhs) noexcept
{
    return static_cast<RenderFeatureLightingOutputFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RenderFeatureLightingOutputFlags& operator|=(RenderFeatureLightingOutputFlags& lhs,
                                                    RenderFeatureLightingOutputFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool operator&(RenderFeatureLightingOutputFlags lhs, RenderFeatureLightingOutputFlags rhs) noexcept
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

enum class RenderFeatureRHICapabilityFlags : uint32_t {
    None = 0,
    DefaultRenderFlow = 1 << 0,
    ImGui = 1 << 1,
    ScenePickReadback = 1 << 2,
    GpuTimestamp = 1 << 3,
};

inline RenderFeatureRHICapabilityFlags operator|(RenderFeatureRHICapabilityFlags lhs,
                                                 RenderFeatureRHICapabilityFlags rhs) noexcept
{
    return static_cast<RenderFeatureRHICapabilityFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RenderFeatureRHICapabilityFlags& operator|=(RenderFeatureRHICapabilityFlags& lhs,
                                                   RenderFeatureRHICapabilityFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool operator&(RenderFeatureRHICapabilityFlags lhs, RenderFeatureRHICapabilityFlags rhs) noexcept
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

struct RenderFeatureRequirements {
    RenderFeatureSceneInputFlags scene_inputs{RenderFeatureSceneInputFlags::None};
    RenderFeatureResourceFlags resources{RenderFeatureResourceFlags::None};
    RenderFeatureLightingOutputFlags lighting_outputs{RenderFeatureLightingOutputFlags::None};
    RenderFeatureRHICapabilityFlags rhi_capabilities{RenderFeatureRHICapabilityFlags::None};
    std::span<const RenderFeatureGraphResource> graph_inputs{};
    std::span<const RenderFeatureGraphResource> graph_outputs{};
    bool requires_framebuffer_size{false};
    bool uses_persistent_resources{false};
    bool uses_history_resources{false};
    bool uses_temporal_jitter{false};
};

struct RenderFeatureContract {
    std::string_view name;
    std::string_view display_name;
    std::string_view category;
    bool runtime_toggleable{false};
    RenderFeatureRequirements requirements{};
};

enum class RenderFeatureParameterType : uint8_t {
    Bool,
    Int,
    Float,
    Color,
};

struct RenderFeatureParameterValue {
    RenderFeatureParameterType type{RenderFeatureParameterType::Float};
    bool bool_value{false};
    int32_t int_value{0};
    float float_value{0.0f};
    glm::vec4 color_value{1.0f};
};

struct RenderFeatureParameterInfo {
    std::string_view name;
    std::string_view display_name;
    RenderFeatureParameterType type{RenderFeatureParameterType::Float};
    RenderFeatureParameterValue value{};
    RenderFeatureParameterValue min{};
    RenderFeatureParameterValue max{};
    float step{0.01f};
    bool read_only{false};
};

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;

    [[nodiscard]] virtual RenderFeatureContract contract() const noexcept = 0;
    [[nodiscard]] virtual bool enabled() const noexcept
    {
        return true;
    }
    [[nodiscard]] virtual RenderFeatureInfo info() const noexcept
    {
        const RenderFeatureContract feature_contract = contract();
        return RenderFeatureInfo{
            .name = feature_contract.name,
            .display_name = feature_contract.display_name,
            .category = feature_contract.category,
            .enabled = enabled(),
            .runtime_toggleable = feature_contract.runtime_toggleable,
        };
    }
    [[nodiscard]] virtual std::vector<RenderFeatureParameterInfo> parameters() const
    {
        return {};
    }
    [[nodiscard]] virtual RenderFeatureRequirements requirements() const noexcept
    {
        return contract().requirements;
    }
    [[nodiscard]] virtual RenderFeatureDiagnostics diagnostics() const
    {
        return {};
    }
    virtual bool setEnabled(bool enabled) noexcept
    {
        (void) enabled;
        return false;
    }
    virtual bool setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept
    {
        (void) name;
        (void) value;
        return false;
    }
    virtual bool registerPasses(RenderFlowBuilder& builder) = 0;
    virtual void prepareFrame(const RenderWorld& world,
                              const SceneRenderContext& scene_context,
                              const RenderFeatureFrameContext& frame_context,
                              RenderPassBlackboard& blackboard) = 0;
    virtual void commitFrame() {}
    virtual void releasePersistentResources() {}
    virtual void shutdown() = 0;
};

} // namespace luna::render_flow
