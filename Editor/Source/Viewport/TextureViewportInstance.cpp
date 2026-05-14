#include "Viewport/TextureViewportInstance.h"

namespace luna {

void TextureViewportInstance::setTexture(editor::TextureView texture) noexcept
{
    m_texture = texture;
    const ViewportSurfaceState& current_state = m_surface.state();
    m_surface.setPresentation(current_state.width,
                              current_state.height,
                              m_texture.y_flip,
                              m_texture.valid() && current_state.width > 0 && current_state.height > 0);
    refreshPresentation();
}

void TextureViewportInstance::clearTexture() noexcept
{
    setTexture(editor::TextureView{});
}

const editor::TextureView& TextureViewportInstance::texture() const noexcept
{
    return m_texture;
}

const ViewportSurfaceState& TextureViewportInstance::state() const noexcept
{
    return m_surface.state();
}

const TextureViewportPresentation& TextureViewportInstance::presentation() const noexcept
{
    return m_presentation;
}

const TextureViewportPresentation& TextureViewportInstance::sync(editor::UVec2 framebuffer_size) noexcept
{
    m_surface.setPresentation(framebuffer_size.x,
                              framebuffer_size.y,
                              m_texture.y_flip,
                              m_texture.valid() && framebuffer_size.x > 0 && framebuffer_size.y > 0);
    refreshPresentation();
    return m_presentation;
}

const TextureViewportPresentation& TextureViewportInstance::sync(editor::TextureView texture,
                                                                 editor::UVec2 framebuffer_size) noexcept
{
    m_texture = texture;
    return sync(framebuffer_size);
}

void TextureViewportInstance::refreshPresentation() noexcept
{
    const ViewportSurfaceState& current_state = m_surface.state();
    m_presentation = TextureViewportPresentation{
        .texture = m_texture,
        .framebuffer_size = editor::UVec2{.x = current_state.width, .y = current_state.height},
        .presentable = current_state.presentable && m_texture.valid(),
    };
}

} // namespace luna
