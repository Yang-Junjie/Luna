#pragma once

#include "Authoring/AuthoringJsonTypes.h"
#include "Authoring/AuthoringProtocol.h"

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace luna::authoring {

enum class AuthoringCapabilityParameterType : uint8_t {
    String,
    Number,
    Integer,
    Boolean,
    Vec3,
};

struct AuthoringCapabilityParameter {
    std::string name;
    AuthoringCapabilityParameterType type{AuthoringCapabilityParameterType::String};
    bool required{true};
    std::string description;
    std::vector<std::string> enum_values;
};

struct AuthoringCapabilityExample {
    std::string description;
    std::string plan_json;
};

struct AuthoringCapability {
    std::string op;
    std::string title;
    std::string description;
    AuthoringCommandEffect effects{AuthoringCommandEffect::None};
    bool requires_confirmation{false};
    std::vector<AuthoringCapabilityParameter> parameters;
    std::vector<AuthoringCapabilityExample> examples;
};

[[nodiscard]] std::vector<AuthoringCapability> defaultAuthoringCapabilities();

[[nodiscard]] Json authoringCapabilitiesJson(const std::vector<AuthoringCapability>& capabilities);
[[nodiscard]] Json defaultAuthoringCapabilitiesJson();

void writeAuthoringCapabilitiesJson(std::ostream& out,
                                    const std::vector<AuthoringCapability>& capabilities);

void writeDefaultAuthoringCapabilitiesJson(std::ostream& out);

} // namespace luna::authoring
