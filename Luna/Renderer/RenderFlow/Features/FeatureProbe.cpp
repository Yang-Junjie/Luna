#include "Renderer/RenderFlow/Features/FeatureProbe.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
#include "Renderer/RenderGraphBuilder.h"

#include <memory>
#include <string>

namespace luna::render_flow {
namespace {

class ProbePass final : public IRenderPass {
public:
    [[nodiscard]] const char* name() const noexcept override
    {
        return "FeatureProbeAmbientOcclusion";
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

RenderFeatureInfo FeatureProbe::info() const noexcept
{
    return RenderFeatureInfo{
        .name = "FeatureProbeAmbientOcclusion",
        .display_name = "Feature Probe Ambient Occlusion",
        .category = "Debug",
        .enabled = true,
        .runtime_toggleable = false,
    };
}

bool FeatureProbe::registerPasses(RenderFlowBuilder& builder)
{
    namespace extension_slots = luna::render_flow::slots::extension_points;

    const bool registered = builder.insertPassBetween(extension_slots::AfterGBuffer,
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
                                RenderPassBlackboard& blackboard)
{
    (void) world;
    (void) scene_context;
    (void) blackboard;
}

void FeatureProbe::shutdown() {}

} // namespace luna::render_flow
