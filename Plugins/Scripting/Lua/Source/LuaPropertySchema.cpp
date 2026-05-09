#include "LuaPropertySchema.h"

#include "Bindings/LuaSharedBindings.h"

#include <glm/vec3.hpp>
#include <sol/sol.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct ParsedLuaPropertyOption {
    std::string label;
    int32_t int_value{0};
    std::string string_value;
};

struct ParsedLuaPropertySchema {
    std::string name;
    std::string display_name;
    std::string description;
    std::string category;
    LunaScriptPropertyType type{LunaScriptPropertyType_Float};
    bool bool_value{false};
    int32_t int_value{0};
    float float_value{0.0f};
    std::string string_value;
    LunaScriptVec3 vec3_value{};
    uint64_t entity_value{0};
    uint64_t asset_value{0};
    bool has_min_value{false};
    bool has_max_value{false};
    bool has_step_value{false};
    float min_value{0.0f};
    float max_value{0.0f};
    float step_value{0.0f};
    std::string asset_type;
    std::string entity_filter;
    std::vector<ParsedLuaPropertyOption> options;
};

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

void hostLog(const LunaScriptHostApi* host_api, LunaScriptHostLogLevel level, const std::string& message)
{
    if (host_api != nullptr && host_api->log != nullptr) {
        host_api->log(host_api->user_data, level, message.c_str());
    }
}

std::string safeString(const char* value)
{
    return value != nullptr ? value : "";
}

bool readPropertyType(const sol::table& property_table, LunaScriptPropertyType& out_type)
{
    sol::object type_object = property_table["type"];
    if (!type_object.valid() || !type_object.is<std::string>()) {
        return false;
    }

    const std::string type = toLower(type_object.as<std::string>());
    if (type == "bool" || type == "boolean") {
        out_type = LunaScriptPropertyType_Bool;
        return true;
    }
    if (type == "int" || type == "integer") {
        out_type = LunaScriptPropertyType_Int;
        return true;
    }
    if (type == "float" || type == "number") {
        out_type = LunaScriptPropertyType_Float;
        return true;
    }
    if (type == "string") {
        out_type = LunaScriptPropertyType_String;
        return true;
    }
    if (type == "vec3" || type == "vector3") {
        out_type = LunaScriptPropertyType_Vec3;
        return true;
    }
    if (type == "entity") {
        out_type = LunaScriptPropertyType_Entity;
        return true;
    }
    if (type == "asset") {
        out_type = LunaScriptPropertyType_Asset;
        return true;
    }

    return false;
}

LunaScriptVec3 readVec3Default(const sol::object& default_object)
{
    if (default_object.is<glm::vec3>()) {
        const glm::vec3 value = default_object.as<glm::vec3>();
        return LunaScriptVec3{value.x, value.y, value.z};
    }

    if (!default_object.is<sol::table>()) {
        return {};
    }

    sol::table table = default_object.as<sol::table>();
    const sol::object x = table["x"];
    const sol::object y = table["y"];
    const sol::object z = table["z"];
    if (x.is<float>() || x.is<double>() || x.is<int>()) {
        return LunaScriptVec3{
            table.get_or("x", 0.0f),
            table.get_or("y", 0.0f),
            table.get_or("z", 0.0f),
        };
    }

    return LunaScriptVec3{
        table.get_or(1, 0.0f),
        table.get_or(2, 0.0f),
        table.get_or(3, 0.0f),
    };
}

void readDefaultValue(ParsedLuaPropertySchema& property, const sol::object& default_object)
{
    if (!default_object.valid() || default_object == sol::nil) {
        return;
    }

    switch (property.type) {
        case LunaScriptPropertyType_Bool:
            if (default_object.is<bool>()) {
                property.bool_value = default_object.as<bool>();
            }
            break;
        case LunaScriptPropertyType_Int:
            if (default_object.is<int>()) {
                property.int_value = default_object.as<int>();
            }
            break;
        case LunaScriptPropertyType_Float:
            if (default_object.is<float>() || default_object.is<double>() || default_object.is<int>()) {
                property.float_value = default_object.as<float>();
            }
            break;
        case LunaScriptPropertyType_String:
            if (default_object.is<std::string>()) {
                property.string_value = default_object.as<std::string>();
            }
            break;
        case LunaScriptPropertyType_Vec3:
            property.vec3_value = readVec3Default(default_object);
            break;
        case LunaScriptPropertyType_Entity:
            if (default_object.is<uint64_t>()) {
                property.entity_value = default_object.as<uint64_t>();
            }
            break;
        case LunaScriptPropertyType_Asset:
            if (default_object.is<uint64_t>()) {
                property.asset_value = default_object.as<uint64_t>();
            }
            break;
    }
}

std::string readOptionalString(const sol::table& table, std::string_view key)
{
    sol::object value = table[std::string(key)];
    return value.is<std::string>() ? value.as<std::string>() : std::string{};
}

bool readOptionalNumber(const sol::table& table, std::string_view key, float& out_value)
{
    sol::object value = table[std::string(key)];
    if (!(value.is<float>() || value.is<double>() || value.is<int>())) {
        return false;
    }

    out_value = value.as<float>();
    return true;
}

