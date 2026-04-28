#include "Renderer/RenderFlow/RenderFeatureResources.h"

#include "Renderer/RenderGraphBuilder.h"

#include <Builders.h>
#include <Device.h>

#include <utility>

namespace luna::render_flow {

namespace {

bool isValidDesc(const PersistentTexture2DDesc& desc) noexcept
{
    return desc.width > 0 && desc.height > 0 && desc.format != luna::RHI::Format::UNDEFINED;
}

bool descsEqual(const PersistentTexture2DDesc& lhs, const PersistentTexture2DDesc& rhs) noexcept
{
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.format == rhs.format && lhs.usage == rhs.usage &&
           lhs.initial_state == rhs.initial_state && lhs.sample_count == rhs.sample_count && lhs.name == rhs.name;
}

PersistentTexture2DDesc historyDesc(PersistentTexture2DDesc desc, uint32_t index)
{
    const std::string base_name = desc.name.empty() ? "RenderFeatureHistory" : desc.name;
    desc.name = base_name + "_History" + std::to_string(index);
    return desc;
}

} // namespace

bool PersistentTexture2D::ensure(const luna::RHI::Ref<luna::RHI::Device>& device,
                                 const PersistentTexture2DDesc& desc)
{
    if (!device || !isValidDesc(desc)) {
        reset();
        return false;
    }

    if (matches(device, desc)) {
        return true;
    }

    reset();

    m_texture = device->CreateTexture(luna::RHI::TextureBuilder()
                                          .SetType(luna::RHI::TextureType::Texture2D)
                                          .SetSize(desc.width, desc.height)
                                          .SetFormat(desc.format)
                                          .SetUsage(desc.usage)
                                          .SetInitialState(desc.initial_state)
                                          .SetSampleCount(desc.sample_count)
                                          .SetName(desc.name)
                                          .Build());
    if (!m_texture) {
        return false;
    }

    m_device = device;
    m_desc = desc;
    m_known_state = desc.initial_state;
    return true;
}

void PersistentTexture2D::reset() noexcept
{
    m_texture.reset();
    m_device.reset();
    m_desc = {};
    m_known_state = luna::RHI::ResourceState::Undefined;
}

const luna::RHI::Ref<luna::RHI::Texture>& PersistentTexture2D::texture() const noexcept
{
    return m_texture;
}

const PersistentTexture2DDesc& PersistentTexture2D::desc() const noexcept
{
    return m_desc;
}

luna::RHI::ResourceState PersistentTexture2D::knownState() const noexcept
{
    return m_known_state;
}

bool PersistentTexture2D::isValid() const noexcept
{
    return m_texture != nullptr;
}

void PersistentTexture2D::setKnownState(luna::RHI::ResourceState state) noexcept
{
    m_known_state = state;
}

bool PersistentTexture2D::matches(const luna::RHI::Ref<luna::RHI::Device>& device,
                                  const PersistentTexture2DDesc& desc) const noexcept
{
    if (!m_texture || m_device.lock() != device || !descsEqual(m_desc, desc)) {
        return false;
    }

    return m_texture->GetType() == luna::RHI::TextureType::Texture2D && m_texture->GetWidth() == desc.width &&
           m_texture->GetHeight() == desc.height && m_texture->GetDepth() == 1 && m_texture->GetMipLevels() == 1 &&
           m_texture->GetArrayLayers() == 1 && m_texture->GetFormat() == desc.format &&
           m_texture->GetUsage() == desc.usage && m_texture->GetSampleCount() == desc.sample_count;
}

bool HistoryTexture2D::ensure(const luna::RHI::Ref<luna::RHI::Device>& device,
                              const PersistentTexture2DDesc& desc)
{
    if (!isValidDesc(desc)) {
        reset();
        return false;
    }

    if (m_initialized && !descsEqual(m_desc, desc)) {
        reset();
    }

    const auto previous_texture_0 = m_textures[0].texture();
    const auto previous_texture_1 = m_textures[1].texture();
    const bool texture_0_ready = m_textures[0].ensure(device, historyDesc(desc, 0));
    const bool texture_1_ready = m_textures[1].ensure(device, historyDesc(desc, 1));
    if (!texture_0_ready || !texture_1_ready) {
        reset();
        return false;
    }

    if (m_initialized &&
        (m_textures[0].texture() != previous_texture_0 || m_textures[1].texture() != previous_texture_1)) {
        invalidateHistory();
    }

    m_desc = desc;
    m_initialized = true;
    return true;
}

void HistoryTexture2D::beginFrame(const RenderFeatureFrameContext& frame_context) noexcept
{
    m_frame_written = false;
    if (frame_context.historyInvalidated()) {
        invalidateHistory();
    }
}

void HistoryTexture2D::markFrameWritten() noexcept
{
    if (m_initialized) {
        m_frame_written = true;
    }
}

void HistoryTexture2D::commitFrame() noexcept
{
    if (!m_frame_written) {
        return;
    }

    advanceFrame();
}

void HistoryTexture2D::advanceFrame() noexcept
{
    if (!m_initialized) {
        m_frame_written = false;
        return;
    }

    m_has_readable_history = true;
    m_write_index = 1u - m_write_index;
    m_frame_written = false;
}

void HistoryTexture2D::invalidateHistory() noexcept
{
    m_has_readable_history = false;
}

void HistoryTexture2D::reset() noexcept
{
    for (auto& texture : m_textures) {
        texture.reset();
    }
    m_desc = {};
    m_write_index = 0;
    m_initialized = false;
    m_has_readable_history = false;
    m_frame_written = false;
}

const luna::RHI::Ref<luna::RHI::Texture>& HistoryTexture2D::readTexture() const noexcept
{
    return m_textures[readIndex()].texture();
}

const luna::RHI::Ref<luna::RHI::Texture>& HistoryTexture2D::writeTexture() const noexcept
{
    return m_textures[m_write_index].texture();
}

PersistentTexture2D& HistoryTexture2D::readResource() noexcept
{
    return m_textures[readIndex()];
}

PersistentTexture2D& HistoryTexture2D::writeResource() noexcept
{
    return m_textures[m_write_index];
}

const PersistentTexture2D& HistoryTexture2D::readResource() const noexcept
{
    return m_textures[readIndex()];
}

const PersistentTexture2D& HistoryTexture2D::writeResource() const noexcept
{
    return m_textures[m_write_index];
}

uint32_t HistoryTexture2D::readIndex() const noexcept
{
    return 1u - m_write_index;
}

uint32_t HistoryTexture2D::writeIndex() const noexcept
{
    return m_write_index;
}

bool HistoryTexture2D::isValid() const noexcept
{
    return m_initialized && m_textures[0].isValid() && m_textures[1].isValid();
}

bool HistoryTexture2D::hasReadableHistory() const noexcept
{
    return isValid() && m_has_readable_history;
}

bool HistoryTexture2D::wasFrameWritten() const noexcept
{
    return m_frame_written;
}

RenderGraphTextureHandle importPersistentTexture2D(luna::RenderGraphBuilder& graph,
                                                   PersistentTexture2D& texture,
                                                   const RenderFeatureTextureImportOptions& options)
{
    if (!texture.isValid()) {
        return {};
    }

    std::string import_name = options.name;
    if (import_name.empty()) {
        import_name = texture.desc().name.empty() ? "RenderFeaturePersistentTexture" : texture.desc().name;
    }

    const luna::RHI::ResourceState initial_state = options.initial_state.value_or(texture.knownState());
    const RenderGraphTextureHandle handle =
        graph.ImportTexture(std::move(import_name), texture.texture(), initial_state, options.final_state);
    if (!handle.isValid()) {
        return {};
    }

    if (options.export_texture) {
        graph.ExportTexture(handle, options.final_state);
        texture.setKnownState(options.final_state);
    }

    return handle;
}

RenderGraphTextureHandle importHistoryReadTexture2D(luna::RenderGraphBuilder& graph,
                                                    HistoryTexture2D& history,
                                                    const RenderFeatureTextureImportOptions& options)
{
    if (!history.hasReadableHistory()) {
        return {};
    }

    return importPersistentTexture2D(graph, history.readResource(), options);
}

RenderGraphTextureHandle importHistoryWriteTexture2D(luna::RenderGraphBuilder& graph,
                                                     HistoryTexture2D& history,
                                                     const RenderFeatureTextureImportOptions& options)
{
    if (!history.isValid()) {
        return {};
    }

    const RenderGraphTextureHandle handle = importPersistentTexture2D(graph, history.writeResource(), options);
    if (handle.isValid()) {
        history.markFrameWritten();
    }
    return handle;
}

} // namespace luna::render_flow
