#include "Renderer/RenderFlow/Features/FeatureProbe.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
#include "Renderer/RenderGraphBuilder.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace luna::render_flow {
namespace {

inline constexpr std::string_view kFeatureName = "FeatureProbeAmbientOcclusion";
constexpr std::array<RenderFeatureGraphResource, 1> kGraphOutputs{{
    {lighting_extension_keys::AmbientOcclusion},
}};

constexpr std::array<RenderPassResourceUsage, 1> kProbePassResources{{
    {.name = lighting_extension_keys::AmbientOcclusion, .access = RenderPassResourceAccess::Write},
}};

class ProbePass final : public IRenderPass {
public:
    [[nodiscard]] const char* name() const noexcept override
    {
        return "FeatureProbeAmbientOcclusion";
    }

    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override
    {
        return kProbePassResources;
    }

    void setup(RenderPassContext& context) override
    {
        const SceneRenderContext& scene_context = context.sceneContext();
        if (!m_logged) {
            LUNA_RENDERER_INFO("FeatureProbeAmbientOcclusion writing fixed ambient occlusion value 0.0 at {}x{}",
                               scene_context.framebuffer_width,
                               scene_context.framebuffer_height);
            m_logged = true;
        }

        RenderGraphTextureHandle ambient_occlusion = context.graph().CreateTexture(RenderGraphTextureDesc{
            .Name = "FeatureProbeAmbientOcclusion",
            .Type = luna::RHI::TextureType::Texture2D,
            .Width = scene_context.framebuffer_width,
            .Height = scene_context.framebuffer_height,
            .Depth = 1,
            .ArrayLayers = 1,
            .MipLevels = 1,
            .Format = luna::RHI::Format::RGBA8_UNORM,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        });
        if (!ambient_occlusion.isValid()) {
            return;
        }

        setLightingExtensionInput(context.blackboard(), LightingExtensionInput::AmbientOcclusion, ambient_occlusion);
        context.graph().AddRasterPass(
            name(),
            [ambient_occlusion](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.WriteColor(ambient_occlusion,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [](RenderGraphRasterPassContext& pass_context) {
                pass_context.beginRendering();
                pass_context.endRendering();
            });
    }

private:
    bool m_logged{false};
};

} // namespace

RenderFeatureContract FeatureProbe::contract() const noexcept
{
    return RenderFeatureContract{
        .name = kFeatureName,
        .display_name = "Feature Probe Ambient Occlusion",
        .category = "Debug",
        .runtime_toggleable = false,
        .requirements = RenderFeatureRequirements{
            .resources = RenderFeatureResourceFlags::ColorAttachment,
            .lighting_outputs = RenderFeatureLightingOutputFlags::AmbientOcclusion,
            .rhi_capabilities = RenderFeatureRHICapabilityFlags::DefaultRenderFlow,
            .graph_outputs = kGraphOutputs,
            .requires_framebuffer_size = true,
        },
    };
}

bool FeatureProbe::registerPasses(RenderFlowBuilder& builder)
{
    namespace extension_slots = luna::render_flow::slots::extension_points;

    const bool registered = builder.insertFeaturePassBetween(kFeatureName,
                                                            extension_slots::AfterGBuffer,
                                                            extension_slots::BeforeLighting,
                                                            "FeatureProbeAmbientOcclusion",
                                                            std::make_unique<ProbePass>());
    if (registered) {
        LUNA_RENDERER_INFO("Registered FeatureProbeAmbientOcclusion between '{}' and '{}'",
                           extension_slots::AfterGBuffer,
                           extension_slots::BeforeLighting);
    }
    return registered;
}

void FeatureProbe::prepareFrame(const RenderWorld& world,
                                const SceneRenderContext& scene_context,
                                const RenderFeatureFrameContext& frame_context,
                                RenderPassBlackboard& blackboard)
{
    (void) world;
    (void) scene_context;
    (void) frame_context;
    (void) blackboard;
}

void FeatureProbe::shutdown() {}

} // namespace luna::render_flow
