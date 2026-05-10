#pragma once

#include "Protocol/EditorProtocol.h"
#include "Protocol/RenderDataPlane.h"
#include "Protocol/ViewProtocol.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace luna::editor {
namespace view_protocol_detail {

using Json = nlohmann::ordered_json;

inline constexpr std::string_view kProtocolName = kEditorProtocolName;
inline constexpr uint32_t kProtocolVersion = kEditorProtocolVersion;

inline std::string nestedFieldName(std::string_view parent, std::string_view child)
{
    if (parent.empty()) {
        return std::string(child);
    }

    std::string result(parent);
    result.push_back('.');
    result.append(child);
    return result;
}

inline std::string indexedFieldName(std::string_view parent, size_t index)
{
    std::string result(parent);
    result.push_back('[');
    result.append(std::to_string(index));
    result.push_back(']');
    return result;
}

inline void appendError(std::vector<std::string>* errors, std::string message)
{
    if (errors != nullptr) {
        errors->push_back(std::move(message));
    }
}

inline bool validateObjectFields(const Json& node,
                                 std::string_view field_name,
                                 std::initializer_list<std::string_view> allowed_fields,
                                 std::vector<std::string>* errors)
{
    for (auto it = node.begin(); it != node.end(); ++it) {
        const std::string key = it.key();
        const auto allowed = std::find(allowed_fields.begin(), allowed_fields.end(), std::string_view(key));
        if (allowed != allowed_fields.end()) {
            continue;
        }

        appendError(errors, "Field '" + nestedFieldName(field_name, key) + "' is not supported.");
        return false;
    }

    return true;
}

inline bool requireObject(const Json& node, std::string_view field_name, std::vector<std::string>* errors)
{
    if (!node.is_object()) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be an object.");
        return false;
    }

    return true;
}

inline bool requireArray(const Json& node, std::string_view field_name, std::vector<std::string>* errors)
{
    if (!node.is_array()) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be an array.");
        return false;
    }

    return true;
}

inline bool readStringField(const Json& node,
                            const char* key,
                            std::string& value,
                            std::string_view field_name,
                            std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        appendError(errors, "Field '" + std::string(field_name) + "' is required.");
        return false;
    }

    if (!it->is_string()) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a string.");
        return false;
    }

    try {
        value = it->get<std::string>();
    } catch (const std::exception&) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a string.");
        return false;
    }

    return true;
}

inline bool readOptionalStringField(const Json& node,
                                    const char* key,
                                    std::string& value,
                                    std::string_view field_name,
                                    std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        return true;
    }

    return readStringField(node, key, value, field_name, errors);
}

inline bool readBoolField(const Json& node,
                          const char* key,
                          bool& value,
                          std::string_view field_name,
                          std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        appendError(errors, "Field '" + std::string(field_name) + "' is required.");
        return false;
    }

    if (!it->is_boolean()) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a boolean.");
        return false;
    }

    try {
        value = it->get<bool>();
    } catch (const std::exception&) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a boolean.");
        return false;
    }

    return true;
}

inline bool readOptionalBoolField(const Json& node,
                                  const char* key,
                                  bool& value,
                                  std::string_view field_name,
                                  std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        return true;
    }

    return readBoolField(node, key, value, field_name, errors);
}

inline bool readUint64Field(const Json& node,
                            const char* key,
                            uint64_t& value,
                            std::string_view field_name,
                            std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        appendError(errors, "Field '" + std::string(field_name) + "' is required.");
        return false;
    }

    if (!it->is_number_integer() && !it->is_number_unsigned()) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a non-negative integer.");
        return false;
    }

    try {
        value = it->get<uint64_t>();
    } catch (const std::exception&) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a non-negative integer.");
        return false;
    }

    return true;
}

inline bool readOptionalUint64Field(const Json& node,
                                    const char* key,
                                    uint64_t& value,
                                    std::string_view field_name,
                                    std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        return true;
    }

    return readUint64Field(node, key, value, field_name, errors);
}

inline bool readFloatField(const Json& node,
                           const char* key,
                           float& value,
                           std::string_view field_name,
                           std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        appendError(errors, "Field '" + std::string(field_name) + "' is required.");
        return false;
    }

    if (!it->is_number()) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a finite number.");
        return false;
    }

    try {
        const double number = it->get<double>();
        if (!std::isfinite(number)) {
            appendError(errors, "Field '" + std::string(field_name) + "' must be a finite number.");
            return false;
        }

        value = static_cast<float>(number);
    } catch (const std::exception&) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a finite number.");
        return false;
    }

    return true;
}

inline bool readOptionalFloatField(const Json& node,
                                   const char* key,
                                   float& value,
                                   std::string_view field_name,
                                   std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        return true;
    }

    return readFloatField(node, key, value, field_name, errors);
}

inline bool readVec3Field(const Json& node,
                          const char* key,
                          glm::vec3& value,
                          std::string_view field_name,
                          std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        appendError(errors, "Field '" + std::string(field_name) + "' is required.");
        return false;
    }

    if (!it->is_array() || it->size() != 3) {
        appendError(errors, "Field '" + std::string(field_name) + "' must be a 3-number array.");
        return false;
    }

    double values[3]{};
    for (size_t index = 0; index < 3; ++index) {
        const auto& element = (*it)[index];
        if (!element.is_number()) {
            appendError(errors,
                        "Field '" + indexedFieldName(field_name, index) + "' must be a finite number.");
            return false;
        }

        try {
            values[index] = element.get<double>();
        } catch (const std::exception&) {
            appendError(errors,
                        "Field '" + indexedFieldName(field_name, index) + "' must be a finite number.");
            return false;
        }

        if (!std::isfinite(values[index])) {
            appendError(errors,
                        "Field '" + indexedFieldName(field_name, index) + "' must be a finite number.");
            return false;
        }
    }

    value = {static_cast<float>(values[0]), static_cast<float>(values[1]), static_cast<float>(values[2])};
    return true;
}

inline bool readOptionalVec3Field(const Json& node,
                                  const char* key,
                                  glm::vec3& value,
                                  std::string_view field_name,
                                  std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        return true;
    }

    return readVec3Field(node, key, value, field_name, errors);
}

inline Json writeVec3(const glm::vec3& value)
{
    Json result = Json::array();
    result.push_back(value.x);
    result.push_back(value.y);
    result.push_back(value.z);
    return result;
}

inline Json writeProtocol()
{
    Json protocol = Json::object();
    protocol["name"] = std::string(kProtocolName);
    protocol["version"] = kProtocolVersion;
    return protocol;
}

inline std::optional<Camera::ProjectionType> cameraProjectionTypeFromName(std::string_view name) noexcept
{
    if (name == "perspective") {
        return Camera::ProjectionType::Perspective;
    }
    if (name == "orthographic") {
        return Camera::ProjectionType::Orthographic;
    }

    return std::nullopt;
}

inline std::string_view cameraProjectionTypeName(Camera::ProjectionType type) noexcept
{
    switch (type) {
        case Camera::ProjectionType::Perspective:
            return "perspective";
        case Camera::ProjectionType::Orthographic:
            return "orthographic";
    }

    return "unknown";
}

inline std::optional<EditorViewportKind> viewportKindFromName(std::string_view name) noexcept
{
    if (name == editorViewportKindName(EditorViewportKind::Scene)) {
        return EditorViewportKind::Scene;
    }
    if (name == editorViewportKindName(EditorViewportKind::Debug)) {
        return EditorViewportKind::Debug;
    }
    if (name == editorViewportKindName(EditorViewportKind::Preview)) {
        return EditorViewportKind::Preview;
    }
    if (name == editorViewportKindName(EditorViewportKind::Custom)) {
        return EditorViewportKind::Custom;
    }

    return std::nullopt;
}

inline std::optional<EditorTransformTool> transformToolFromName(std::string_view name) noexcept
{
    if (name == editorTransformToolName(EditorTransformTool::Translate)) {
        return EditorTransformTool::Translate;
    }
    if (name == editorTransformToolName(EditorTransformTool::Rotate)) {
        return EditorTransformTool::Rotate;
    }
    if (name == editorTransformToolName(EditorTransformTool::Scale)) {
        return EditorTransformTool::Scale;
    }

    return std::nullopt;
}

