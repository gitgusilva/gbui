// The slider's geometry, and the one thing about it that used to be wrong.
#include "gbui/widgets/slider.hpp"

#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

/** Builds one slider inside a container of the given direction and reports the
 *  rectangle it ended up with. */
Rect sliderIn(Direction direction, const SliderOptions& options = {}) {
    Theme theme = Theme::dark();
    Interaction input;
    Arena arena;
    Ui ui(arena);
    {
        Style parent;
        parent.direction = direction;
        auto root = ui.scope(parent);
        (void)slider(ui, input, "s", 0.5, options);
        (void)root;
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 400, 300}, context);
    input.update(arena, ui.root(), InputFrame{});
    return input.frameOf("s");
}

}  // namespace

TEST("a slider fills the row it is in") {
    const Rect frame = sliderIn(Direction::Row);
    CHECK_NEAR(frame.width, 400.0f);
    CHECK_NEAR(frame.height, 20.0f);
}

TEST("a slider in a column does not eat the height") {
    // `grow` is main-axis and the component cannot know which way its parent
    // points, so in a column it used to take all the leftover height and leave
    // everything above it pinned to the top of the card. It is clamped now.
    const Rect frame = sliderIn(Direction::Column);
    CHECK_NEAR(frame.height, 20.0f);
    // Still stretched across the column, which is the cross axis and what
    // `Align::Stretch` was always going to do.
    CHECK_NEAR(frame.width, 400.0f);
}

TEST("a taller slider is allowed to be taller") {
    const Rect frame = sliderIn(Direction::Column, {.height = 44.0f});
    CHECK_NEAR(frame.height, 44.0f);
}
