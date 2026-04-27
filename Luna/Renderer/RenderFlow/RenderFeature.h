#pragma once

#include "Renderer/RenderFlow/RenderFlowTypes.h"

#include <cstdint>
#include <glm/vec4.hpp>
#include <string_view>
#include <vector>

namespace luna {
class RenderWorld;
}

namespace luna::render_flow {

class RenderFlowBuilder;
class RenderPassBlackboard;

struct RenderFeatureInfo {
    std::string_view name;
    std::string_view display_name;
    std::string_view category;
    bool enabled{true};
    bool runtime_toggleable{false};
};

enum class RenderFeatureParameterType : uint8_t {
    Bool,
    Int,
    Float,
    Color,
};

struct RenderFeatureParameterValue {
    RenderFeatureParameterType type{RenderFeatureParameterType::Float};
    bool bool_value{false};
    int32_t int_value{0};
    float float_value{0.0f};
    glm::vec4 color_value{1.0f};
};

struct RenderFeatureParameterInfo {
    std::string_view name;
    std::string_view display_name;
    RenderFeatureParameterType type{RenderFeatureParameterType::Float};
    RenderFeatureParameterValue value{};
    RenderFeatureParameterValue min{};
    RenderFeatureParameterValue max{};
    float step{0.01f};
    bool read_only{false};
};

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;

    [[nodiscard]] virtual RenderFeatureInfo info() const noexcept = 0;
    [[nodiscard]] virtual std::vector<RenderFeatureParameterInfo> parameters() const
    {
        return {};
    }
    virtual bool setEnabled(bool enabled) noexcept
    {
        (void) enabled;
        return false;
    }
    virtual bool setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept
    {
        (void) name;
        (void) value;
        return false;
    }
    virtual bool registerPasses(RenderFlowBuilder& builder) = 0;
    virtual void prepareFrame(const RenderWorld& world,
                              const SceneRenderContext& scene_context,
                              RenderPassBlackboard& blackboard) = 0;
    virtual void shutdown() = 0;
};

} // namespace luna::render_flow