inline std::optional<EditorTransformSpace> transformSpaceFromName(std::string_view name) noexcept
{
    if (name == editorTransformSpaceName(EditorTransformSpace::Local)) {
        return EditorTransformSpace::Local;
    }
    if (name == editorTransformSpaceName(EditorTransformSpace::World)) {
        return EditorTransformSpace::World;
    }

    return std::nullopt;
}

inline std::optional<EditorRenderPlaneKind> renderPlaneKindFromName(std::string_view name) noexcept
{
    if (name == editorRenderPlaneKindName(EditorRenderPlaneKind::SceneViewport)) {
        return EditorRenderPlaneKind::SceneViewport;
    }
    if (name == editorRenderPlaneKindName(EditorRenderPlaneKind::DebugViewport)) {
        return EditorRenderPlaneKind::DebugViewport;
    }
    if (name == editorRenderPlaneKindName(EditorRenderPlaneKind::Preview)) {
        return EditorRenderPlaneKind::Preview;
    }
    if (name == editorRenderPlaneKindName(EditorRenderPlaneKind::Capture)) {
        return EditorRenderPlaneKind::Capture;
    }

    return std::nullopt;
}

inline std::optional<EditorRenderTransportKind> renderTransportKindFromName(std::string_view name) noexcept
{
    if (name == editorRenderTransportKindName(EditorRenderTransportKind::None)) {
        return EditorRenderTransportKind::None;
    }
    if (name == editorRenderTransportKindName(EditorRenderTransportKind::NativeSurface)) {
        return EditorRenderTransportKind::NativeSurface;
    }
    if (name == editorRenderTransportKindName(EditorRenderTransportKind::SharedTexture)) {
        return EditorRenderTransportKind::SharedTexture;
    }
    if (name == editorRenderTransportKindName(EditorRenderTransportKind::CpuImage)) {
        return EditorRenderTransportKind::CpuImage;
    }

    return std::nullopt;
}

inline std::optional<RenderDebugViewMode> renderDebugViewModeFromName(std::string_view name) noexcept
{
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::None)) {
        return RenderDebugViewMode::None;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::Velocity)) {
        return RenderDebugViewMode::Velocity;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::HistoryValidity)) {
        return RenderDebugViewMode::HistoryValidity;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::ShadowCascades)) {
        return RenderDebugViewMode::ShadowCascades;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BaseColor)) {
        return RenderDebugViewMode::BaseColor;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::Normal)) {
        return RenderDebugViewMode::Normal;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::Metallic)) {
        return RenderDebugViewMode::Metallic;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::Roughness)) {
        return RenderDebugViewMode::Roughness;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::DirectLighting)) {
        return RenderDebugViewMode::DirectLighting;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::SpecularIbl)) {
        return RenderDebugViewMode::SpecularIbl;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomInput)) {
        return RenderDebugViewMode::BloomInput;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomPrefilter)) {
        return RenderDebugViewMode::BloomPrefilter;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomMip0)) {
        return RenderDebugViewMode::BloomMip0;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomMip1)) {
        return RenderDebugViewMode::BloomMip1;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomMip2)) {
        return RenderDebugViewMode::BloomMip2;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomMip3)) {
        return RenderDebugViewMode::BloomMip3;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomMip4)) {
        return RenderDebugViewMode::BloomMip4;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomMip5)) {
        return RenderDebugViewMode::BloomMip5;
    }
    if (name == editorRenderDebugViewModeName(RenderDebugViewMode::BloomComposite)) {
        return RenderDebugViewMode::BloomComposite;
    }

    return std::nullopt;
}

inline std::string renderFormatName(luna::RHI::Format format)
{
    switch (format) {
        case luna::RHI::Format::R8_UNORM:
            return "R8_UNORM";
        case luna::RHI::Format::R8_SNORM:
            return "R8_SNORM";
        case luna::RHI::Format::R8_UINT:
            return "R8_UINT";
        case luna::RHI::Format::R8_SINT:
            return "R8_SINT";
        case luna::RHI::Format::RG8_UNORM:
            return "RG8_UNORM";
        case luna::RHI::Format::RG8_SNORM:
            return "RG8_SNORM";
        case luna::RHI::Format::RG8_UINT:
            return "RG8_UINT";
        case luna::RHI::Format::RG8_SINT:
            return "RG8_SINT";
        case luna::RHI::Format::RGBA8_UNORM:
            return "RGBA8_UNORM";
        case luna::RHI::Format::RGBA8_SNORM:
            return "RGBA8_SNORM";
        case luna::RHI::Format::RGBA8_UINT:
            return "RGBA8_UINT";
        case luna::RHI::Format::RGBA8_SINT:
            return "RGBA8_SINT";
        case luna::RHI::Format::RGBA8_SRGB:
            return "RGBA8_SRGB";
        case luna::RHI::Format::BGRA8_UNORM:
            return "BGRA8_UNORM";
        case luna::RHI::Format::BGRA8_SRGB:
            return "BGRA8_SRGB";
        case luna::RHI::Format::R16_UNORM:
            return "R16_UNORM";
        case luna::RHI::Format::R16_SNORM:
            return "R16_SNORM";
        case luna::RHI::Format::R16_UINT:
            return "R16_UINT";
        case luna::RHI::Format::R16_SINT:
            return "R16_SINT";
        case luna::RHI::Format::R16_FLOAT:
            return "R16_FLOAT";
        case luna::RHI::Format::RG16_UNORM:
            return "RG16_UNORM";
        case luna::RHI::Format::RG16_SNORM:
            return "RG16_SNORM";
        case luna::RHI::Format::RG16_UINT:
            return "RG16_UINT";
        case luna::RHI::Format::RG16_SINT:
            return "RG16_SINT";
        case luna::RHI::Format::RG16_FLOAT:
            return "RG16_FLOAT";
        case luna::RHI::Format::RGBA16_UNORM:
            return "RGBA16_UNORM";
        case luna::RHI::Format::RGBA16_SNORM:
            return "RGBA16_SNORM";
        case luna::RHI::Format::RGBA16_UINT:
            return "RGBA16_UINT";
        case luna::RHI::Format::RGBA16_SINT:
            return "RGBA16_SINT";
        case luna::RHI::Format::RGBA16_FLOAT:
            return "RGBA16_FLOAT";
        case luna::RHI::Format::R32_UINT:
            return "R32_UINT";
        case luna::RHI::Format::R32_SINT:
            return "R32_SINT";
        case luna::RHI::Format::R32_FLOAT:
            return "R32_FLOAT";
        case luna::RHI::Format::RG32_UINT:
            return "RG32_UINT";
        case luna::RHI::Format::RG32_SINT:
            return "RG32_SINT";
        case luna::RHI::Format::RG32_FLOAT:
            return "RG32_FLOAT";
        case luna::RHI::Format::RGB32_UINT:
            return "RGB32_UINT";
        case luna::RHI::Format::RGB32_SINT:
            return "RGB32_SINT";
        case luna::RHI::Format::RGB32_FLOAT:
            return "RGB32_FLOAT";
        case luna::RHI::Format::RGBA32_UINT:
            return "RGBA32_UINT";
        case luna::RHI::Format::RGBA32_SINT:
            return "RGBA32_SINT";
        case luna::RHI::Format::RGBA32_FLOAT:
            return "RGBA32_FLOAT";
        case luna::RHI::Format::RGB10A2_UNORM:
            return "RGB10A2_UNORM";
        case luna::RHI::Format::RGB10A2_UINT:
            return "RGB10A2_UINT";
        case luna::RHI::Format::RG11B10_FLOAT:
            return "RG11B10_FLOAT";
        case luna::RHI::Format::D16_UNORM:
            return "D16_UNORM";
        case luna::RHI::Format::D24_UNORM_S8_UINT:
            return "D24_UNORM_S8_UINT";
        case luna::RHI::Format::D32_FLOAT:
            return "D32_FLOAT";
        case luna::RHI::Format::D32_FLOAT_S8_UINT:
            return "D32_FLOAT_S8_UINT";
        case luna::RHI::Format::S8_UINT:
            return "S8_UINT";
        case luna::RHI::Format::BC1_RGB_UNORM:
            return "BC1_RGB_UNORM";
        case luna::RHI::Format::BC1_RGB_SRGB:
            return "BC1_RGB_SRGB";
        case luna::RHI::Format::BC1_RGBA_UNORM:
            return "BC1_RGBA_UNORM";
        case luna::RHI::Format::BC1_RGBA_SRGB:
            return "BC1_RGBA_SRGB";
        case luna::RHI::Format::BC2_UNORM:
            return "BC2_UNORM";
        case luna::RHI::Format::BC2_SRGB:
            return "BC2_SRGB";
        case luna::RHI::Format::BC3_UNORM:
            return "BC3_UNORM";
        case luna::RHI::Format::BC3_SRGB:
            return "BC3_SRGB";
        case luna::RHI::Format::BC4_UNORM:
            return "BC4_UNORM";
        case luna::RHI::Format::BC4_SNORM:
            return "BC4_SNORM";
        case luna::RHI::Format::BC5_UNORM:
            return "BC5_UNORM";
        case luna::RHI::Format::BC5_SNORM:
            return "BC5_SNORM";
        case luna::RHI::Format::BC6H_UFLOAT:
            return "BC6H_UFLOAT";
        case luna::RHI::Format::BC6H_SFLOAT:
            return "BC6H_SFLOAT";
        case luna::RHI::Format::BC7_UNORM:
            return "BC7_UNORM";
        case luna::RHI::Format::BC7_SRGB:
            return "BC7_SRGB";
        case luna::RHI::Format::UNDEFINED:
            return "UNDEFINED";
    }

    return "Unknown";
}