std::string formatOptionLabel(const sol::object& value)
{
    if (value.is<std::string>()) {
        return value.as<std::string>();
    }
    if (value.is<int>()) {
        return std::to_string(value.as<int>());
    }
    if (value.is<float>() || value.is<double>()) {
        return std::to_string(value.as<float>());
    }
    if (value.is<bool>()) {
        return value.as<bool>() ? "true" : "false";
    }

    return {};
}

bool readOptionValue(LunaScriptPropertyType property_type,
                     const sol::object& value,
                     ParsedLuaPropertyOption& out_option)
{
    switch (property_type) {
        case LunaScriptPropertyType_Int:
            if (value.is<int>()) {
                out_option.int_value = value.as<int>();
                return true;
            }
            if (value.is<float>() || value.is<double>()) {
                out_option.int_value = static_cast<int32_t>(value.as<float>());
                return true;
            }
            return false;
        case LunaScriptPropertyType_String:
            if (value.is<std::string>()) {
                out_option.string_value = value.as<std::string>();
                return true;
            }
            return false;
        default:
            return false;
    }
}

std::string readOptionLabel(const sol::table& option_table)
{
    std::string label = readOptionalString(option_table, "label");
    if (label.empty()) {
        label = readOptionalString(option_table, "name");
    }
    if (label.empty()) {
        label = readOptionalString(option_table, "display_name");
    }
    if (label.empty()) {
        label = readOptionalString(option_table, "displayName");
    }
    return label;
}

void appendParsedOption(LunaScriptPropertyType property_type,
                        const sol::object& label_object,
                        const sol::object& value_object,
                        std::vector<ParsedLuaPropertyOption>& options)
{
    ParsedLuaPropertyOption option{};
    if (!readOptionValue(property_type, value_object, option)) {
        return;
    }

    option.label = label_object.is<int>() ? formatOptionLabel(value_object) : formatOptionLabel(label_object);
    if (option.label.empty()) {
        option.label = property_type == LunaScriptPropertyType_String ? option.string_value : std::to_string(option.int_value);
    }
    options.push_back(std::move(option));
}

void readOptions(const sol::table& property_table, ParsedLuaPropertySchema& property)
{
    sol::object options_object = property_table["options"];
    if (!options_object.is<sol::table>()) {
        return;
    }

    const sol::table options_table = options_object.as<sol::table>();
    for (const auto& pair : options_table) {
        if (pair.second.is<sol::table>()) {
            const sol::table option_table = pair.second.as<sol::table>();
            sol::object value_object = option_table["value"];
            if (!value_object.valid() || value_object == sol::nil) {
                value_object = option_table[1];
            }
            if (!value_object.valid() || value_object == sol::nil) {
                continue;
            }

            ParsedLuaPropertyOption option{};
            if (!readOptionValue(property.type, value_object, option)) {
                continue;
            }

            option.label = readOptionLabel(option_table);
            if (option.label.empty()) {
                option.label = formatOptionLabel(value_object);
            }
            if (option.label.empty()) {
                option.label =
                    property.type == LunaScriptPropertyType_String ? option.string_value : std::to_string(option.int_value);
            }
            property.options.push_back(std::move(option));
            continue;
        }

        appendParsedOption(property.type, pair.first, pair.second, property.options);
    }
}

sol::table resolvePrototypeTable(sol::state& lua_state,
                                 const LunaScriptSchemaRequest& request,
                                 const sol::environment& environment,
                                 const sol::protected_function_result& execute_result)
{
    sol::object returned_object = execute_result.return_count() > 0
                                      ? execute_result.get<sol::object>()
                                      : sol::make_object(lua_state, sol::nil);
    if (returned_object.is<sol::table>()) {
        return returned_object.as<sol::table>();
    }

    const std::string type_name = safeString(request.type_name);
    if (!type_name.empty()) {
        sol::object named_prototype = environment[type_name];
        if (named_prototype.is<sol::table>()) {
            return named_prototype.as<sol::table>();
        }
    }

    return sol::table{};
}

bool parsePropertySchema(const std::string& property_name,
                         const sol::object& property_object,
                         ParsedLuaPropertySchema& out_property)
{
    if (property_name.empty() || !property_object.is<sol::table>()) {
        return false;
    }

    sol::table property_table = property_object.as<sol::table>();
    out_property.name = property_name;
    out_property.display_name = readOptionalString(property_table, "display_name");
    if (out_property.display_name.empty()) {
        out_property.display_name = readOptionalString(property_table, "displayName");
    }
    out_property.description = readOptionalString(property_table, "description");
    out_property.category = readOptionalString(property_table, "category");
    if (out_property.category.empty()) {
        out_property.category = readOptionalString(property_table, "group");
    }

    if (!readPropertyType(property_table, out_property.type)) {
        return false;
    }

    readDefaultValue(out_property, property_table["default"]);
    out_property.has_min_value = readOptionalNumber(property_table, "min", out_property.min_value);
    out_property.has_max_value = readOptionalNumber(property_table, "max", out_property.max_value);
    out_property.has_step_value = readOptionalNumber(property_table, "step", out_property.step_value);
    out_property.asset_type = readOptionalString(property_table, "asset_type");
    if (out_property.asset_type.empty()) {
        out_property.asset_type = readOptionalString(property_table, "assetType");
    }
    out_property.entity_filter = readOptionalString(property_table, "entity_filter");
    if (out_property.entity_filter.empty()) {
        out_property.entity_filter = readOptionalString(property_table, "entityFilter");
    }
    readOptions(property_table, out_property);
    return true;
}

