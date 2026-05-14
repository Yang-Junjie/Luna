#pragma once

#include "EditorApi/EditorTypes.h"
#include "Viewport/ViewportSurface.h"

namespace luna {

struct TextureViewportPresentation {
    editor::TextureView texture;
    editor::UVec2 framebuffer_size{};
    bool presentable{false};
};

class TextureViewportInstance final {
public:
    void setTexture(editor::TextureView texture) noexcept;
    void clearTexture() noexcept;

    [[nodiscard]] const editor::TextureView& texture() const noexcept;
    [[nodiscard]] const ViewportSurfaceState& state() const noexcept;
    [[nodiscard]] const TextureViewportPresentation& presentation() const noexcept;

    const TextureViewportPresentation& sync(editor::UVec2 framebuffer_size) noexcept;
    const TextureViewportPresentation& sync(editor::TextureView texture, editor::UVec2 framebuffer_size) noexcept;

private:
    void refreshPresentation() noexcept;

    ViewportSurface m_surface;
    editor::TextureView m_texture;
    TextureViewportPresentation m_presentation;
};

} // namespace luna