inline std::optional<luna::RHI::Format> renderFormatFromName(std::string_view name) noexcept
{
#define LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(value) \
    if (name == #value) {                             \
        return luna::RHI::Format::value;              \
    }

    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R8_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R8_SNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R8_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R8_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG8_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG8_SNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG8_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG8_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA8_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA8_SNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA8_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA8_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA8_SRGB)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BGRA8_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BGRA8_SRGB)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R16_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R16_SNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R16_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R16_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R16_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG16_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG16_SNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG16_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG16_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG16_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA16_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA16_SNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA16_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA16_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA16_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R32_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R32_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(R32_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG32_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG32_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG32_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGB32_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGB32_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGB32_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA32_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA32_SINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGBA32_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGB10A2_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RGB10A2_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(RG11B10_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(D16_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(D24_UNORM_S8_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(D32_FLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(D32_FLOAT_S8_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(S8_UINT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC1_RGB_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC1_RGB_SRGB)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC1_RGBA_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC1_RGBA_SRGB)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC2_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC2_SRGB)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC3_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC3_SRGB)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC4_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC4_SNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC5_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC5_SNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC6H_UFLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC6H_SFLOAT)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC7_UNORM)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(BC7_SRGB)
    LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE(UNDEFINED)

#undef LUNA_EDITOR_VIEW_PROTOCOL_FORMAT_CASE

    return std::nullopt;
}

inline Json renderPlaneDescriptorJson(const EditorRenderPlaneDescriptor& descriptor)
{
    Json result = Json::object();
    result["planeId"] = descriptor.plane_id;
    result["viewportId"] = descriptor.viewport_id;
    result["kind"] = std::string(editorRenderPlaneKindName(descriptor.kind));
    result["transport"] = std::string(editorRenderTransportKindName(descriptor.transport));
    result["format"] = renderFormatName(descriptor.format);
    result["width"] = descriptor.width;
    result["height"] = descriptor.height;
    result["yFlip"] = descriptor.y_flip;
    result["presentable"] = descriptor.presentable;
    result["generation"] = descriptor.generation;
    result["bindingToken"] = descriptor.binding_token;
    result["label"] = descriptor.label;
    return result;
}

inline bool renderPlaneDescriptorFromJson(const Json& node,
                                          EditorRenderPlaneDescriptor& descriptor,
                                          std::vector<std::string>* errors,
                                          std::string_view field_name)
{
    if (!requireObject(node, field_name, errors)) {
        return false;
    }

    if (!validateObjectFields(node,
                              field_name,
                              {"planeId",
                               "viewportId",
                               "kind",
                               "transport",
                               "format",
                               "width",
                               "height",
                               "yFlip",
                               "presentable",
                               "generation",
                               "bindingToken",
                               "label"},
                              errors)) {
        return false;
    }

    uint64_t plane_id = 0;
    uint64_t viewport_id = 0;
    std::string kind_name;
    std::string transport_name;
    std::string format_name;
    uint64_t width = 0;
    uint64_t height = 0;
    bool y_flip = false;
    bool presentable = false;
    uint64_t generation = 0;
    std::string binding_token;
    std::string label;

    if (!readUint64Field(node, "planeId", plane_id, nestedFieldName(field_name, "planeId"), errors) ||
        !readUint64Field(node, "viewportId", viewport_id, nestedFieldName(field_name, "viewportId"), errors) ||
        !readStringField(node, "kind", kind_name, nestedFieldName(field_name, "kind"), errors) ||
        !readStringField(node, "transport", transport_name, nestedFieldName(field_name, "transport"), errors) ||
        !readStringField(node, "format", format_name, nestedFieldName(field_name, "format"), errors) ||
        !readUint64Field(node, "width", width, nestedFieldName(field_name, "width"), errors) ||
        !readUint64Field(node, "height", height, nestedFieldName(field_name, "height"), errors) ||
        !readBoolField(node, "yFlip", y_flip, nestedFieldName(field_name, "yFlip"), errors) ||
        !readBoolField(node, "presentable", presentable, nestedFieldName(field_name, "presentable"), errors) ||
        !readUint64Field(node, "generation", generation, nestedFieldName(field_name, "generation"), errors) ||
        !readStringField(node, "bindingToken", binding_token, nestedFieldName(field_name, "bindingToken"), errors) ||
        !readStringField(node, "label", label, nestedFieldName(field_name, "label"), errors)) {
        return false;
    }

    const auto parsed_kind = renderPlaneKindFromName(kind_name);
    if (!parsed_kind.has_value()) {
        appendError(errors, "Field '" + nestedFieldName(field_name, "kind") + "' has unsupported value '" + kind_name + "'.");
        return false;
    }

    const auto parsed_transport = renderTransportKindFromName(transport_name);
    if (!parsed_transport.has_value()) {
        appendError(errors,
                    "Field '" + nestedFieldName(field_name, "transport") + "' has unsupported value '" +
                        transport_name + "'.");
        return false;
    }

    const auto parsed_format = renderFormatFromName(format_name);
    if (!parsed_format.has_value()) {
        appendError(errors,
                    "Field '" + nestedFieldName(field_name, "format") + "' has unsupported value '" + format_name + "'.");
        return false;
    }

    descriptor.plane_id = static_cast<EditorRenderPlaneId>(plane_id);
    descriptor.viewport_id = static_cast<EditorViewportId>(viewport_id);
    descriptor.kind = *parsed_kind;
    descriptor.transport = *parsed_transport;
    descriptor.format = *parsed_format;
    descriptor.width = static_cast<uint32_t>(width);
    descriptor.height = static_cast<uint32_t>(height);
    descriptor.y_flip = y_flip;
    descriptor.presentable = presentable;
    descriptor.generation = generation;
    descriptor.binding_token = std::move(binding_token);
    descriptor.label = std::move(label);
    return true;
}

inline Json renderFrameInfoJson(const EditorRenderFrameInfo& frame)
{
    Json result = Json::object();
    result["frameId"] = frame.frame_id;
    result["planeId"] = frame.plane_id;
    result["sequence"] = frame.sequence;
    result["timestampNs"] = frame.timestamp_ns;
    result["width"] = frame.width;
    result["height"] = frame.height;
    result["ready"] = frame.ready;
    return result;
}

