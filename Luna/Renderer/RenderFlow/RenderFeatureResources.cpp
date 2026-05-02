#include "Renderer/RenderFlow/RenderFeatureResources.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderFlow/RenderFeatureBindingContract.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RendererUtilities.h"

#include <Backend.h>
#include <Builders.h>
#include <Device.h>

#include <string>
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

RenderFeatureResourceReport makeResourceReport(bool evaluated,
                                                bool resources_complete,
                                                std::span<const RenderFeatureResourceStatus> resources)
{
    RenderFeatureResourceReport report;
    report.evaluated = evaluated;
    bool complete = evaluated && resources_complete;
    for (const RenderFeatureResourceStatus& resource : resources) {
        complete = complete && resource.ready;
        if (evaluated) {
            report.resources.push_back(RenderFeatureResourceReportEntry{
                .name = std::string(resource.name),
                .ready = resource.ready,
            });
        }
    }

    report.valid = !evaluated || complete;
    report.summary = evaluated ? (complete ? "ok" : "incomplete") : "not evaluated";
    return report;
}

void copyResourceReportEntries(const RenderFeatureResourceReport& report,
                               std::vector<RenderFeatureStatusEntry>& target)
{
    target.clear();
    target.reserve(report.resources.size());
    for (const RenderFeatureResourceReportEntry& resource : report.resources) {
        target.push_back(RenderFeatureStatusEntry{
            .name = resource.name,
            .ready = resource.ready,
        });
    }
}

} // namespace

RenderFeatureGpuResourceState::RenderFeatureGpuResourceState(std::string feature_name)
    : m_feature_name(std::move(feature_name))
{}

void RenderFeatureGpuResourceState::setFeatureName(std::string feature_name)
{
    m_feature_name = std::move(feature_name);
}

const std::string& RenderFeatureGpuResourceState::featureName() const noexcept
{
    return m_feature_name;
}

RenderFeatureGpuResourceDecision RenderFeatureGpuResourceState::evaluate(const SceneRenderContext& context,
                                                                         bool resources_complete) const noexcept
{
    if (!context.device || !context.compiler) {
        return RenderFeatureGpuResourceDecision{
            .action = RenderFeatureGpuResourceAction::InvalidContext,
        };
    }

    if (!m_device) {
        return RenderFeatureGpuResourceDecision{
            .action = RenderFeatureGpuResourceAction::Rebuild,
        };
    }

    const bool device_changed = m_device != context.device;
    const bool backend_changed = m_backend_type != context.backend_type;
    if (!device_changed && !backend_changed && resources_complete) {
        return RenderFeatureGpuResourceDecision{
            .action = RenderFeatureGpuResourceAction::Reuse,
        };
    }

    return RenderFeatureGpuResourceDecision{
        .action = RenderFeatureGpuResourceAction::Rebuild,
        .device_changed = device_changed,
        .backend_changed = backend_changed,
    };
}

void RenderFeatureGpuResourceState::bindContext(const SceneRenderContext& context) noexcept
{
    m_device = context.device;
    m_backend_type = context.backend_type;
}

void RenderFeatureGpuResourceState::reset() noexcept
{
    m_device.reset();
    m_backend_type = luna::RHI::BackendType::Auto;
}

bool RenderFeatureGpuResourceState::hasContext() const noexcept
{
    return m_device != nullptr;
}

const luna::RHI::Ref<luna::RHI::Device>& RenderFeatureGpuResourceState::device() const noexcept
{
    return m_device;
}

luna::RHI::BackendType RenderFeatureGpuResourceState::backendType() const noexcept
{
    return m_backend_type;
}

bool logRenderFeatureGpuResourceBuildResult(const RenderFeatureGpuResourceState& state,
                                            std::span<const RenderFeatureResourceStatus> resources)
{
    bool complete = state.hasContext();
    for (const RenderFeatureResourceStatus& resource : resources) {
        complete = complete && resource.ready;
    }

    const std::string_view feature_name = state.featureName().empty() ? std::string_view("RenderFeature")
                                                                      : std::string_view(state.featureName());
    if (complete) {
        LUNA_RENDERER_INFO("Created GPU resources for feature '{}' on backend '{}'",
                           feature_name,
                           luna::RHI::BackendTypeToString(state.backendType()));
        return true;
    }

    std::string status;
    for (const RenderFeatureResourceStatus& resource : resources) {
        if (!status.empty()) {
            status += " ";
        }
        status += resource.name;
        status += "=";
        status += resource.ready ? "true" : "false";
    }
    if (status.empty()) {
        status = "<none>";
    }

    LUNA_RENDERER_WARN("{} GPU resources are incomplete: {}", feature_name, status);
    return false;
}

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

