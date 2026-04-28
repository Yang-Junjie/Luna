#pragma once

#include "Renderer/RenderGraph.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"

#include <Texture.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace luna::RHI {
class Device;
}

namespace luna {
class RenderGraphBuilder;
}

namespace luna::render_flow {

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
    [[nodiscard]] bool ensure(const luna::RHI::Ref<luna::RHI::Device>& device,
                              const PersistentTexture2DDesc& desc);
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
    [[nodiscard]] bool ensure(const luna::RHI::Ref<luna::RHI::Device>& device,
                              const PersistentTexture2DDesc& desc);
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

[[nodiscard]] RenderGraphTextureHandle importPersistentTexture2D(
    luna::RenderGraphBuilder& graph,
    PersistentTexture2D& texture,
    const RenderFeatureTextureImportOptions& options = {});
[[nodiscard]] RenderGraphTextureHandle importHistoryReadTexture2D(
    luna::RenderGraphBuilder& graph,
    HistoryTexture2D& history,
    const RenderFeatureTextureImportOptions& options = {.final_state = luna::RHI::ResourceState::ShaderRead,
                                                        .export_texture = false});
[[nodiscard]] RenderGraphTextureHandle importHistoryWriteTexture2D(
    luna::RenderGraphBuilder& graph,
    HistoryTexture2D& history,
    const RenderFeatureTextureImportOptions& options = {.final_state = luna::RHI::ResourceState::ShaderRead});

} // namespace luna::render_flow