inline bool renderFrameInfoFromJson(const Json& node,
                                    EditorRenderFrameInfo& frame,
                                    std::vector<std::string>* errors,
                                    std::string_view field_name)
{
    if (!requireObject(node, field_name, errors)) {
        return false;
    }

    if (!validateObjectFields(node,
                              field_name,
                              {"frameId", "planeId", "sequence", "timestampNs", "width", "height", "ready"},
                              errors)) {
        return false;
    }

    uint64_t frame_id = 0;
    uint64_t plane_id = 0;
    uint64_t sequence = 0;
    uint64_t timestamp_ns = 0;
    uint64_t width = 0;
    uint64_t height = 0;
    bool ready = false;

    if (!readUint64Field(node, "frameId", frame_id, nestedFieldName(field_name, "frameId"), errors) ||
        !readUint64Field(node, "planeId", plane_id, nestedFieldName(field_name, "planeId"), errors) ||
        !readUint64Field(node, "sequence", sequence, nestedFieldName(field_name, "sequence"), errors) ||
        !readUint64Field(node, "timestampNs", timestamp_ns, nestedFieldName(field_name, "timestampNs"), errors) ||
        !readUint64Field(node, "width", width, nestedFieldName(field_name, "width"), errors) ||
        !readUint64Field(node, "height", height, nestedFieldName(field_name, "height"), errors) ||
        !readBoolField(node, "ready", ready, nestedFieldName(field_name, "ready"), errors)) {
        return false;
    }

    frame.frame_id = static_cast<EditorFrameId>(frame_id);
    frame.plane_id = static_cast<EditorRenderPlaneId>(plane_id);
    frame.sequence = sequence;
    frame.timestamp_ns = timestamp_ns;
    frame.width = static_cast<uint32_t>(width);
    frame.height = static_cast<uint32_t>(height);
    frame.ready = ready;
    return true;
}

inline Json renderPlaneStateJson(const EditorRenderPlaneState& state)
{
    Json result = Json::object();
    result["descriptor"] = renderPlaneDescriptorJson(state.descriptor);
    result["frame"] = renderFrameInfoJson(state.frame);
    result["active"] = state.active;
    return result;
}

inline bool renderPlaneStateFromJson(const Json& node,
                                     EditorRenderPlaneState& state,
                                     std::vector<std::string>* errors,
                                     std::string_view field_name)
{
    if (!requireObject(node, field_name, errors)) {
        return false;
    }

    if (!validateObjectFields(node, field_name, {"descriptor", "frame", "active"}, errors)) {
        return false;
    }

    const auto descriptor = node.find("descriptor");
    const auto frame = node.find("frame");
    const auto active = node.find("active");
    if (descriptor == node.end() || frame == node.end() || active == node.end()) {
        appendError(errors, "Field '" + std::string(field_name) + "' is missing required properties.");
        return false;
    }

    if (!renderPlaneDescriptorFromJson(*descriptor, state.descriptor, errors, nestedFieldName(field_name, "descriptor")) ||
        !renderFrameInfoFromJson(*frame, state.frame, errors, nestedFieldName(field_name, "frame")) ||
        !readBoolField(node, "active", state.active, nestedFieldName(field_name, "active"), errors)) {
        return false;
    }

    return true;
}

inline Json cameraStateJson(const EditorViewportCameraState& camera)
{
    Json result = Json::object();
    result["projection"] = std::string(cameraProjectionTypeName(camera.projection_type));
    result["position"] = writeVec3(camera.position);
    result["orientationEulerRadians"] = writeVec3(camera.orientation_euler_radians);
    result["perspectiveVerticalFovRadians"] = camera.perspective_vertical_fov_radians;
    result["perspectiveNear"] = camera.perspective_near;
    result["perspectiveFar"] = camera.perspective_far;
    result["orthographicSize"] = camera.orthographic_size;
    result["orthographicNear"] = camera.orthographic_near;
    result["orthographicFar"] = camera.orthographic_far;
    return result;
}

inline bool cameraStateFromJson(const Json& node,
                                EditorViewportCameraState& camera,
                                std::vector<std::string>* errors,
                                std::string_view field_name)
{
    if (!requireObject(node, field_name, errors)) {
        return false;
    }

    if (!validateObjectFields(node,
                              field_name,
                              {"projection",
                               "position",
                               "orientationEulerRadians",
                               "perspectiveVerticalFovRadians",
                               "perspectiveNear",
                               "perspectiveFar",
                               "orthographicSize",
                               "orthographicNear",
                               "orthographicFar"},
                              errors)) {
        return false;
    }

    std::string projection;
    glm::vec3 position{};
    glm::vec3 orientation{};
    float perspective_vertical_fov_radians = 0.0f;
    float perspective_near = 0.0f;
    float perspective_far = 0.0f;
    float orthographic_size = 0.0f;
    float orthographic_near = 0.0f;
    float orthographic_far = 0.0f;

    if (!readStringField(node, "projection", projection, nestedFieldName(field_name, "projection"), errors) ||
        !readVec3Field(node, "position", position, nestedFieldName(field_name, "position"), errors) ||
        !readVec3Field(node,
                       "orientationEulerRadians",
                       orientation,
                       nestedFieldName(field_name, "orientationEulerRadians"),
                       errors) ||
        !readFloatField(node,
                        "perspectiveVerticalFovRadians",
                        perspective_vertical_fov_radians,
                        nestedFieldName(field_name, "perspectiveVerticalFovRadians"),
                        errors) ||
        !readFloatField(node, "perspectiveNear", perspective_near, nestedFieldName(field_name, "perspectiveNear"), errors) ||
        !readFloatField(node, "perspectiveFar", perspective_far, nestedFieldName(field_name, "perspectiveFar"), errors) ||
        !readFloatField(node, "orthographicSize", orthographic_size, nestedFieldName(field_name, "orthographicSize"), errors) ||
        !readFloatField(node, "orthographicNear", orthographic_near, nestedFieldName(field_name, "orthographicNear"), errors) ||
        !readFloatField(node, "orthographicFar", orthographic_far, nestedFieldName(field_name, "orthographicFar"), errors)) {
        return false;
    }

    const auto projection_type = cameraProjectionTypeFromName(projection);
    if (!projection_type.has_value()) {
        appendError(errors,
                    "Field '" + nestedFieldName(field_name, "projection") + "' has unsupported value '" + projection +
                        "'.");
        return false;
    }

    camera.projection_type = *projection_type;
    camera.position = position;
    camera.orientation_euler_radians = orientation;
    camera.perspective_vertical_fov_radians = perspective_vertical_fov_radians;
    camera.perspective_near = perspective_near;
    camera.perspective_far = perspective_far;
    camera.orthographic_size = orthographic_size;
    camera.orthographic_near = orthographic_near;
    camera.orthographic_far = orthographic_far;
    return true;
}

inline Json interactionStateJson(const EditorViewportInteractionState& interaction)
{
    Json result = Json::object();
    result["visible"] = interaction.visible;
    result["focused"] = interaction.focused;
    result["hovered"] = interaction.hovered;
    result["inputEnabled"] = interaction.input_enabled;
    result["mouseCaptured"] = interaction.mouse_captured;
    result["runtimeViewport"] = interaction.runtime_viewport;
    result["pickDebugEnabled"] = interaction.pick_debug_enabled;
    return result;
}

inline bool interactionStateFromJson(const Json& node,
                                     EditorViewportInteractionState& interaction,
                                     std::vector<std::string>* errors,
                                     std::string_view field_name)
{
    if (!requireObject(node, field_name, errors)) {
        return false;
    }

    if (!validateObjectFields(node,
                              field_name,
                              {"visible",
                               "focused",
                               "hovered",
                               "inputEnabled",
                               "mouseCaptured",
                               "runtimeViewport",
                               "pickDebugEnabled"},
                              errors)) {
        return false;
    }

    if (!readBoolField(node, "visible", interaction.visible, nestedFieldName(field_name, "visible"), errors) ||
        !readBoolField(node, "focused", interaction.focused, nestedFieldName(field_name, "focused"), errors) ||
        !readBoolField(node, "hovered", interaction.hovered, nestedFieldName(field_name, "hovered"), errors) ||
        !readBoolField(node, "inputEnabled", interaction.input_enabled, nestedFieldName(field_name, "inputEnabled"), errors) ||
        !readBoolField(node, "mouseCaptured", interaction.mouse_captured, nestedFieldName(field_name, "mouseCaptured"), errors) ||
        !readBoolField(node, "runtimeViewport", interaction.runtime_viewport, nestedFieldName(field_name, "runtimeViewport"), errors) ||
        !readBoolField(node, "pickDebugEnabled", interaction.pick_debug_enabled, nestedFieldName(field_name, "pickDebugEnabled"), errors)) {
        return false;
    }

    return true;
}

