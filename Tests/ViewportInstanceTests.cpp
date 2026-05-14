#include "Viewport/TextureViewportInstance.h"
#include "Viewport/ViewportSurface.h"

#include <iostream>
#include <string_view>

namespace {

class TestContext {
public:
    bool expect(bool condition, std::string_view message)
    {
        if (condition) {
            return true;
        }

        ++m_failures;
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    [[nodiscard]] int result() const noexcept
    {
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures{0};
};

luna::editor::TextureView validTexture(bool y_flip = false)
{
    return luna::editor::TextureView{
        .id = 0x1234u,
        .size = luna::editor::UVec2{.x = 128u, .y = 64u},
        .y_flip = y_flip,
    };
}

void testViewportSurfaceState(TestContext& context)
{
    luna::ViewportSurface surface;
    surface.setPresentation(320u, 180u, true, true);

    const luna::ViewportSurfaceState& state = surface.state();
    context.expect(state.width == 320u, "surface should store width");
    context.expect(state.height == 180u, "surface should store height");
    context.expect(state.y_flip, "surface should store y-flip");
    context.expect(state.presentable, "surface should store presentable state");

    surface.reset();
    context.expect(surface.state().width == 0u, "surface reset should clear width");
    context.expect(surface.state().height == 0u, "surface reset should clear height");
    context.expect(!surface.state().y_flip, "surface reset should clear y-flip");
    context.expect(!surface.state().presentable, "surface reset should clear presentable state");
}

void testTextureViewportPresentation(TestContext& context)
{
    luna::TextureViewportInstance viewport;

    const luna::TextureViewportPresentation& empty_presentation =
        viewport.sync(luna::editor::UVec2{.x = 640u, .y = 360u});
    context.expect(!empty_presentation.presentable, "texture viewport without texture should not be presentable");
    context.expect(empty_presentation.framebuffer_size.x == 640u,
                   "texture viewport should keep requested width without texture");
    context.expect(empty_presentation.framebuffer_size.y == 360u,
                   "texture viewport should keep requested height without texture");

    const luna::TextureViewportPresentation& valid_presentation =
        viewport.sync(validTexture(true), luna::editor::UVec2{.x = 640u, .y = 360u});
    context.expect(valid_presentation.presentable, "valid texture viewport should be presentable");
    context.expect(valid_presentation.texture.valid(), "valid texture viewport should expose texture");
    context.expect(valid_presentation.texture.y_flip, "valid texture viewport should preserve texture y-flip");
    context.expect(viewport.state().width == 640u, "valid texture viewport should store surface width");
    context.expect(viewport.state().height == 360u, "valid texture viewport should store surface height");
    context.expect(viewport.state().y_flip, "valid texture viewport should mirror texture y-flip");

    const luna::TextureViewportPresentation& zero_width_presentation =
        viewport.sync(luna::editor::UVec2{.x = 0u, .y = 360u});
    context.expect(!zero_width_presentation.presentable,
                   "texture viewport with zero framebuffer width should not be presentable");
    context.expect(zero_width_presentation.texture.valid(),
                   "texture viewport should keep texture when framebuffer size is zero");

    viewport.clearTexture();
    context.expect(!viewport.presentation().presentable, "cleared texture viewport should not be presentable");
    context.expect(!viewport.presentation().texture.valid(), "cleared texture viewport should clear texture");
}

} // namespace

int main()
{
    TestContext context;
    testViewportSurfaceState(context);
    testTextureViewportPresentation(context);
    return context.result();
}