RenderFeatureResourceSet::RenderFeatureResourceSet(std::string feature_name)
    : m_gpu_resources(std::move(feature_name))
{}

void RenderFeatureResourceSet::setFeatureName(std::string feature_name)
{
    m_gpu_resources.setFeatureName(std::move(feature_name));
}

const std::string& RenderFeatureResourceSet::featureName() const noexcept
{
    return m_gpu_resources.featureName();
}

RenderFeatureGpuResourceDecision RenderFeatureResourceSet::evaluateGpuResources(
    const SceneRenderContext& context,
    bool resources_complete) const noexcept
{
    return m_gpu_resources.evaluate(context, resources_complete);
}

RenderFeatureGpuResourceDecision RenderFeatureResourceSet::prepareGpuResourceBuild(
    const SceneRenderContext& context,
    bool resources_complete) noexcept
{
    const RenderFeatureGpuResourceDecision decision = evaluateGpuResources(context, resources_complete);
    if (decision.action == RenderFeatureGpuResourceAction::Rebuild) {
        bindGpuContext(context);
    }
    return decision;
}

void RenderFeatureResourceSet::bindGpuContext(const SceneRenderContext& context) noexcept
{
    m_gpu_resources.bindContext(context);
}

void RenderFeatureResourceSet::resetGpuContext() noexcept
{
    m_gpu_resources.reset();
}

bool RenderFeatureResourceSet::hasGpuContext() const noexcept
{
    return m_gpu_resources.hasContext();
}

const luna::RHI::Ref<luna::RHI::Device>& RenderFeatureResourceSet::device() const noexcept
{
    return m_gpu_resources.device();
}

luna::RHI::BackendType RenderFeatureResourceSet::backendType() const noexcept
{
    return m_gpu_resources.backendType();
}

bool RenderFeatureResourceSet::logGpuResourceBuildResult(
    std::span<const RenderFeatureResourceStatus> resources) const
{
    return logRenderFeatureGpuResourceBuildResult(m_gpu_resources, resources);
}

void RenderFeatureResourceSet::resetBindingContractDiagnostics() noexcept
{
    m_binding_contract_valid = true;
    m_binding_contract_summary = "not evaluated";
}

bool RenderFeatureResourceSet::validateShaderBindingContract(
    std::span<const RenderFeatureShaderBindingCheck> shaders,
    const ShaderBindingContract& contract,
    const std::filesystem::path& shader_file)
{
    bool complete = true;
    bool missing_shader = false;
    for (const RenderFeatureShaderBindingCheck& shader : shaders) {
        if (!shader.shader) {
            complete = false;
            missing_shader = true;
            continue;
        }

        complete = luna::render_flow::validateAndLogRenderFeatureShaderModuleBindings(
                       shader.shader, contract, shader_file, shader.entry_point) &&
                   complete;
    }

    m_binding_contract_valid = complete;
    if (missing_shader) {
        m_binding_contract_summary = "shader module missing";
    } else if (!complete) {
        m_binding_contract_summary = "shader reflection mismatch; see renderer log";
    } else {
        m_binding_contract_summary = "ok";
    }

    return m_binding_contract_valid;
}

void RenderFeatureResourceSet::writeBindingContractDiagnostics(RenderFeatureDiagnostics& diagnostics) const
{
    diagnostics.binding_contract_valid = m_binding_contract_valid;
    diagnostics.binding_contract_summary = m_binding_contract_summary;
}

RenderFeatureResourceReport RenderFeatureResourceSet::gpuResourceReport(
    bool resources_complete,
    std::span<const RenderFeatureResourceStatus> resources) const
{
    return makeResourceReport(m_gpu_resources.hasContext(), resources_complete, resources);
}

RenderFeatureResourceReport RenderFeatureResourceSet::resourceReport(
    bool evaluated,
    std::span<const RenderFeatureResourceStatus> resources) const
{
    return makeResourceReport(evaluated, true, resources);
}

RenderFeatureResourceReport RenderFeatureResourceSet::historyResourceReport(
    bool evaluated,
    const HistoryTexture2D& history,
    std::span<const RenderFeatureResourceStatus> resources) const
{
    RenderFeatureResourceReport report = resourceReport(evaluated, resources);
    if (!evaluated) {
        return report;
    }

    report.valid = report.valid && history.isValid();
    if (!history.isValid()) {
        report.summary = "incomplete";
    } else {
        report.summary = history.hasReadableHistory() ? "ok" : "warming up";
    }
    return report;
}

