#pragma once

#include "Asset/Asset.h"
#include "Asset/AssetTypes.h"
#include "Core/UUID.h"

#include <cstdint>

#include <string>

namespace luna::editor {

using EditorId = std::string;
using EntityId = UUID;
using ScriptEntryId = UUID;
using TextureHandle = uintptr_t;
using ViewportId = uint64_t;

inline constexpr ViewportId kInvalidViewportId{0};
inline constexpr ViewportId kDefaultViewportId{1};

struct Vec2 {
    float x{0.0f};
    float y{0.0f};
};

struct UVec2 {
    uint32_t x{0};
    uint32_t y{0};
};

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct Vec4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};
};

struct TextureView {
    TextureHandle id{0};
    UVec2 size{};
    bool y_flip{false};

    [[nodiscard]] bool valid() const noexcept
    {
        return id != 0 && size.x > 0 && size.y > 0;
    }
};

struct AssetDropPayload {
    AssetHandle handle{0};
    AssetType type{AssetType::None};

    [[nodiscard]] bool valid() const noexcept
    {
        return handle.isValid() && type != AssetType::None;
    }
};

enum class WindowFlag : uint32_t {
    None = 0,
    NoSavedSettings = 1u << 0,
    NoDocking = 1u << 1,
    NoPadding = 1u << 2,
};

using WindowFlags = uint32_t;

[[nodiscard]] constexpr WindowFlags operator|(WindowFlag lhs, WindowFlag rhs) noexcept
{
    return static_cast<WindowFlags>(lhs) | static_cast<WindowFlags>(rhs);
}

[[nodiscard]] constexpr bool hasWindowFlag(WindowFlags flags, WindowFlag flag) noexcept
{
    return (flags & static_cast<WindowFlags>(flag)) != 0u;
}

enum class TableFlag : uint32_t {
    None = 0,
    RowBg = 1u << 0,
    BordersInnerH = 1u << 1,
    BordersInnerV = 1u << 2,
    SizingStretchProp = 1u << 3,
    ScrollY = 1u << 4,
};

using TableFlags = uint32_t;

[[nodiscard]] constexpr TableFlags operator|(TableFlag lhs, TableFlag rhs) noexcept
{
    return static_cast<TableFlags>(lhs) | static_cast<TableFlags>(rhs);
}

[[nodiscard]] constexpr TableFlags operator|(TableFlags lhs, TableFlag rhs) noexcept
{
    return lhs | static_cast<TableFlags>(rhs);
}

[[nodiscard]] constexpr bool hasTableFlag(TableFlags flags, TableFlag flag) noexcept
{
    return (flags & static_cast<TableFlags>(flag)) != 0u;
}

enum class TableColumnFlag : uint32_t {
    None = 0,
    WidthFixed = 1u << 0,
    WidthStretch = 1u << 1,
};

using TableColumnFlags = uint32_t;

[[nodiscard]] constexpr TableColumnFlags operator|(TableColumnFlag lhs, TableColumnFlag rhs) noexcept
{
    return static_cast<TableColumnFlags>(lhs) | static_cast<TableColumnFlags>(rhs);
}

[[nodiscard]] constexpr TableColumnFlags operator|(TableColumnFlags lhs, TableColumnFlag rhs) noexcept
{
    return lhs | static_cast<TableColumnFlags>(rhs);
}

[[nodiscard]] constexpr bool hasTableColumnFlag(TableColumnFlags flags, TableColumnFlag flag) noexcept
{
    return (flags & static_cast<TableColumnFlags>(flag)) != 0u;
}

enum class TreeNodeFlag : uint32_t {
    None = 0,
    OpenOnArrow = 1u << 0,
    OpenOnDoubleClick = 1u << 1,
    SpanAvailWidth = 1u << 2,
    Leaf = 1u << 3,
    NoTreePushOnOpen = 1u << 4,
    Selected = 1u << 5,
    DefaultOpen = 1u << 6,
    FramePadding = 1u << 7,
};

using TreeNodeFlags = uint32_t;

[[nodiscard]] constexpr TreeNodeFlags operator|(TreeNodeFlag lhs, TreeNodeFlag rhs) noexcept
{
    return static_cast<TreeNodeFlags>(lhs) | static_cast<TreeNodeFlags>(rhs);
}

[[nodiscard]] constexpr TreeNodeFlags operator|(TreeNodeFlags lhs, TreeNodeFlag rhs) noexcept
{
    return lhs | static_cast<TreeNodeFlags>(rhs);
}

[[nodiscard]] constexpr bool hasTreeNodeFlag(TreeNodeFlags flags, TreeNodeFlag flag) noexcept
{
    return (flags & static_cast<TreeNodeFlags>(flag)) != 0u;
}

enum class MouseButton : uint8_t {
    Left,
    Right,
    Middle,
};

} // namespace luna::editor
