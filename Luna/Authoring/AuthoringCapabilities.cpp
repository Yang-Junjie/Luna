#include "AuthoringCapabilities.h"

#include <nlohmann/json.hpp>

#include <ostream>
#include <string>
#include <utility>

namespace luna::authoring {
namespace {

using Json = nlohmann::ordered_json;

const char* parameterTypeName(AuthoringCapabilityParameterType type)
{
    switch (type) {
        case AuthoringCapabilityParameterType::String:
            return "string";
        case AuthoringCapabilityParameterType::Number:
            return "number";
        case AuthoringCapabilityParameterType::Integer:
            return "integer";
        case AuthoringCapabilityParameterType::Boolean:
            return "boolean";
        case AuthoringCapabilityParameterType::Vec3:
            return "vec3";
    }

    return "string";
}

Json effectNames(AuthoringCommandEffect effects)
{
    Json result = Json::array();
    if (hasAuthoringCommandEffect(effects, AuthoringCommandEffect::ReadsScene)) {
        result.push_back("readsScene");
    }
    if (hasAuthoringCommandEffect(effects, AuthoringCommandEffect::MutatesScene)) {
        result.push_back("mutatesScene");
    }
    if (hasAuthoringCommandEffect(effects, AuthoringCommandEffect::ReadsFileSystem)) {
        result.push_back("readsFileSystem");
    }
    if (hasAuthoringCommandEffect(effects, AuthoringCommandEffect::WritesFileSystem)) {
        result.push_back("writesFileSystem");
    }
    return result;
}

Json jsonParameter(const AuthoringCapabilityParameter& parameter)
{
    Json result = Json::object();
    result["type"] = parameterTypeName(parameter.type);
    result["required"] = parameter.required;
    result["description"] = parameter.description;
    if (!parameter.enum_values.empty()) {
        Json enum_values = Json::array();
        for (const std::string& value : parameter.enum_values) {
            enum_values.push_back(value);
        }
        result["enum"] = std::move(enum_values);
    }
    return result;
}

Json jsonParameters(const std::vector<AuthoringCapabilityParameter>& parameters)
{
    Json result = Json::object();
    for (const AuthoringCapabilityParameter& parameter : parameters) {
        result[parameter.name] = jsonParameter(parameter);
    }
    return result;
}

Json parseExamplePlanJson(const std::string& plan_json)
{
    try {
        return Json::parse(plan_json);
    } catch (...) {
        return plan_json;
    }
}

Json jsonExamples(const std::vector<AuthoringCapabilityExample>& examples)
{
    Json result = Json::array();
    for (const AuthoringCapabilityExample& example : examples) {
        Json item = Json::object();
        item["description"] = example.description;
        item["command"] = parseExamplePlanJson(example.plan_json);
        result.push_back(std::move(item));
    }
    return result;
}

Json authoringCapabilityJson(const AuthoringCapability& capability)
{
    Json result = Json::object();
    result["op"] = capability.op;
    result["title"] = capability.title;
    result["description"] = capability.description;
    result["effects"] = effectNames(capability.effects);
    result["requiresConfirmation"] = capability.requires_confirmation;
    result["parameters"] = jsonParameters(capability.parameters);
    result["examples"] = jsonExamples(capability.examples);
    return result;
}

AuthoringCapability makeCapability(std::string op,
                                   std::string title,
                                   std::string description,
                                   AuthoringCommandEffect effects,
                                   std::vector<AuthoringCapabilityParameter> parameters,
                                   std::vector<AuthoringCapabilityExample> examples,
                                   bool requires_confirmation = false)
{
    return {
        .op = std::move(op),
        .title = std::move(title),
        .description = std::move(description),
        .effects = effects,
        .requires_confirmation = requires_confirmation,
        .parameters = std::move(parameters),
        .examples = std::move(examples),
    };
}

AuthoringCapabilityParameter param(std::string name,
                                   AuthoringCapabilityParameterType type,
                                   std::string description,
                                   bool required = true,
                                   std::vector<std::string> enum_values = {})
{
    return {
        .name = std::move(name),
        .type = type,
        .required = required,
        .description = std::move(description),
        .enum_values = std::move(enum_values),
    };
}

AuthoringCapabilityExample example(std::string description, std::string plan_json)
{
    return {
        .description = std::move(description),
        .plan_json = std::move(plan_json),
    };
}

} // namespace

std::vector<AuthoringCapability> defaultAuthoringCapabilities()
{
    using Type = AuthoringCapabilityParameterType;

    return {
        makeCapability("new",
                       "New Scene",
                       "Create a new empty scene.",
                       authoringCommandEffects(AuthoringCommandKind::NewScene),
                       {},
                       {example("Start a fresh scene.", R"({ "op": "new" })")}),
        makeCapability("open",
                       "Open Scene",
                       "Open a scene file from disk.",
                       authoringCommandEffects(AuthoringCommandKind::OpenScene),
                       {param("path", Type::String, "Scene file path to open.")},
                       {example("Open an existing scene.", R"({ "op": "open", "path": "Scenes/Level.lunascene" })")}),
        makeCapability("save",
                       "Save Scene",
                       "Save the current scene to disk.",
                       authoringCommandEffects(AuthoringCommandKind::SaveScene),
                       {param("path", Type::String, "Scene file path to write.")},
                       {example("Save the current scene.", R"({ "op": "save", "path": "Scenes/Generated.lunascene" })")},
                       true),
        makeCapability("entity",
                       "Create Entity",
                       "Create a named empty entity and bind it to an alias for later commands.",
                       authoringCommandEffects(AuthoringCommandKind::CreateEntity),
                       {
                           param("alias", Type::String, "Alias used by later commands."),
                           param("name", Type::String, "Entity display name."),
                       },
                       {example("Create a game controller entity.",
                                R"({ "op": "entity", "alias": "GameController", "name": "Game Controller" })")}),
        makeCapability("camera",
                       "Create Camera",
                       "Create a camera entity.",
                       authoringCommandEffects(AuthoringCommandKind::CreateCamera),
                       {param("alias", Type::String, "Alias used by later commands.")},
                       {example("Create a main camera.", R"({ "op": "camera", "alias": "MainCamera" })")}),
        makeCapability("directional-light",
                       "Create Directional Light",
                       "Create a directional light entity.",
                       authoringCommandEffects(AuthoringCommandKind::CreateDirectionalLight),
                       {param("alias", Type::String, "Alias used by later commands.")},
                       {example("Create a sun light.", R"({ "op": "directional-light", "alias": "Sun" })")}),
        makeCapability("point-light",
                       "Create Point Light",
                       "Create a point light entity.",
                       authoringCommandEffects(AuthoringCommandKind::CreatePointLight),
                       {param("alias", Type::String, "Alias used by later commands.")},
                       {example("Create a point light.", R"({ "op": "point-light", "alias": "KeyLight" })")}),
        makeCapability("spot-light",
                       "Create Spot Light",
                       "Create a spot light entity.",
                       authoringCommandEffects(AuthoringCommandKind::CreateSpotLight),
                       {param("alias", Type::String, "Alias used by later commands.")},
                       {example("Create a spot light.", R"({ "op": "spot-light", "alias": "SpotLight" })")}),
        makeCapability("primitive",
                       "Create Primitive",
                       "Create an entity using a builtin primitive mesh.",
                       authoringCommandEffects(AuthoringCommandKind::CreatePrimitive),
                       {
                           param("alias", Type::String, "Alias used by later commands."),
                           param("mesh",
                                 Type::String,
                                 "Builtin primitive mesh name.",
                                 true,
                                 {"Cube", "Sphere", "Plane", "Cylinder", "Cone"}),
                       },
                       {example("Create a cube entity.",
                                R"({ "op": "primitive", "alias": "Player", "mesh": "Cube" })")}),
        makeCapability("parent",
                       "Parent Entity",
                       "Parent one entity under another entity.",
                       authoringCommandEffects(AuthoringCommandKind::Parent),
                       {
                           param("child", Type::String, "Child entity alias or UUID."),
                           param("parent", Type::String, "Parent entity alias or UUID."),
                       },
                       {example("Parent a mesh under a root.",
                                R"({ "op": "parent", "child": "Body", "parent": "Root" })")}),
        makeCapability("unparent",
                       "Unparent Entity",
                       "Remove an entity from its current parent.",
                       authoringCommandEffects(AuthoringCommandKind::Unparent),
                       {param("child", Type::String, "Child entity alias or UUID.")},
                       {example("Detach an entity from its parent.", R"({ "op": "unparent", "child": "Body" })")}),
        makeCapability("name",
                       "Rename Entity",
                       "Rename an existing entity.",
                       authoringCommandEffects(AuthoringCommandKind::Rename),
                       {
                           param("entity", Type::String, "Entity alias or UUID."),
                           param("name", Type::String, "New entity display name."),
                       },
                       {example("Rename an entity.",
                                R"({ "op": "name", "entity": "Player", "name": "Player Character" })")}),
        makeCapability("transform",
                       "Set Transform",
                       "Set entity translation, rotation in degrees, and scale.",
                       authoringCommandEffects(AuthoringCommandKind::SetTransform),
                       {
                           param("entity", Type::String, "Entity alias or UUID."),
                           param("translation", Type::Vec3, "Position as [x, y, z]."),
                           param("rotationDeg", Type::Vec3, "Euler rotation in degrees as [x, y, z].", false),
                           param("scale", Type::Vec3, "Scale as [x, y, z].", false),
                       },
                       {example("Move and rotate an entity.",
                                R"({ "op": "transform", "entity": "Player", "translation": [0, 1, 0], "rotationDeg": [0, 45, 0], "scale": [1, 1, 1] })")}),
        makeCapability("light-intensity",
                       "Set Light Intensity",
                       "Set the intensity of a light component.",
                       authoringCommandEffects(AuthoringCommandKind::SetLightIntensity),
                       {
                           param("entity", Type::String, "Light entity alias or UUID."),
                           param("value", Type::Number, "Light intensity value."),
                       },
                       {example("Set a light intensity.",
                                R"({ "op": "light-intensity", "entity": "KeyLight", "value": 3.5 })")}),
        makeCapability("light-color",
                       "Set Light Color",
                       "Set the RGB color of a light component.",
                       authoringCommandEffects(AuthoringCommandKind::SetLightColor),
                       {
                           param("entity", Type::String, "Light entity alias or UUID."),
                           param("color", Type::Vec3, "RGB color as [r, g, b]."),
                       },
                       {example("Set a warm light color.",
                                R"({ "op": "light-color", "entity": "KeyLight", "color": [1, 0.92, 0.78] })")}),
        makeCapability("camera-perspective",
                       "Set Perspective Camera",
                       "Configure a camera for perspective projection.",
                       authoringCommandEffects(AuthoringCommandKind::SetCameraPerspective),
                       {
                           param("entity", Type::String, "Camera entity alias or UUID."),
                           param("fovDeg", Type::Number, "Vertical field of view in degrees."),
                           param("near", Type::Number, "Near clip plane."),
                           param("far", Type::Number, "Far clip plane."),
                       },
                       {example("Configure a perspective camera.",
                                R"({ "op": "camera-perspective", "entity": "MainCamera", "fovDeg": 60, "near": 0.1, "far": 500 })")}),
        makeCapability("camera-orthographic",
                       "Set Orthographic Camera",
                       "Configure a camera for orthographic projection.",
                       authoringCommandEffects(AuthoringCommandKind::SetCameraOrthographic),
                       {
                           param("entity", Type::String, "Camera entity alias or UUID."),
                           param("size", Type::Number, "Orthographic size."),
                           param("near", Type::Number, "Near clip plane."),
                           param("far", Type::Number, "Far clip plane."),
                       },
                       {example("Configure an orthographic camera.",
                                R"({ "op": "camera-orthographic", "entity": "MainCamera", "size": 12, "near": 0.1, "far": 500 })")}),
        makeCapability("inspect",
                       "Inspect Scene Data",
                       "Read scene, hierarchy, or entity details without mutating the scene.",
                       AuthoringCommandEffect::ReadsScene,
                       {
                           param("target",
                                 Type::String,
                                 "Inspection target.",
                                 true,
                                 {"scene", "entity", "hierarchy"}),
                           param("entity", Type::String, "Entity alias or UUID when target is entity.", false),
                       },
                       {
                           example("Inspect the scene.", R"({ "op": "inspect", "target": "scene" })"),
                           example("Inspect an entity.",
                                   R"({ "op": "inspect", "target": "entity", "entity": "Player" })"),
                       }),
        makeCapability("verify",
                       "Verify Scene Conditions",
                       "Check scene conditions and report structured verification results.",
                       AuthoringCommandEffect::ReadsScene,
                       {
                           param("check",
                                 Type::String,
                                 "Verification check name.",
                                 true,
                                 {"sceneSaved", "entityExists", "hasComponent", "entityCountAtLeast"}),
                           param("entity", Type::String, "Entity alias or UUID for entity checks.", false),
                           param("component", Type::String, "Component name for hasComponent.", false),
                           param("count", Type::Integer, "Minimum entity count for entityCountAtLeast.", false),
                       },
                       {
                           example("Verify an entity exists.",
                                   R"({ "op": "verify", "check": "entityExists", "entity": "Player" })"),
                           example("Verify a component exists.",
                                   R"({ "op": "verify", "check": "hasComponent", "entity": "Player", "component": "Mesh" })"),
                       }),
        makeCapability("summary",
                       "Print Summary",
                       "Print a human-readable scene summary.",
                       authoringCommandEffects(AuthoringCommandKind::Summary),
                       {},
                       {example("Request a summary.", R"({ "op": "summary" })")}),
        makeCapability("snapshot",
                       "Read Scene Snapshot",
                       "Read the current scene hierarchy and component overview without mutating the scene.",
                       authoringCommandEffects(AuthoringCommandKind::Snapshot),
                       {},
                       {example("Read a full scene snapshot.", R"({ "op": "snapshot" })")}),
    };
}

Json authoringCapabilitiesJson(const std::vector<AuthoringCapability>& capabilities)
{
    Json protocol = Json::object();
    protocol["name"] = kAuthoringProtocolName;
    protocol["version"] = kAuthoringProtocolVersion;

    Json capability_list = Json::array();
    for (const AuthoringCapability& capability : capabilities) {
        capability_list.push_back(authoringCapabilityJson(capability));
    }

    Json root = Json::object();
    root["protocol"] = std::move(protocol);
    root["capabilities"] = std::move(capability_list);
    return root;
}

Json defaultAuthoringCapabilitiesJson()
{
    return authoringCapabilitiesJson(defaultAuthoringCapabilities());
}

void writeAuthoringCapabilitiesJson(std::ostream& out, const std::vector<AuthoringCapability>& capabilities)
{
    out << authoringCapabilitiesJson(capabilities).dump(2, ' ', false, Json::error_handler_t::replace) << '\n';
}

void writeDefaultAuthoringCapabilitiesJson(std::ostream& out)
{
    writeAuthoringCapabilitiesJson(out, defaultAuthoringCapabilities());
}

} // namespace luna::authoring
