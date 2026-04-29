#pragma once

#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderGraph.h"

#include <cstdint>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <Texture.h>
#include <vector>

namespace luna::RHI {
class Device;
class ShaderModule;
} // namespace luna::RHI

namespace luna {
class RenderGraphBuilder;
}

namespace luna::render_flow {

struct RenderFeatureDiagnostics;
struct ShaderBindingContract;

enum class RenderFeatureGpuResourceAction : uint8_t {
    InvalidContext,
    Reuse,
    Rebuild,
};

struct RenderFeatureGpuResourceDecision {
    RenderFeatureGpuResourceAction action{RenderFeatureGpuResourceAction::InvalidContext};
    bool device_changed{false};
    bool backend_changed{false};
};

struct RenderFeatureResourceStatus {
    std::string_view name;
    bool ready{false};
};

struct RenderFeatureResourceReportEntry {
    std::string name;
    bool ready{false};
};

struct RenderFeatureResourceReport {
    bool evaluated{false};
    bool valid{true};
    std::string summary;
    std::vector<RenderFeatureResourceReportEntry> resources;
};

struct RenderFeatureShaderBindingCheck {
    luna::RHI::Ref<luna::RHI::ShaderModule> shader;
    std::string_view entry_point;
};

class RenderFeatureGpuResourceState {
public:
    RenderFeatureGpuResourceState() = default;
    explicit RenderFeatureGpuResourceState(std::string feature_name);

    void setFeatureName(std::string feature_name);
    [[nodiscard]] const std::string& featureName() const noexcept;
    [[nodiscard]] RenderFeatureGpuResourceDecision evaluate(const SceneRenderContext& context,
                                                            bool resources_complete) const noexcept;
    void bindContext(const SceneRenderContext& context) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool hasContext() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Device>& device() const noexcept;
    [[nodiscard]] luna::RHI::BackendType backendType() const noexcept;

private:
    std::string m_feature_name;
    luna::RHI::Ref<luna::RHI::Device> m_device;
    luna::RHI::BackendType m_backend_type{luna::RHI::BackendType::Auto};
};

[[nodiscard]] bool logRenderFeatureGpuResourceBuildResult(const RenderFeatureGpuResourceState& state,
                                                          std::span<const RenderFeatureResourceStatus> resources);

enum class RenderFeatureResourceScope : uint8_t {
    FrameTransient,
    ImportedExternal,
    FeaturePersistent,
};

struct PersistentTexture2DDesc {
    uint32_t width{0};
    uint32_t height{0};
    luna::RHI::Format format{luna::RHI::Format::UNDEFINED};
    luna::RHI::TextureUsageFlags usage{luna::RHI::TextureUsageFlags::Sampled};
    luna::RHI::ResourceState initial_state{luna::RHI::ResourceState::Undefined};
    luna::RHI::SampleCount sample_count{luna::RHI::SampleCount::Count1};
    std::string name;
};

struct RenderFeatureTextureImportOptions {
    std::string name;
    std::optional<luna::RHI::ResourceState> initial_state;
    luna::RHI::ResourceState final_state{luna::RHI::ResourceState::Common};
    bool export_texture{true};
};

class PersistentTexture2D {
public:
    [[nodiscard]] bool ensure(const luna::RHI::Ref<luna::RHI::Device>& device, const PersistentTexture2DDesc& desc);
    void reset() noexcept;

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Texture>& texture() const noexcept;
    [[nodiscard]] const PersistentTexture2DDesc& desc() const noexcept;
    [[nodiscard]] luna::RHI::ResourceState knownState() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    void setKnownState(luna::RHI::ResourceState state) noexcept;

private:
    [[nodiscard]] bool matches(const luna::RHI::Ref<luna::RHI::Device>& device,
                               const PersistentTexture2DDesc& desc) const noexcept;

    luna::RHI::Ref<luna::RHI::Texture> m_texture;
    std::weak_ptr<luna::RHI::Device> m_device;
    PersistentTexture2DDesc m_desc;
    luna::RHI::ResourceState m_known_state{luna::RHI::ResourceState::Undefined};
};

class HistoryTexture2D {
public:
    [[nodiscard]] bool ensure(const luna::RHI::Ref<luna::RHI::Device>& device, const PersistentTexture2DDesc& desc);
    void beginFrame(const RenderFeatureFrameContext& frame_context) noexcept;
    void markFrameWritten() noexcept;
    void commitFrame() noexcept;
    void advanceFrame() noexcept;
    void invalidateHistory() noexcept;
    void reset() noexcept;

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Texture>& readTexture() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Texture>& writeTexture() const noexcept;
    [[nodiscard]] PersistentTexture2D& readResource() noexcept;
    [[nodiscard]] PersistentTexture2D& writeResource() noexcept;
    [[nodiscard]] const PersistentTexture2D& readResource() const noexcept;
    [[nodiscard]] const PersistentTexture2D& writeResource() const noexcept;
    [[nodiscard]] uint32_t readIndex() const noexcept;
    [[nodiscard]] uint32_t writeIndex() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool hasReadableHistory() const noexcept;
    [[nodiscard]] bool wasFrameWritten() const noexcept;

private:
    std::array<PersistentTexture2D, 2> m_textures;
    PersistentTexture2DDesc m_desc;
    uint32_t m_write_index{0};
    bool m_initialized{false};
    bool m_has_readable_history{false};
    bool m_frame_written{false};
};

class RenderFeatureResourceSet {
public:
    RenderFeatureResourceSet() = default;
    explicit RenderFeatureResourceSet(std::string feature_name);