inline Json viewportStateJson(const EditorViewportState& state)
{
    Json result = Json::object();
    result["viewportId"] = state.viewport_id;
    result["title"] = state.title;
    result["kind"] = std::string(editorViewportKindName(state.kind));
    result["size"] = Json{{"width", state.size.width}, {"height", state.size.height}};
    result["camera"] = cameraStateJson(state.camera);
    result["interaction"] = interactionStateJson(state.interaction);
    result["transformTool"] = std::string(editorTransformToolName(state.transform_tool));
    result["transformSpace"] = std::string(editorTransformSpaceName(state.transform_space));
    result["debugViewMode"] = std::string(editorRenderDebugViewModeName(state.debug_view_mode));
    result["debugVelocityScale"] = state.debug_velocity_scale;
    result["renderPlane"] = renderPlaneStateJson(state.render_plane);
    return result;
}

inline bool viewportStateFromJson(const Json& node,
                                  EditorViewportState& state,
                                  std::vector<std::string>* errors,
                                  std::string_view field_name)
{
    if (!requireObject(node, field_name, errors)) {
        return false;
    }

    if (!validateObjectFields(node,
                              field_name,
                              {"viewportId",
                               "title",
                               "kind",
                               "size",
                               "camera",
                               "interaction",
                               "transformTool",
                               "transformSpace",
                               "debugViewMode",
                               "debugVelocityScale",
                               "renderPlane"},
                              errors)) {
        return false;
    }

    uint64_t viewport_id = 0;
    std::string title;
    std::string kind;
    std::string transform_tool;
    std::string transform_space;
    std::string debug_view_mode;
    float debug_velocity_scale = 0.0f;

    if (!readUint64Field(node, "viewportId", viewport_id, nestedFieldName(field_name, "viewportId"), errors) ||
        !readStringField(node, "title", title, nestedFieldName(field_name, "title"), errors) ||
        !readStringField(node, "kind", kind, nestedFieldName(field_name, "kind"), errors) ||
        !readFloatField(node, "debugVelocityScale", debug_velocity_scale, nestedFieldName(field_name, "debugVelocityScale"), errors)) {
        return false;
    }

    const auto size_it = node.find("size");
    const auto camera_it = node.find("camera");
    const auto interaction_it = node.find("interaction");
    const auto render_plane_it = node.find("renderPlane");
    if (size_it == node.end() || camera_it == node.end() || interaction_it == node.end() || render_plane_it == node.end()) {
        appendError(errors, "Field '" + std::string(field_name) + "' is missing required properties.");
        return false;
    }

    if (!requireObject(*size_it, nestedFieldName(field_name, "size"), errors) ||
        !requireObject(*camera_it, nestedFieldName(field_name, "camera"), errors) ||
        !requireObject(*interaction_it, nestedFieldName(field_name, "interaction"), errors) ||
        !requireObject(*render_plane_it, nestedFieldName(field_name, "renderPlane"), errors)) {
        return false;
    }

    uint64_t width = 0;
    uint64_t height = 0;
    if (!readUint64Field(*size_it, "width", width, nestedFieldName(field_name, "size.width"), errors) ||
        !readUint64Field(*size_it, "height", height, nestedFieldName(field_name, "size.height"), errors)) {
        return false;
    }

    if (!readStringField(node, "transformTool", transform_tool, nestedFieldName(field_name, "transformTool"), errors) ||
        !readStringField(node, "transformSpace", transform_space, nestedFieldName(field_name, "transformSpace"), errors) ||
        !readStringField(node, "debugViewMode", debug_view_mode, nestedFieldName(field_name, "debugViewMode"), errors)) {
        return false;
    }

    const auto parsed_kind = viewportKindFromName(kind);
    if (!parsed_kind.has_value()) {
        appendError(errors,
                    "Field '" + nestedFieldName(field_name, "kind") + "' has unsupported value '" + kind + "'.");
        return false;
    }

    const auto parsed_transform_tool = transformToolFromName(transform_tool);
    if (!parsed_transform_tool.has_value()) {
        appendError(errors,
                    "Field '" + nestedFieldName(field_name, "transformTool") + "' has unsupported value '" +
                        transform_tool + "'.");
        return false;
    }

    const auto parsed_transform_space = transformSpaceFromName(transform_space);
    if (!parsed_transform_space.has_value()) {
        appendError(errors,
                    "Field '" + nestedFieldName(field_name, "transformSpace") + "' has unsupported value '" +
                        transform_space + "'.");
        return false;
    }

    const auto parsed_debug_view_mode = renderDebugViewModeFromName(debug_view_mode);
    if (!parsed_debug_view_mode.has_value()) {
        appendError(errors,
                    "Field '" + nestedFieldName(field_name, "debugViewMode") + "' has unsupported value '" +
                        debug_view_mode + "'.");
        return false;
    }

    state.viewport_id = static_cast<EditorViewportId>(viewport_id);
    state.title = std::move(title);
    state.kind = *parsed_kind;
    state.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    if (!cameraStateFromJson(*camera_it, state.camera, errors, nestedFieldName(field_name, "camera")) ||
        !interactionStateFromJson(*interaction_it, state.interaction, errors, nestedFieldName(field_name, "interaction")) ||
        !renderPlaneStateFromJson(*render_plane_it, state.render_plane, errors, nestedFieldName(field_name, "renderPlane"))) {
        return false;
    }
    state.transform_tool = *parsed_transform_tool;
    state.transform_space = *parsed_transform_space;
    state.debug_view_mode = *parsed_debug_view_mode;
    state.debug_velocity_scale = debug_velocity_scale;
    return true;
}

inline Json viewportCommandJson(const EditorViewportCommand& command)
{
    Json result = Json::object();
    result["kind"] = std::string(editorViewportCommandName(command.kind));
    result["viewportId"] = command.viewport_id;

    switch (command.kind) {
        case EditorViewportCommandKind::CreateViewport:
            result["title"] = command.title;
            result["viewportKind"] = std::string(editorViewportKindName(command.viewport_kind));
            result["size"] = Json{{"width", command.size.width}, {"height", command.size.height}};
            result["camera"] = cameraStateJson(command.camera);
            result["interaction"] = interactionStateJson(command.interaction);
            result["transformTool"] = std::string(editorTransformToolName(command.transform_tool));
            result["transformSpace"] = std::string(editorTransformSpaceName(command.transform_space));
            result["debugViewMode"] = std::string(editorRenderDebugViewModeName(command.debug_view_mode));
            result["debugVelocityScale"] = command.debug_velocity_scale;
            break;
        case EditorViewportCommandKind::ResizeViewport:
            result["size"] = Json{{"width", command.size.width}, {"height", command.size.height}};
            break;
        case EditorViewportCommandKind::SetCamera:
            result["camera"] = cameraStateJson(command.camera);
            break;
        case EditorViewportCommandKind::SetInteractionState:
            result["interaction"] = interactionStateJson(command.interaction);
            break;
        case EditorViewportCommandKind::SetTransformState:
            result["transformTool"] = std::string(editorTransformToolName(command.transform_tool));
            result["transformSpace"] = std::string(editorTransformSpaceName(command.transform_space));
            break;
        case EditorViewportCommandKind::SetDebugViewMode:
            result["debugViewMode"] = std::string(editorRenderDebugViewModeName(command.debug_view_mode));
            break;
        case EditorViewportCommandKind::SetDebugVelocityScale:
            result["debugVelocityScale"] = command.debug_velocity_scale;
            break;
        case EditorViewportCommandKind::RequestPick:
            result["pickX"] = command.pick_x;
            result["pickY"] = command.pick_y;
            break;
        case EditorViewportCommandKind::DestroyViewport:
        case EditorViewportCommandKind::CaptureFrame:
            break;
    }

    return result;
}

