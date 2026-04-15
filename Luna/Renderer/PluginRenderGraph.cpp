#include "Renderer/PluginRenderGraphInternal.h"

#include "Core/Application.h"
#include "Core/Log.h"
#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/RenderGraphBuilder.h"

#include <algorithm>

namespace {

class PluginClearColorPass final : public luna::val::RenderPass {
public:
    PluginClearColorPass(luna::val::Format surface_format, uint32_t width, uint32_t height)
        : m_surface_format(surface_format)
        , m_width(width)
        , m_height(height)
    {}

    void SetupPipeline(luna::val::PipelineState pipeline) override
    {
        pipeline.DeclareAttachment("scene_color", m_surface_format, m_width, m_height);
        pipeline.AddOutputAttachment("scene_color", luna::val::ClearColor{0.10f, 0.10f, 0.12f, 1.0f});
    }

private:
    luna::val::Format m_surface_format{luna::val::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
};

bool isSupportedDefinition(const luna::render::RenderGraphDefinition& definition)
{
    const auto& passes = definition.passes();
    if (passes.empty()) {
        return false;
    }

    if (passes.size() == 1) {
        return passes.front().m_kind == luna::render::RenderGraphPassKind::Scene ||
               passes.front().m_kind == luna::render::RenderGraphPassKind::ClearColor;
    }

    if (passes.size() == 2) {
        const auto base_kind = passes.front().m_kind;
        const auto overlay_kind = passes.back().m_kind;
        const bool valid_base =
            base_kind == luna::render::RenderGraphPassKind::Scene || base_kind == luna::render::RenderGraphPassKind::ClearColor;
        return valid_base && overlay_kind == luna::render::RenderGraphPassKind::ImGui;
    }

    return false;
}

std::unique_ptr<luna::val::RenderGraph>
    buildFromDefinition(const luna::render::RenderGraphDefinition& definition,
                        const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
{
    const auto& passes = definition.passes();
    const bool wants_imgui = std::any_of(passes.begin(), passes.end(), [](const luna::render::RenderGraphPassSpec& pass) {
        return pass.m_kind == luna::render::RenderGraphPassKind::ImGui;
    });

    if (!isSupportedDefinition(definition)) {
        LUNA_CORE_ERROR("Unsupported plugin RenderGraph definition; falling back to the internal default graph");
        return luna::Application::get().getRenderer().getSceneRenderer().buildRenderGraph(
            build_info.m_surface_format, build_info.m_framebuffer_width, build_info.m_framebuffer_height, wants_imgui);
    }

    const bool include_imgui = passes.size() == 2;

    if (passes.front().m_kind == luna::render::RenderGraphPassKind::Scene) {
        return luna::Application::get().getRenderer().getSceneRenderer().buildRenderGraph(
            build_info.m_surface_format, build_info.m_framebuffer_width, build_info.m_framebuffer_height, include_imgui);
    }

    luna::val::RenderGraphBuilder builder;
    builder.AddRenderPass(
        passes.front().m_name,
        std::make_unique<PluginClearColorPass>(
            build_info.m_surface_format, build_info.m_framebuffer_width, build_info.m_framebuffer_height));

    if (include_imgui) {
        builder.AddRenderPass(passes.back().m_name, std::make_unique<luna::val::ImGuiRenderPass>("scene_color"));
    }

    builder.SetOutputName("scene_color");
    return builder.Build();
}

} // namespace

namespace luna::render::detail {

VulkanRenderer::RenderGraphBuilderCallback createRenderGraphBuilderCallback(RenderGraphFactory factory, bool include_imgui)
{
    return [factory = std::move(factory), include_imgui](const VulkanRenderer::RenderGraphBuildInfo& build_info) {
        RenderGraphBuildInfo public_build_info{
            .m_framebuffer_width = build_info.m_framebuffer_width,
            .m_framebuffer_height = build_info.m_framebuffer_height,
            .m_include_imgui = include_imgui,
        };

        RenderGraphDefinition definition = factory(public_build_info);
        return buildFromDefinition(definition, build_info);
    };
}

} // namespace luna::render::detail
