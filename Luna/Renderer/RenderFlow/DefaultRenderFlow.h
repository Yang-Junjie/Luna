#pragma once

#include "Renderer/RenderFlow/RenderFlow.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderFlow/RenderPass.h"

#include <memory>
#include <functional>
#include <string_view>
#include <vector>

namespace luna {

class DefaultRenderFlow final : public IRenderFlow {
public:
    using ConfigureFunction = std::function<void(render_flow::RenderFlowBuilder&)>;

    DefaultRenderFlow();
    ~DefaultRenderFlow() override;

    void render(RenderFlowContext& context) override;
    void shutdown();
    bool addFeature(std::unique_ptr<render_flow::IRenderFeature> feature);
    bool configure(const ConfigureFunction& configure_function);
    [[nodiscard]] std::vector<render_flow::RenderFeatureInfo> featureInfos() const;
    bool setFeatureEnabled(std::string_view name, bool enabled);
    [[nodiscard]] std::vector<render_flow::RenderFeatureParameterInfo> featureParameters(std::string_view name) const;
    bool setFeatureParameter(std::string_view feature_name,
                             std::string_view parameter_name,
                             const render_flow::RenderFeatureParameterValue& value);

    [[nodiscard]] render_flow::RenderFlowBuilder& builder() noexcept;
    [[nodiscard]] const render_flow::RenderFlowBuilder& builder() const noexcept;

private:
    void installRegisteredFeatures();
    [[nodiscard]] bool hasFeature(std::string_view name) const;

private:
    std::vector<std::unique_ptr<render_flow::IRenderFeature>> m_features;
    render_flow::RenderPassBlackboard m_blackboard;
    render_flow::RenderFlowBuilder m_builder;
};

} // namespace luna