inline bool viewportCommandFromJson(const Json& node,
                                    EditorViewportCommand& command,
                                    std::vector<std::string>* errors,
                                    std::string_view field_name)
{
    if (!requireObject(node, field_name, errors)) {
        return false;
    }

    if (!validateObjectFields(node,
                              field_name,
                              {"kind",
                               "viewportId",
                               "title",
                               "viewportKind",
                               "size",
                               "camera",
                               "interaction",
                               "transformTool",
                               "transformSpace",
                               "debugViewMode",
                               "debugVelocityScale",
                               "pickX",
                               "pickY"},
                              errors)) {
        return false;
    }

    std::string kind;
    uint64_t viewport_id = 0;
    std::string title{"Viewport"};
    std::string viewport_kind_name{std::string(editorViewportKindName(EditorViewportKind::Scene))};
    std::string transform_tool_name{std::string(editorTransformToolName(EditorTransformTool::Translate))};
    std::string transform_space_name{std::string(editorTransformSpaceName(EditorTransformSpace::Local))};
    std::string debug_view_mode_name{std::string(editorRenderDebugViewModeName(RenderDebugViewMode::None))};
    uint64_t pick_x = 0;
    uint64_t pick_y = 0;

    if (!readStringField(node, "kind", kind, nestedFieldName(field_name, "kind"), errors) ||
        !readUint64Field(node, "viewportId", viewport_id, nestedFieldName(field_name, "viewportId"), errors) ||
        !readOptionalStringField(node, "title", title, nestedFieldName(field_name, "title"), errors) ||
        !readOptionalStringField(node,
                                 "viewportKind",
                                 viewport_kind_name,
                                 nestedFieldName(field_name, "viewportKind"),
                                 errors) ||
        !readOptionalStringField(node,
                                 "transformTool",
                                 transform_tool_name,
                                 nestedFieldName(field_name, "transformTool"),
                                 errors) ||
        !readOptionalStringField(node,
                                 "transformSpace",
                                 transform_space_name,
                                 nestedFieldName(field_name, "transformSpace"),
                                 errors) ||
        !readOptionalStringField(node,
                                 "debugViewMode",
                                 debug_view_mode_name,
                                 nestedFieldName(field_name, "debugViewMode"),
                                 errors) ||
        !readOptionalUint64Field(node, "pickX", pick_x, nestedFieldName(field_name, "pickX"), errors) ||
        !readOptionalUint64Field(node, "pickY", pick_y, nestedFieldName(field_name, "pickY"), errors)) {
        return false;
    }

    const auto parsed_kind = [&]() -> std::optional<EditorViewportCommandKind> {
        if (kind == editorViewportCommandName(EditorViewportCommandKind::CreateViewport)) {
            return EditorViewportCommandKind::CreateViewport;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::DestroyViewport)) {
            return EditorViewportCommandKind::DestroyViewport;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::ResizeViewport)) {
            return EditorViewportCommandKind::ResizeViewport;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::SetCamera)) {
            return EditorViewportCommandKind::SetCamera;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::SetInteractionState)) {
            return EditorViewportCommandKind::SetInteractionState;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::SetTransformState)) {
            return EditorViewportCommandKind::SetTransformState;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::SetDebugViewMode)) {
            return EditorViewportCommandKind::SetDebugViewMode;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::SetDebugVelocityScale)) {
            return EditorViewportCommandKind::SetDebugVelocityScale;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::RequestPick)) {
            return EditorViewportCommandKind::RequestPick;
        }
        if (kind == editorViewportCommandName(EditorViewportCommandKind::CaptureFrame)) {
            return EditorViewportCommandKind::CaptureFrame;
        }
        return std::nullopt;
    }();
    if (!parsed_kind.has_value()) {
        appendError(errors,
                    "Field '" + nestedFieldName(field_name, "kind") + "' has unsupported value '" + kind + "'.");
        return false;
    }

    command = EditorViewportCommand{};
    command.kind = *parsed_kind;
    command.viewport_id = static_cast<EditorViewportId>(viewport_id);
    command.title = std::move(title);

    const auto parsed_viewport_kind = viewportKindFromName(viewport_kind_name);
    if (command.kind == EditorViewportCommandKind::CreateViewport) {
        if (!parsed_viewport_kind.has_value()) {
            appendError(errors,
                        "Field '" + nestedFieldName(field_name, "viewportKind") + "' has unsupported value '" +
                            viewport_kind_name + "'.");
            return false;
        }

        command.viewport_kind = *parsed_viewport_kind;

        const auto size_it = node.find("size");
        const auto camera_it = node.find("camera");
        const auto interaction_it = node.find("interaction");
        if (size_it == node.end() || camera_it == node.end() || interaction_it == node.end()) {
            appendError(errors, "Field '" + std::string(field_name) + "' is missing required properties.");
            return false;
        }

        if (!requireObject(*size_it, nestedFieldName(field_name, "size"), errors) ||
            !requireObject(*camera_it, nestedFieldName(field_name, "camera"), errors) ||
            !requireObject(*interaction_it, nestedFieldName(field_name, "interaction"), errors)) {
            return false;
        }

        uint64_t width = 0;
        uint64_t height = 0;
        if (!readUint64Field(*size_it, "width", width, nestedFieldName(field_name, "size.width"), errors) ||
            !readUint64Field(*size_it, "height", height, nestedFieldName(field_name, "size.height"), errors) ||
            !cameraStateFromJson(*camera_it, command.camera, errors, nestedFieldName(field_name, "camera")) ||
            !interactionStateFromJson(*interaction_it,
                                      command.interaction,
                                      errors,
                                      nestedFieldName(field_name, "interaction"))) {
            return false;
        }

        command.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        if (!readStringField(node,
                             "transformTool",
                             transform_tool_name,
                             nestedFieldName(field_name, "transformTool"),
                             errors) ||
            !readStringField(node,
                             "transformSpace",
                             transform_space_name,
                             nestedFieldName(field_name, "transformSpace"),
                             errors) ||
            !readStringField(node,
                             "debugViewMode",
                             debug_view_mode_name,
                             nestedFieldName(field_name, "debugViewMode"),
                             errors) ||
            !readFloatField(node,
                            "debugVelocityScale",
                            command.debug_velocity_scale,
                            nestedFieldName(field_name, "debugVelocityScale"),
                            errors)) {
            return false;
        }

        const auto parsed_transform_tool = transformToolFromName(transform_tool_name);
        const auto parsed_transform_space = transformSpaceFromName(transform_space_name);
        const auto parsed_debug_view_mode = renderDebugViewModeFromName(debug_view_mode_name);
        if (!parsed_transform_tool.has_value() || !parsed_transform_space.has_value() || !parsed_debug_view_mode.has_value()) {
            appendError(errors, "Field '" + std::string(field_name) + "' contains unsupported viewport settings.");
            return false;
        }

        command.transform_tool = *parsed_transform_tool;
        command.transform_space = *parsed_transform_space;
        command.debug_view_mode = *parsed_debug_view_mode;
        command.pick_x = static_cast<uint32_t>(pick_x);
        command.pick_y = static_cast<uint32_t>(pick_y);
        return true;
    }

    if (parsed_viewport_kind.has_value()) {
        command.viewport_kind = *parsed_viewport_kind;
    }

    if (command.kind == EditorViewportCommandKind::DestroyViewport) {
        return true;
    }

    const auto size_it = node.find("size");
    if (command.kind == EditorViewportCommandKind::ResizeViewport) {
        if (size_it == node.end() || !requireObject(*size_it, nestedFieldName(field_name, "size"), errors)) {
            appendError(errors, "Field '" + nestedFieldName(field_name, "size") + "' is required.");
            return false;
        }

        uint64_t width = 0;
        uint64_t height = 0;
        if (!readUint64Field(*size_it, "width", width, nestedFieldName(field_name, "size.width"), errors) ||
            !readUint64Field(*size_it, "height", height, nestedFieldName(field_name, "size.height"), errors)) {
            return false;
        }

        command.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        return true;
    }

    if (command.kind == EditorViewportCommandKind::SetCamera) {
        const auto camera_it = node.find("camera");
        if (camera_it == node.end() || !requireObject(*camera_it, nestedFieldName(field_name, "camera"), errors)) {
            appendError(errors, "Field '" + nestedFieldName(field_name, "camera") + "' is required.");
            return false;
        }

        return cameraStateFromJson(*camera_it, command.camera, errors, nestedFieldName(field_name, "camera"));
    }

    if (command.kind == EditorViewportCommandKind::SetInteractionState) {
        const auto interaction_it = node.find("interaction");
        if (interaction_it == node.end() ||
            !requireObject(*interaction_it, nestedFieldName(field_name, "interaction"), errors)) {
            appendError(errors, "Field '" + nestedFieldName(field_name, "interaction") + "' is required.");
            return false;
        }

        return interactionStateFromJson(*interaction_it,
                                        command.interaction,
                                        errors,
                                        nestedFieldName(field_name, "interaction"));
    }

    if (command.kind == EditorViewportCommandKind::SetTransformState) {
        if (!readOptionalStringField(node,
                                     "transformTool",
                                     transform_tool_name,
                                     nestedFieldName(field_name, "transformTool"),
                                     errors) ||
            !readOptionalStringField(node,
                                     "transformSpace",
                                     transform_space_name,
                                     nestedFieldName(field_name, "transformSpace"),
                                     errors)) {
            return false;
        }

        const auto parsed_transform_tool = transformToolFromName(transform_tool_name);
        const auto parsed_transform_space = transformSpaceFromName(transform_space_name);
        if (!parsed_transform_tool.has_value() || !parsed_transform_space.has_value()) {
            appendError(errors, "Field '" + std::string(field_name) + "' contains unsupported transform settings.");
            return false;
        }

        command.transform_tool = *parsed_transform_tool;
        command.transform_space = *parsed_transform_space;
        return true;
    }

    if (command.kind == EditorViewportCommandKind::SetDebugViewMode) {
        if (!readOptionalStringField(node,
                                     "debugViewMode",
                                     debug_view_mode_name,
                                     nestedFieldName(field_name, "debugViewMode"),
                                     errors)) {
            return false;
        }

        const auto parsed_debug_view_mode = renderDebugViewModeFromName(debug_view_mode_name);
        if (!parsed_debug_view_mode.has_value()) {
            appendError(errors,
                        "Field '" + nestedFieldName(field_name, "debugViewMode") + "' has unsupported value '" +
                            debug_view_mode_name + "'.");
            return false;
        }

        command.debug_view_mode = *parsed_debug_view_mode;
        return true;
    }

    if (command.kind == EditorViewportCommandKind::SetDebugVelocityScale) {
        return readFloatField(node,
                              "debugVelocityScale",
                              command.debug_velocity_scale,
                              nestedFieldName(field_name, "debugVelocityScale"),
                              errors);
    }

    if (command.kind == EditorViewportCommandKind::RequestPick) {
        command.pick_x = static_cast<uint32_t>(pick_x);
        command.pick_y = static_cast<uint32_t>(pick_y);
        return true;
    }

    if (command.kind == EditorViewportCommandKind::CaptureFrame) {
        return true;
    }

    appendError(errors, "Field '" + std::string(field_name) + "' has unsupported command kind.");
    return false;
}

} // namespace view_protocol_detail