std::vector<ParsedLuaPropertySchema> parsePropertiesTable(const sol::table& properties_table)
{
    std::vector<ParsedLuaPropertySchema> properties;
    for (const auto& pair : properties_table) {
        if (!pair.first.is<std::string>()) {
            continue;
        }

        ParsedLuaPropertySchema property{};
        if (parsePropertySchema(pair.first.as<std::string>(), pair.second, property)) {
            properties.push_back(std::move(property));
        }
    }

    return properties;
}

LunaScriptPropertySchemaDesc toDesc(const ParsedLuaPropertySchema& property,
                                    std::vector<LunaScriptPropertyOptionDesc>& option_descs)
{
    option_descs.clear();
    option_descs.reserve(property.options.size());
    for (const ParsedLuaPropertyOption& option : property.options) {
        LunaScriptPropertyOptionDesc desc{};
        desc.label = option.label.c_str();
        desc.int_value = option.int_value;
        desc.string_value = option.string_value.c_str();
        option_descs.push_back(desc);
    }

    LunaScriptPropertySchemaDesc desc{};
    desc.name = property.name.c_str();
    desc.display_name = property.display_name.c_str();
    desc.description = property.description.c_str();
    desc.type = property.type;
    desc.default_bool_value = property.bool_value ? 1 : 0;
    desc.default_int_value = property.int_value;
    desc.default_float_value = property.float_value;
    desc.default_string_value = property.string_value.c_str();
    desc.default_vec3_value = property.vec3_value;
    desc.default_entity_value = property.entity_value;
    desc.default_asset_value = property.asset_value;
    desc.category = property.category.c_str();
    if (property.has_min_value) {
        desc.flags |= static_cast<uint32_t>(LunaScriptPropertySchemaFlag_HasMin);
    }
    if (property.has_max_value) {
        desc.flags |= static_cast<uint32_t>(LunaScriptPropertySchemaFlag_HasMax);
    }
    if (property.has_step_value) {
        desc.flags |= static_cast<uint32_t>(LunaScriptPropertySchemaFlag_HasStep);
    }
    desc.min_value = property.min_value;
    desc.max_value = property.max_value;
    desc.step_value = property.step_value;
    desc.asset_type = property.asset_type.c_str();
    desc.entity_filter = property.entity_filter.c_str();
    desc.options = option_descs.data();
    desc.option_count = option_descs.size();
    return desc;
}

} // namespace

namespace lua_plugin {

int enumerateLuaPropertySchema(const LunaScriptHostApi* host_api,
                               const LunaScriptSchemaRequest* request,
                               void* user_data,
                               LunaScriptEnumeratePropertySchemaFn enumerate_fn)
{
    if (request == nullptr || enumerate_fn == nullptr) {
        return 0;
    }

    const char* source = request->source != nullptr ? request->source : "";
    if (source[0] == '\0') {
        return 1;
    }

    sol::state lua_state;
    lua_state.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
    bindLuaSchemaInspectionTypes(lua_state);

    sol::environment environment(lua_state, sol::create, lua_state.globals());
    sol::load_result load_result = lua_state.load(source, safeString(request->asset_name));
    if (!load_result.valid()) {
        sol::error error = load_result;
        hostLog(host_api, LunaScriptHostLogLevel_Warn, "Lua schema load failed: " + std::string(error.what()));
        return 1;
    }

    sol::protected_function chunk = load_result;
    sol::set_environment(environment, chunk);
    sol::protected_function_result execute_result = chunk();
    if (!execute_result.valid()) {
        sol::error error = execute_result;
        hostLog(host_api, LunaScriptHostLogLevel_Warn, "Lua schema initialization failed: " + std::string(error.what()));
        return 1;
    }

    sol::table prototype = resolvePrototypeTable(lua_state, *request, environment, execute_result);
    if (!prototype.valid()) {
        return 1;
    }

    sol::object properties_object = prototype["Properties"];
    if (!properties_object.is<sol::table>()) {
        return 1;
    }

    std::vector<ParsedLuaPropertySchema> properties = parsePropertiesTable(properties_object.as<sol::table>());
    std::vector<LunaScriptPropertyOptionDesc> option_descs;
    for (const ParsedLuaPropertySchema& property : properties) {
        const LunaScriptPropertySchemaDesc desc = toDesc(property, option_descs);
        if (enumerate_fn(user_data, &desc) == 0) {
            return 0;
        }
    }

    return 1;
}

} // namespace lua_plugin
