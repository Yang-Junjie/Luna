#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <functional>
#include <string_view>
#include <vector>

namespace luna {

class RenderFeaturesPanel {
public:
    using FeatureList = std::vector<render_flow::RenderFeatureInfo>;
    using ParameterList = std::vector<render_flow::RenderFeatureParameterInfo>;
    using GetParametersFunction = std::function<ParameterList(std::string_view feature_name)>;
    using SetFeatureEnabledFunction = std::function<bool(std::string_view feature_name, bool enabled)>;
    using SetParameterFunction = std::function<bool(std::string_view feature_name,
                                                    std::string_view parameter_name,
                                                    const render_flow::RenderFeatureParameterValue& value)>;

    void onImGuiRender(bool& open,
                       const FeatureList& features,
                       const GetParametersFunction& get_parameters,
                       const SetFeatureEnabledFunction& set_feature_enabled,
                       const SetParameterFunction& set_parameter);
};

} // namespace luna