using Json = view_protocol_detail::Json;
using view_protocol_detail::nestedFieldName;

inline Json editorProtocolJson()
{
    return view_protocol_detail::writeProtocol();
}

inline Json editorViewportCameraStateJson(const EditorViewportCameraState& camera)
{
    return view_protocol_detail::cameraStateJson(camera);
}

inline bool editorViewportCameraStateFromJson(const view_protocol_detail::Json& node,
                                              EditorViewportCameraState& camera,
                                              std::vector<std::string>* errors = nullptr,
                                              std::string_view field_name = "camera")
{
    return view_protocol_detail::cameraStateFromJson(node, camera, errors, field_name);
}

inline Json editorViewportInteractionStateJson(const EditorViewportInteractionState& interaction)
{
    return view_protocol_detail::interactionStateJson(interaction);
}

inline bool editorViewportInteractionStateFromJson(const view_protocol_detail::Json& node,
                                                   EditorViewportInteractionState& interaction,
                                                   std::vector<std::string>* errors = nullptr,
                                                   std::string_view field_name = "interaction")
{
    return view_protocol_detail::interactionStateFromJson(node, interaction, errors, field_name);
}

inline Json editorRenderPlaneDescriptorJson(const EditorRenderPlaneDescriptor& descriptor)
{
    return view_protocol_detail::renderPlaneDescriptorJson(descriptor);
}

inline bool editorRenderPlaneDescriptorFromJson(const view_protocol_detail::Json& node,
                                                EditorRenderPlaneDescriptor& descriptor,
                                                std::vector<std::string>* errors = nullptr,
                                                std::string_view field_name = "renderPlane.descriptor")
{
    return view_protocol_detail::renderPlaneDescriptorFromJson(node, descriptor, errors, field_name);
}

inline Json editorRenderFrameInfoJson(const EditorRenderFrameInfo& frame)
{
    return view_protocol_detail::renderFrameInfoJson(frame);
}

inline bool editorRenderFrameInfoFromJson(const view_protocol_detail::Json& node,
                                          EditorRenderFrameInfo& frame,
                                          std::vector<std::string>* errors = nullptr,
                                          std::string_view field_name = "renderPlane.frame")
{
    return view_protocol_detail::renderFrameInfoFromJson(node, frame, errors, field_name);
}

inline Json editorRenderPlaneStateJson(const EditorRenderPlaneState& state)
{
    return view_protocol_detail::renderPlaneStateJson(state);
}

inline bool editorRenderPlaneStateFromJson(const view_protocol_detail::Json& node,
                                           EditorRenderPlaneState& state,
                                           std::vector<std::string>* errors = nullptr,
                                           std::string_view field_name = "renderPlane")
{
    return view_protocol_detail::renderPlaneStateFromJson(node, state, errors, field_name);
}

inline Json editorViewportStateJson(const EditorViewportState& state)
{
    return view_protocol_detail::viewportStateJson(state);
}

inline bool editorViewportStateFromJson(const view_protocol_detail::Json& node,
                                        EditorViewportState& state,
                                        std::vector<std::string>* errors = nullptr,
                                        std::string_view field_name = "state")
{
    return view_protocol_detail::viewportStateFromJson(node, state, errors, field_name);
}

inline Json editorViewportCommandJson(const EditorViewportCommand& command)
{
    return view_protocol_detail::viewportCommandJson(command);
}

inline bool editorViewportCommandFromJson(const view_protocol_detail::Json& node,
                                          EditorViewportCommand& command,
                                          std::vector<std::string>* errors = nullptr,
                                          std::string_view field_name = "command")
{
    return view_protocol_detail::viewportCommandFromJson(node, command, errors, field_name);
}

inline Json editorViewportCommandResultJson(const EditorViewportCommandResult& result)
{
    Json json = Json::object();
    json["accepted"] = result.accepted;
    json["changed"] = result.changed;
    json["requestPick"] = result.request_pick;
    json["requestCapture"] = result.request_capture;
    return json;
}

inline bool editorViewportCommandResultFromJson(const view_protocol_detail::Json& node,
                                                EditorViewportCommandResult& result,
                                                std::vector<std::string>* errors = nullptr,
                                                std::string_view field_name = "result")
{
    if (!view_protocol_detail::requireObject(node, field_name, errors)) {
        return false;
    }

    if (!view_protocol_detail::validateObjectFields(node,
                                                    field_name,
                                                    {"accepted", "changed", "requestPick", "requestCapture"},
                                                    errors)) {
        return false;
    }

    if (!view_protocol_detail::readBoolField(node, "accepted", result.accepted, nestedFieldName(field_name, "accepted"), errors) ||
        !view_protocol_detail::readBoolField(node, "changed", result.changed, nestedFieldName(field_name, "changed"), errors) ||
        !view_protocol_detail::readBoolField(node,
                                             "requestPick",
                                             result.request_pick,
                                             nestedFieldName(field_name, "requestPick"),
                                             errors) ||
        !view_protocol_detail::readBoolField(node,
                                             "requestCapture",
                                             result.request_capture,
                                             nestedFieldName(field_name, "requestCapture"),
                                             errors)) {
        return false;
    }

    return true;
}

inline Json editorViewportCommandEnvelopeJson(const EditorViewportCommand& command)
{
    Json envelope = Json::object();
    envelope["protocol"] = editorProtocolJson();
    envelope["command"] = editorViewportCommandJson(command);
    return envelope;
}

inline Json editorViewportStateEnvelopeJson(const EditorViewportState& state)
{
    Json envelope = Json::object();
    envelope["protocol"] = editorProtocolJson();
    envelope["state"] = editorViewportStateJson(state);
    return envelope;
}

inline Json editorViewportResponseEnvelopeJson(const EditorViewportState& state,
                                               const EditorViewportCommandResult& result)
{
    Json envelope = Json::object();
    envelope["protocol"] = editorProtocolJson();
    envelope["accepted"] = result.accepted;
    envelope["changed"] = result.changed;
    envelope["requestPick"] = result.request_pick;
    envelope["requestCapture"] = result.request_capture;
    envelope["state"] = editorViewportStateJson(state);
    return envelope;
}

