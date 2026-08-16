// Where the divider goes, and what stops the panes squeezing each other out.
#include <string>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/containers.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

TextMetrics measureFixed(std::string_view text, const TextStyle&, const Typography&, float) {
    std::size_t characters = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++characters;
    }
    return {static_cast<float>(characters) * 8.0f, 14.0f, 11.0f};
}

struct Split {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    float position = 0.5f;
    SplitPaneOptions options{};
    SplitPaneResult result{};
    Rect window{0, 0, 600, 300};
    /** Read straight out of the arena, so the *first* frame can be asserted. */
    Rect leading{};
    Rect trailing{};

    void frame(const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        ui.setMeasure(&measureFixed, theme.typography());
        NodeId root;
        {
            auto column = ui.column({.width = window.width, .height = window.height});
            result = splitPane(
                ui, input, "s", position,
                [](Ui& inner) { inner.tag("s.leading.fill"); },
                [](Ui& inner) { inner.tag("s.trailing.fill"); }, options);
            root = column.id();
        }
        LayoutContext context;
        context.theme = &theme;
        context.measure = &measureFixed;
        layout(arena, root, window, context);

        leading = trailing = Rect{};
        for (std::size_t i = 0; i < arena.size(); ++i) {
            const NodeId id{static_cast<std::uint32_t>(i)};
            // `tag` names the last node built, and a callback that builds
            // nothing leaves that as the pane's own scope — so the tag lands on
            // the pane, which is the rectangle these cases are about.
            if (arena[id].id == "s.leading.fill") leading = arena[id].frame;
            if (arena[id].id == "s.trailing.fill") trailing = arena[id].frame;
        }
        if (result.changed) position = result.position;
    }

    void settle() {
        frame();
        frame();
        frame();
    }

    void key(Key which, bool shift = false) {
        input.focus("s.divider", FocusSource::Keyboard);
        InputFrame event;
        event.keys.push_back(KeyEvent{which, Modifiers{.shift = shift}});
        frame(event);
    }
};

}  // namespace

/**
 * The claim the header makes. A split placed from a measured width lags a
 * resize by a frame and jumps while the window is dragged; a ratio is resolved
 * during layout. This is the *first* frame.
 */
TEST("the panes are in the right places on the very first frame") {
    Split split;
    split.position = 0.25f;
    split.frame();

    const float usable = 600.0f - split.options.dividerWidth;
    CHECK_NEAR(split.leading.width, usable * 0.25);
    CHECK_NEAR(split.trailing.width, usable * 0.75);
    // And they meet, with only the divider between them.
    CHECK_NEAR(split.trailing.x - split.leading.right(), split.options.dividerWidth);
}

TEST("the minimums are the layout's, so they hold when the window shrinks too") {
    // A clamp in the drag handler misses the second half of this entirely:
    // nobody dragged anything, the container simply got smaller.
    Split split;
    split.options.minLeading = 200.0f;
    split.options.minTrailing = 200.0f;
    split.position = 0.5f;
    split.frame();

    const float usable = 600.0f - split.options.dividerWidth;
    CHECK_NEAR(split.leading.width, usable * 0.5);

    // Asking for nineteen twentieths does not get it: the trailing pane keeps
    // its floor and the leading one takes what is left.
    split.position = 0.95f;
    split.frame();
    CHECK_NEAR(split.trailing.width, 200.0);
    CHECK_NEAR(split.leading.width, usable - 200.0);

    // And the same when the *window* shrinks under a split that was fine.
    split.position = 0.5f;
    split.window = Rect{0, 0, 460, 300};
    split.frame();
    split.position = 0.9f;
    split.frame();
    CHECK_NEAR(split.trailing.width, 200.0);
    CHECK(split.leading.width >= 200.0f);
}

TEST("dragging the divider moves it, and keeps working past the edge") {
    Split split;
    split.settle();

    const Rect grip = split.input.frameOf("s.divider");
    CHECK(grip.width > 0.0f);

    InputFrame down;
    down.pointer = {grip.x + grip.width / 2.0f, 150.0f};
    down.pointerDown = true;
    split.frame(down);

    InputFrame moved;
    moved.pointer = {450.0f, 150.0f};
    moved.pointerDown = true;
    split.frame(moved);
    CHECK_NEAR(split.position, 0.75);

    // Off the right of the window entirely, and the divider goes as far as it
    // is allowed rather than stopping where the pointer left.
    InputFrame past;
    past.pointer = {2000.0f, 150.0f};
    past.pointerDown = true;
    split.frame(past);
    CHECK_NEAR(split.position, 1.0);
}

/** ARIA's window splitter, and the half everybody forgets: a split only
 *  draggable with a pointer is a layout most people cannot change. */
TEST("the divider answers the arrow keys") {
    Split split;
    split.settle();

    split.key(Key::Right);
    CHECK_NEAR(split.position, 0.52);
    split.key(Key::Left);
    CHECK_NEAR(split.position, 0.50);
    // Shift is the coarse gesture, five times over.
    split.key(Key::Right, true);
    CHECK_NEAR(split.position, 0.60);
    split.key(Key::Home);
    CHECK_NEAR(split.position, 0.0);
    split.key(Key::End);
    CHECK_NEAR(split.position, 1.0);
    split.key(Key::Right);
    CHECK_NEAR(split.position, 1.0);   // and no further
}

TEST("a vertical split moves on the other axis") {
    Split split;
    split.options.orientation = SplitOrientation::Vertical;
    split.options.minLeading = 40.0f;
    split.options.minTrailing = 40.0f;
    split.position = 0.25f;
    split.frame();

    const float usable = 300.0f - split.options.dividerWidth;
    CHECK_NEAR(split.leading.height, usable * 0.25);
    CHECK_NEAR(split.trailing.height, usable * 0.75);

    split.settle();
    split.key(Key::Down);
    CHECK_NEAR(split.position, 0.27);
}

TEST("the divider is the only thing in a split that takes the keyboard") {
    // Two panes full of controls already have their own; a container that also
    // took one would be a Tab press between every pane and its contents.
    Split split;
    split.settle();
    for (std::size_t i = 0; i < split.arena.size(); ++i) {
        const Node& node = split.arena[NodeId{static_cast<std::uint32_t>(i)}];
        if (node.focusable && !node.id.empty()) CHECK(node.id == "s.divider");
    }
}
