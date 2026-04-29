#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderFlow/RenderFeatureSupport.h"
#include "Renderer/RenderFlow/RenderFlow.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace luna {

class DefaultRenderFlow final : public IRenderFlow {
public:
    using ConfigureFunction = std::function<void(render_flow::RenderFlowBuilder&)>;

    DefaultRenderFlow();
    ~DefaultRenderFlow() override;

    void render(RenderFlowContext& context) override;
    bool commitFrame() override;
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
    struct FeatureRuntimeState {
        bool supported{true};
        bool active_this_frame{false};
        bool prepared_this_frame{false};
        std::string support_summary{"not evaluated"};
        bool graph_contract_valid{true};
        std::string graph_contract_summary{"not evaluated"};
        bool pass_contract_valid{true};
        std::string pass_contract_summary{"not evaluated"};
        std::vector<std::string> owned_passes;
    };

    void installRegisteredFeatures();
    [[nodiscard]] bool hasFeature(std::string_view name) const;
    [[nodiscard]] FeatureRuntimeState* findFeatureRuntimeState(std::string_view name);
    [[nodiscard]] const FeatureRuntimeState* findFeatureRuntimeState(std::string_view name) const;
    [[nodiscard]] bool isPassActive(std::string_view name) const;
    void logFeatureSupportDiagnostics(const render_flow::IRenderFeature& feature,
                                      const render_flow::RenderFeatureSupportResult& support);
    void validateFeatureGraphContracts();
    void validatePassGraphContracts();

private:
    std::vector<std::unique_ptr<render_flow::IRenderFeature>> m_features;
    render_flow::RenderPassBlackboard m_blackboard;
    render_flow::RenderFlowBuilder m_builder;
    std::unordered_map<std::string, FeatureRuntimeState> m_feature_runtime_states;
    bool m_contract_diagnostics_dirty{true};
    bool m_frame_ready_to_commit{false};
};

} // namespace luna