inline bool editorViewportCommandEnvelopeFromJson(const view_protocol_detail::Json& node,
                                                  EditorViewportCommand& command,
                                                  std::vector<std::string>* errors = nullptr,
                                                  std::string_view field_name = "request")
{
    if (!view_protocol_detail::requireObject(node, field_name, errors)) {
        return false;
    }

    if (!view_protocol_detail::validateObjectFields(node, field_name, {"protocol", "command"}, errors)) {
        return false;
    }

    const auto protocol = node.find("protocol");
    const auto command_node = node.find("command");
    if (protocol == node.end() || command_node == node.end()) {
        view_protocol_detail::appendError(errors, "Field '" + std::string(field_name) + "' is missing required properties.");
        return false;
    }

    if (!view_protocol_detail::requireObject(*protocol, nestedFieldName(field_name, "protocol"), errors) ||
        !view_protocol_detail::requireObject(*command_node, nestedFieldName(field_name, "command"), errors)) {
        return false;
    }

    std::string protocol_name;
    uint64_t protocol_version = 0;
    if (!view_protocol_detail::readStringField(*protocol,
                                               "name",
                                               protocol_name,
                                               nestedFieldName(field_name, "protocol.name"),
                                               errors) ||
        !view_protocol_detail::readUint64Field(*protocol,
                                               "version",
                                               protocol_version,
                                               nestedFieldName(field_name, "protocol.version"),
                                               errors)) {
        return false;
    }

    if (protocol_name != kEditorProtocolName || protocol_version != kEditorProtocolVersion) {
        view_protocol_detail::appendError(errors,
                                           "Field '" + nestedFieldName(field_name, "protocol") +
                                               "' has an incompatible protocol.");
        return false;
    }

    return editorViewportCommandFromJson(*command_node, command, errors, nestedFieldName(field_name, "command"));
}

inline bool editorViewportStateEnvelopeFromJson(const view_protocol_detail::Json& node,
                                                EditorViewportState& state,
                                                std::vector<std::string>* errors = nullptr,
                                                std::string_view field_name = "stateEnvelope")
{
    if (!view_protocol_detail::requireObject(node, field_name, errors)) {
        return false;
    }

    if (!view_protocol_detail::validateObjectFields(node, field_name, {"protocol", "state"}, errors)) {
        return false;
    }

    const auto protocol = node.find("protocol");
    const auto state_node = node.find("state");
    if (protocol == node.end() || state_node == node.end()) {
        view_protocol_detail::appendError(errors, "Field '" + std::string(field_name) + "' is missing required properties.");
        return false;
    }

    if (!view_protocol_detail::requireObject(*protocol, nestedFieldName(field_name, "protocol"), errors) ||
        !view_protocol_detail::requireObject(*state_node, nestedFieldName(field_name, "state"), errors)) {
        return false;
    }

    std::string protocol_name;
    uint64_t protocol_version = 0;
    if (!view_protocol_detail::readStringField(*protocol,
                                               "name",
                                               protocol_name,
                                               nestedFieldName(field_name, "protocol.name"),
                                               errors) ||
        !view_protocol_detail::readUint64Field(*protocol,
                                               "version",
                                               protocol_version,
                                               nestedFieldName(field_name, "protocol.version"),
                                               errors)) {
        return false;
    }

    if (protocol_name != kEditorProtocolName || protocol_version != kEditorProtocolVersion) {
        view_protocol_detail::appendError(errors,
                                           "Field '" + nestedFieldName(field_name, "protocol") +
                                               "' has an incompatible protocol.");
        return false;
    }

    return editorViewportStateFromJson(*state_node, state, errors, nestedFieldName(field_name, "state"));
}

inline bool editorViewportResponseEnvelopeFromJson(const view_protocol_detail::Json& node,
                                                   EditorViewportState& state,
                                                   EditorViewportCommandResult& result,
                                                   std::vector<std::string>* errors = nullptr,
                                                   std::string_view field_name = "response")
{
    if (!view_protocol_detail::requireObject(node, field_name, errors)) {
        return false;
    }

    if (!view_protocol_detail::validateObjectFields(node,
                                                    field_name,
                                                    {"protocol", "accepted", "changed", "requestPick", "requestCapture", "state"},
                                                    errors)) {
        return false;
    }

    const auto protocol = node.find("protocol");
    const auto state_node = node.find("state");
    if (protocol == node.end() || state_node == node.end()) {
        view_protocol_detail::appendError(errors, "Field '" + std::string(field_name) + "' is missing required properties.");
        return false;
    }

    if (!view_protocol_detail::requireObject(*protocol, nestedFieldName(field_name, "protocol"), errors) ||
        !view_protocol_detail::requireObject(*state_node, nestedFieldName(field_name, "state"), errors)) {
        return false;
    }

    std::string protocol_name;
    uint64_t protocol_version = 0;
    if (!view_protocol_detail::readStringField(*protocol,
                                               "name",
                                               protocol_name,
                                               nestedFieldName(field_name, "protocol.name"),
                                               errors) ||
        !view_protocol_detail::readUint64Field(*protocol,
                                               "version",
                                               protocol_version,
                                               nestedFieldName(field_name, "protocol.version"),
                                               errors)) {
        return false;
    }

    if (protocol_name != kEditorProtocolName || protocol_version != kEditorProtocolVersion) {
        view_protocol_detail::appendError(errors,
                                           "Field '" + nestedFieldName(field_name, "protocol") +
                                               "' has an incompatible protocol.");
        return false;
    }

    if (!view_protocol_detail::readBoolField(node,
                                             "accepted",
                                             result.accepted,
                                             nestedFieldName(field_name, "accepted"),
                                             errors) ||
        !view_protocol_detail::readBoolField(node,
                                             "changed",
                                             result.changed,
                                             nestedFieldName(field_name, "changed"),
                                             errors) ||
        !view_protocol_detail::readBoolField(node,
                                             "requestPick",
                                             result.request_pick,
                                             nestedFieldName(field_name, "requestPick"),
                                             errors) ||
        !view_protocol_detail::readBoolField(node,
                                             "requestCapture",
                                             result.request_capture,
                                             nestedFieldName(field_name, "requestCapture"),
                                             errors)) {
        return false;
    }

    return editorViewportStateFromJson(*state_node, state, errors, nestedFieldName(field_name, "state"));
}

inline bool applyEditorViewportCommandEnvelopeJson(EditorViewportState& state,
                                                   const view_protocol_detail::Json& request,
                                                   view_protocol_detail::Json& response,
                                                   std::vector<std::string>* errors = nullptr,
                                                   std::string_view field_name = "request")
{
    EditorViewportCommand command;
    if (!editorViewportCommandEnvelopeFromJson(request, command, errors, field_name)) {
        return false;
    }

    const EditorViewportCommandResult result = applyEditorViewportCommand(state, command);
    response = editorViewportResponseEnvelopeJson(state, result);
    return true;
}

inline std::string dumpEditorProtocolJson(const view_protocol_detail::Json& value)
{
    return value.dump(2, ' ', false, view_protocol_detail::Json::error_handler_t::replace);
}

inline void writeEditorViewportCommandJson(std::ostream& out, const EditorViewportCommand& command)
{
    out << dumpEditorProtocolJson(editorViewportCommandJson(command)) << '\n';
}

inline void writeEditorViewportStateJson(std::ostream& out, const EditorViewportState& state)
{
    out << dumpEditorProtocolJson(editorViewportStateJson(state)) << '\n';
}

inline void writeEditorViewportCommandEnvelopeJson(std::ostream& out, const EditorViewportCommand& command)
{
    out << dumpEditorProtocolJson(editorViewportCommandEnvelopeJson(command)) << '\n';
}

inline void writeEditorViewportStateEnvelopeJson(std::ostream& out, const EditorViewportState& state)
{
    out << dumpEditorProtocolJson(editorViewportStateEnvelopeJson(state)) << '\n';
}

inline void writeEditorViewportResponseEnvelopeJson(std::ostream& out,
                                                    const EditorViewportState& state,
                                                    const EditorViewportCommandResult& result)
{
    out << dumpEditorProtocolJson(editorViewportResponseEnvelopeJson(state, result)) << '\n';
}

} // namespace luna::editor
