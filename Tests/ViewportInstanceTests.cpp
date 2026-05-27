#include "Viewport/TextureViewportInstance.h"
#include "Viewport/ViewportInteraction.h"
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
        .id = 0x12'34u,
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

void testViewportInteractionTopmostHover(TestContext& context)
{
    luna::ViewportInteractionTracker tracker;
    tracker.beginFrame();

    const luna::ViewportInteractionInput base_input{
        .rect =
            luna::ViewportSurfaceRect{
                .min = luna::editor::Vec2{.x = 0.0f, .y = 0.0f},
                .max = luna::editor::Vec2{.x = 100.0f, .y = 100.0f},
            },
        .hovered = true,
        .clicked = true,
        .left_mouse_down = true,
    };

    tracker.recordSurface(10u, "base", base_input);
    context.expect(tracker.isHovered(10u), "first hovered viewport should own hover");
    context.expect(tracker.isClicked(10u), "first hovered viewport should own click");

    tracker.recordSurface(20u,
                          "overlay",
                          luna::ViewportInteractionInput{
                              .rect =
                                  luna::ViewportSurfaceRect{
                                      .min = luna::editor::Vec2{.x = 10.0f, .y = 10.0f},
                                      .max = luna::editor::Vec2{.x = 80.0f, .y = 80.0f},
                                  },
                              .hovered = true,
                              .clicked = true,
                              .left_mouse_down = true,
                          });

    context.expect(!tracker.isHovered(10u), "later hovered viewport should block lower hover");
    context.expect(!tracker.isClicked(10u), "later hovered viewport should block lower click");
    context.expect(tracker.isHovered(20u), "later hovered viewport should own hover");
    context.expect(tracker.isClicked(20u), "later hovered viewport should own click");

    tracker.beginFrame();
    context.expect(!tracker.isHovered(20u), "beginFrame should clear transient hover");
    context.expect(!tracker.isClicked(20u), "beginFrame should clear transient click");
}

void testViewportInteractionCapture(TestContext& context)
{
    luna::ViewportInteractionTracker tracker;
    tracker.recordSurface(30u, "viewport", luna::ViewportInteractionInput{});
    context.expect(!tracker.allowsInput(30u), "non-hovered viewport should not accept input");

    tracker.setMouseCapture(30u, true);
    context.expect(tracker.hasMouseCapture(30u), "captured viewport should expose capture");
    context.expect(tracker.allowsInput(30u), "captured viewport should accept input without hover");

    tracker.beginFrame();
    context.expect(tracker.allowsInput(30u), "beginFrame should preserve capture");

    tracker.setMouseCapture(30u, false);
    context.expect(!tracker.hasMouseCapture(30u), "released viewport should clear capture");
    context.expect(!tracker.allowsInput(30u), "released non-hovered viewport should stop accepting input");
}

void testViewportInteractionCapturedDrag(TestContext& context)
{
    luna::ViewportInteractionTracker tracker;
    tracker.recordSurface(31u,
                          "viewport",
                          luna::ViewportInteractionInput{
                              .mouse_delta = luna::editor::Vec2{.x = 0.0f, .y = 0.0f},
                              .hovered = true,
                              .clicked = true,
                              .left_mouse_down = true,
                          });

    tracker.recordSurface(31u,
                          "viewport",
                          luna::ViewportInteractionInput{
                              .mouse_delta = luna::editor::Vec2{.x = 7.0f, .y = -3.0f},
                              .left_mouse_down = true,
                              .dragging = true,
                          });

    const luna::ViewportInteractionState* captured_state = tracker.find(31u);
    context.expect(captured_state != nullptr && captured_state->dragging,
                   "captured viewport should keep dragging after hover leaves");
    context.expect(captured_state != nullptr && captured_state->mouse_drag_delta.x == 7.0f &&
                       captured_state->mouse_drag_delta.y == -3.0f,
                   "captured viewport should expose drag delta after hover leaves");

    tracker.recordSurface(31u,
                          "viewport",
                          luna::ViewportInteractionInput{
                              .left_mouse_released = true,
                          });
    context.expect(!tracker.hasMouseCapture(31u), "mouse release should clear viewport capture");
}

void testViewportInteractionOwnerCleanup(TestContext& context)
{
    luna::ViewportInteractionTracker tracker;
    tracker.recordSurface(40u, "owner.a", luna::ViewportInteractionInput{.hovered = true});
    tracker.recordSurface(41u, "owner.b", luna::ViewportInteractionInput{.hovered = true});
    tracker.setMouseCapture(40u, true);

    tracker.clearOwner("owner.a");
    context.expect(tracker.find(40u) == nullptr, "owner cleanup should remove matching viewport interaction");
    context.expect(tracker.find(41u) != nullptr, "owner cleanup should keep other owner interactions");
    context.expect(tracker.capturedViewport() != 40u, "owner cleanup should clear matching capture");
}

} // namespace

int main()
{
    TestContext context;
    testViewportSurfaceState(context);
    testTextureViewportPresentation(context);
    testViewportInteractionTopmostHover(context);
    testViewportInteractionCapture(context);
    testViewportInteractionCapturedDrag(context);
    testViewportInteractionOwnerCleanup(context);
    return context.result();
}