    void setFeatureName(std::string feature_name);
    [[nodiscard]] const std::string& featureName() const noexcept;

    [[nodiscard]] RenderFeatureGpuResourceDecision evaluateGpuResources(const SceneRenderContext& context,
                                                                        bool resources_complete) const noexcept;
    [[nodiscard]] RenderFeatureGpuResourceDecision prepareGpuResourceBuild(const SceneRenderContext& context,
                                                                           bool resources_complete) noexcept;
    void bindGpuContext(const SceneRenderContext& context) noexcept;
    void resetGpuContext() noexcept;
    [[nodiscard]] bool hasGpuContext() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Device>& device() const noexcept;
    [[nodiscard]] luna::RHI::BackendType backendType() const noexcept;

    [[nodiscard]] bool logGpuResourceBuildResult(std::span<const RenderFeatureResourceStatus> resources) const;
    void resetBindingContractDiagnostics() noexcept;
    bool validateShaderBindingContract(std::span<const RenderFeatureShaderBindingCheck> shaders,
                                       const ShaderBindingContract& contract,
                                       const std::filesystem::path& shader_file);
    void writeBindingContractDiagnostics(RenderFeatureDiagnostics& diagnostics) const;
    [[nodiscard]] RenderFeatureResourceReport
        gpuResourceReport(bool resources_complete, std::span<const RenderFeatureResourceStatus> resources) const;
    [[nodiscard]] RenderFeatureResourceReport
        resourceReport(bool evaluated, std::span<const RenderFeatureResourceStatus> resources) const;
    [[nodiscard]] RenderFeatureResourceReport historyResourceReport(
        bool evaluated, const HistoryTexture2D& history, std::span<const RenderFeatureResourceStatus> resources) const;
    void writePipelineResourceDiagnostics(RenderFeatureDiagnostics& diagnostics,
                                          bool resources_complete,
                                          std::span<const RenderFeatureResourceStatus> resources) const;
    void writePersistentResourceDiagnostics(RenderFeatureDiagnostics& diagnostics,
                                            bool evaluated,
                                            std::span<const RenderFeatureResourceStatus> resources) const;
    void writeHistoryResourceDiagnostics(RenderFeatureDiagnostics& diagnostics,
                                         bool evaluated,
                                         const HistoryTexture2D& history,
                                         std::span<const RenderFeatureResourceStatus> resources) const;

    [[nodiscard]] bool ensurePersistentTexture2D(PersistentTexture2D& texture,
                                                 const SceneRenderContext& context,
                                                 const PersistentTexture2DDesc& desc) const;
    void releasePersistentTexture2D(PersistentTexture2D& texture) const noexcept;
    [[nodiscard]] RenderGraphTextureHandle
        importPersistentTexture2D(luna::RenderGraphBuilder& graph,
                                  PersistentTexture2D& texture,
                                  const RenderFeatureTextureImportOptions& options = {}) const;

    [[nodiscard]] bool ensureHistoryTexture2D(HistoryTexture2D& history,
                                              const SceneRenderContext& context,
                                              const PersistentTexture2DDesc& desc) const;
    [[nodiscard]] bool hasReadableHistoryTexture2D(const HistoryTexture2D& history) const noexcept;
    [[nodiscard]] bool wasHistoryTexture2DWritten(const HistoryTexture2D& history) const noexcept;
    void beginHistoryFrame(HistoryTexture2D& history, const RenderFeatureFrameContext& frame_context) const noexcept;
    void invalidateHistoryTexture2D(HistoryTexture2D& history) const noexcept;
    void commitHistoryTexture2D(HistoryTexture2D& history) const noexcept;
    void resetHistoryTexture2D(HistoryTexture2D& history) const noexcept;
    [[nodiscard]] RenderGraphTextureHandle importHistoryReadTexture2D(
        luna::RenderGraphBuilder& graph,
        HistoryTexture2D& history,
        const RenderFeatureTextureImportOptions& options = {.final_state = luna::RHI::ResourceState::ShaderRead,
                                                            .export_texture = false}) const;
    [[nodiscard]] RenderGraphTextureHandle importHistoryWriteTexture2D(
        luna::RenderGraphBuilder& graph,
        HistoryTexture2D& history,
        const RenderFeatureTextureImportOptions& options = {.final_state = luna::RHI::ResourceState::ShaderRead}) const;

private:
    RenderFeatureGpuResourceState m_gpu_resources;
    bool m_binding_contract_valid{true};
    std::string m_binding_contract_summary{"not evaluated"};
};

[[nodiscard]] RenderGraphTextureHandle importPersistentTexture2D(luna::RenderGraphBuilder& graph,
                                                                 PersistentTexture2D& texture,
                                                                 const RenderFeatureTextureImportOptions& options = {});
[[nodiscard]] RenderGraphTextureHandle
    importHistoryReadTexture2D(luna::RenderGraphBuilder& graph,
                               HistoryTexture2D& history,
                               const RenderFeatureTextureImportOptions& options = {
                                   .final_state = luna::RHI::ResourceState::ShaderRead, .export_texture = false});
[[nodiscard]] RenderGraphTextureHandle importHistoryWriteTexture2D(
    luna::RenderGraphBuilder& graph,
    HistoryTexture2D& history,
    const RenderFeatureTextureImportOptions& options = {.final_state = luna::RHI::ResourceState::ShaderRead});

} // namespace luna::render_flow