void RenderFeatureResourceSet::writePipelineResourceDiagnostics(
    RenderFeatureDiagnostics& diagnostics,
    bool resources_complete,
    std::span<const RenderFeatureResourceStatus> resources) const
{
    const RenderFeatureResourceReport report = gpuResourceReport(resources_complete, resources);
    diagnostics.pipeline_resources_valid = report.valid;
    diagnostics.pipeline_resources_summary = report.summary;
    copyResourceReportEntries(report, diagnostics.pipeline_resources);
}

void RenderFeatureResourceSet::writePersistentResourceDiagnostics(
    RenderFeatureDiagnostics& diagnostics,
    bool evaluated,
    std::span<const RenderFeatureResourceStatus> resources) const
{
    const RenderFeatureResourceReport report = resourceReport(evaluated, resources);
    diagnostics.persistent_resources_valid = report.valid;
    diagnostics.persistent_resources_summary = report.summary;
    copyResourceReportEntries(report, diagnostics.persistent_resources);
}

void RenderFeatureResourceSet::writeHistoryResourceDiagnostics(
    RenderFeatureDiagnostics& diagnostics,
    bool evaluated,
    const HistoryTexture2D& history,
    std::span<const RenderFeatureResourceStatus> resources) const
{
    const RenderFeatureResourceReport report = historyResourceReport(evaluated, history, resources);
    diagnostics.history_resources_valid = report.valid;
    diagnostics.history_resources_summary = report.summary;
    copyResourceReportEntries(report, diagnostics.history_resources);
}

bool RenderFeatureResourceSet::ensurePersistentTexture2D(PersistentTexture2D& texture,
                                                         const SceneRenderContext& context,
                                                         const PersistentTexture2DDesc& desc) const
{
    return texture.ensure(context.device, desc);
}

void RenderFeatureResourceSet::releasePersistentTexture2D(PersistentTexture2D& texture) const noexcept
{
    texture.reset();
}

RenderGraphTextureHandle RenderFeatureResourceSet::importPersistentTexture2D(
    luna::RenderGraphBuilder& graph,
    PersistentTexture2D& texture,
    const RenderFeatureTextureImportOptions& options) const
{
    return luna::render_flow::importPersistentTexture2D(graph, texture, options);
}

bool RenderFeatureResourceSet::ensureHistoryTexture2D(HistoryTexture2D& history,
                                                      const SceneRenderContext& context,
                                                      const PersistentTexture2DDesc& desc) const
{
    if (!context.device) {
        history.reset();
        return false;
    }

    return history.ensure(context.device, desc);
}

bool RenderFeatureResourceSet::hasReadableHistoryTexture2D(const HistoryTexture2D& history) const noexcept
{
    return history.hasReadableHistory();
}

bool RenderFeatureResourceSet::wasHistoryTexture2DWritten(const HistoryTexture2D& history) const noexcept
{
    return history.wasFrameWritten();
}

void RenderFeatureResourceSet::beginHistoryFrame(HistoryTexture2D& history,
                                                 const RenderFeatureFrameContext& frame_context) const noexcept
{
    history.beginFrame(frame_context);
}

void RenderFeatureResourceSet::invalidateHistoryTexture2D(HistoryTexture2D& history) const noexcept
{
    history.invalidateHistory();
}

void RenderFeatureResourceSet::commitHistoryTexture2D(HistoryTexture2D& history) const noexcept
{
    history.commitFrame();
}

void RenderFeatureResourceSet::resetHistoryTexture2D(HistoryTexture2D& history) const noexcept
{
    history.reset();
}

RenderGraphTextureHandle RenderFeatureResourceSet::importHistoryReadTexture2D(
    luna::RenderGraphBuilder& graph,
    HistoryTexture2D& history,
    const RenderFeatureTextureImportOptions& options) const
{
    return luna::render_flow::importHistoryReadTexture2D(graph, history, options);
}

RenderGraphTextureHandle RenderFeatureResourceSet::importHistoryWriteTexture2D(
    luna::RenderGraphBuilder& graph,
    HistoryTexture2D& history,
    const RenderFeatureTextureImportOptions& options) const
{
    return luna::render_flow::importHistoryWriteTexture2D(graph, history, options);
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
