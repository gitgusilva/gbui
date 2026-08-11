#include "gbui/widgets/containers.hpp"

#include <string>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

/** Builds a view holding taller-than-it-is content, twice, so the second pass
 *  has last frame's geometry the way a real loop does. */
struct View {
    ScrollState state;
    Rect viewport{};
    Rect content{};
    bool barDrawn = false;
};

View run(const ScrollOptions& options, float wheel = 0.0f, int rows = 20) {
    Theme theme = Theme::dark();
    Interaction input;
    View view;

    for (int pass = 0; pass < 3; ++pass) {
        Arena arena;
        Ui ui(arena);
        {
            auto root = ui.beginColumn({.width = 200.0f, .height = 400.0f});
            {
                auto scroll = beginScroll(ui, input, "view", view.state, options);
                for (int i = 0; i < rows; ++i) ui.add({.width = 300.0f, .height = 20.0f});
                (void)scroll;
            }
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, 200, 400}, context);

        InputFrame frame;
        frame.pointer = {50.0f, 50.0f};
        frame.wheel = pass == 1 ? wheel : 0.0f;
        input.update(arena, ui.root(), frame);

        view.viewport = input.frameOf("view");
        view.content = input.frameOf("view.content");
        view.barDrawn = !input.frameOf("view.thumb").empty();
    }
    return view;
}

}  // namespace

TEST("a view told not to scroll clips instead of moving") {
    const View view = run({.axis = ScrollAxis::None, .grow = 0.0f, .maxHeight = 120.0f}, -5.0f);

    // The wheel turned over it and nothing moved, and no bar was drawn — but
    // the content still keeps its own height and is still clipped, which is the
    // difference between this and having no container.
    CHECK_NEAR(view.state.offset, 0.0f);
    CHECK(!view.barDrawn);
    CHECK(view.content.height > view.viewport.height);
}

TEST("the wheel moves a scrolling view") {
    // Bounded, so there is something to scroll: unbounded it would simply be as
    // tall as its content.
    const View view = run({.axis = ScrollAxis::Vertical, .grow = 0.0f, .maxHeight = 120.0f}, -5.0f);
    CHECK(view.state.offset > 0.0f);
    CHECK(view.barDrawn);
}

TEST("a maximum height turns a tall view into a scrolling one") {
    // No ceiling: it takes its content's height, all 400 px of it, and has
    // nothing to scroll.
    const View unbounded = run({.axis = ScrollAxis::Vertical, .grow = 0.0f});
    CHECK_NEAR(unbounded.viewport.height, 400.0f);
    CHECK(!unbounded.state.scrollable());

    // With a ceiling it stops there, and what does not fit scrolls.
    const View bounded = run({.axis = ScrollAxis::Vertical, .grow = 0.0f, .maxHeight = 120.0f});
    CHECK_NEAR(bounded.viewport.height, 120.0f);
    CHECK(bounded.state.maxOffset() > 0.0f);
}

TEST("a minimum height keeps a short view from collapsing") {
    const View view = run({.axis = ScrollAxis::Vertical, .grow = 0.0f, .minHeight = 80.0f}, 0.0f, 1);
    CHECK_NEAR(view.viewport.height, 80.0f);
    CHECK(!view.state.scrollable());
}

TEST("the axis a view does not scroll is pinned to it") {
    // 300-wide rows in a 200-wide view that scrolls vertically: the content is
    // held to the viewport's width rather than running off the side where
    // nothing could reach it.
    const View view = run({.axis = ScrollAxis::Vertical});
    CHECK(view.content.width <= view.viewport.width + 0.01f);
}

namespace {

/** An outer view holding an inner one, with the pointer placed inside the
 *  inner. Both are tall enough to scroll, so the only thing deciding which one
 *  moves is where the wheel is routed. */
struct Nested {
    ScrollState outer;
    ScrollState inner;
    std::string wheelTarget;
};

Nested runNested(Vec2 pointer, float wheel) {
    Theme theme = Theme::dark();
    Interaction input;
    Nested nested;

    for (int pass = 0; pass < 3; ++pass) {
        Arena arena;
        Ui ui(arena);
        {
            auto root = ui.beginColumn({.width = 200.0f, .height = 400.0f});
            {
                auto page = beginScroll(ui, input, "page", nested.outer,
                                        {.axis = ScrollAxis::Vertical});
                ui.add({.width = 180.0f, .height = 100.0f});
                {
                    auto list = beginScroll(ui, input, "page.list", nested.inner,
                                            {.axis = ScrollAxis::Vertical, .grow = 0.0f,
                                             .maxHeight = 100.0f});
                    for (int i = 0; i < 20; ++i) ui.add({.width = 160.0f, .height = 20.0f});
                    (void)list;
                }
                ui.add({.width = 180.0f, .height = 600.0f});
                (void)page;
            }
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, 200, 400}, context);

        InputFrame frame;
        frame.pointer = pointer;
        frame.wheel = pass == 1 ? wheel : 0.0f;
        input.update(arena, ui.root(), frame);
        // Read on the frame the wheel is actually delivered on. Reading it
        // afterwards reads a tree the scroll has already moved: the page slides
        // up, and whatever was below the pointer is now under it.
        if (pass == 1) nested.wheelTarget = std::string(input.wheelTarget());
    }
    return nested;
}

}  // namespace

/**
 * The regression this pins: every scroll used to react to "the pointer is
 * somewhere inside me", which is true of the page *and* of the list on it. One
 * wheel notch moved both, so an inner list scrolled at double speed and dragged
 * the page along with it.
 */
TEST("the wheel goes to the innermost view under the pointer") {
    // Inside the inner list, which starts 100px down.
    const Nested inner = runNested({100.0f, 150.0f}, -3.0f);
    CHECK(inner.wheelTarget == "page.list");
    CHECK(inner.inner.offset > 0.0f);
    CHECK_NEAR(inner.outer.offset, 0.0f);
}

TEST("the wheel falls through to the page when nothing inner is under it") {
    // Above the inner list, over the page's own filler.
    const Nested outer = runNested({100.0f, 40.0f}, -3.0f);
    CHECK(outer.wheelTarget == "page");
    CHECK(outer.outer.offset > 0.0f);
    CHECK_NEAR(outer.inner.offset, 0.0f);
}

/** Nothing scrollable under the pointer names nothing, so a widget that wants
 *  the wheel for something else — a chart zooming — can tell it is free. */
TEST("a pointer over nothing scrollable claims no wheel target") {
    const Nested none = runNested({-50.0f, -50.0f}, -3.0f);
    CHECK(none.wheelTarget.empty());
}

namespace {

/** A box whose columns scroll sideways, holding rows that scroll up and down —
 *  the shape a table has, and the one where the two wheels differ. */
struct BothWays {
    ScrollState across;
    ScrollState down;
    std::string wheelTarget;
    std::string wheelTargetX;
};

BothWays runBothWays(float wheel, bool shift) {
    Theme theme = Theme::dark();
    Interaction input;
    BothWays box;

    for (int pass = 0; pass < 3; ++pass) {
        Arena arena;
        Ui ui(arena);
        {
            auto root = ui.beginColumn({.width = 200.0f, .height = 200.0f});
            {
                auto sideways = beginScroll(ui, input, "across", box.across,
                                            {.direction = Direction::Column,
                                             .axis = ScrollAxis::Horizontal});
                {
                    // A definite width, the way a table's columns give it one.
                    // Without it the inner view is sized by its parent while
                    // the parent is sized by it, and the pair settles on zero.
                    auto wide = ui.beginColumn({.shrink = 0.0f, .width = 400.0f});
                    {
                        auto rows = beginScroll(ui, input, "across.rows", box.down,
                                                {.axis = ScrollAxis::Vertical,
                                                 .grow = 0.0f,
                                                 .maxHeight = 150.0f});
                        for (int i = 0; i < 30; ++i) ui.add({.height = 20.0f});
                        (void)rows;
                    }
                    (void)wide;
                }
                (void)sideways;
            }
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, 200, 200}, context);

        InputFrame frame;
        frame.pointer = {80.0f, 80.0f};
        frame.wheel = pass == 1 ? wheel : 0.0f;
        frame.modifiers.shift = shift;
        input.update(arena, ui.root(), frame);
        if (pass == 1) {
            box.wheelTarget = std::string(input.wheelTarget());
            box.wheelTargetX = std::string(input.wheelTargetX());
        }
    }
    return box;
}

}  // namespace

/** The two are resolved separately, because the nearest thing that scrolls at
 *  all is usually not the nearest thing that scrolls sideways. */
TEST("a nested pair reports one target for each wheel") {
    const BothWays box = runBothWays(0.0f, false);
    CHECK(box.wheelTarget == "across.rows");
    CHECK(box.wheelTargetX == "across");
}

TEST("a plain wheel moves the rows and not the columns") {
    const BothWays box = runBothWays(-3.0f, false);
    CHECK(box.down.offset > 0.0f);
    CHECK_NEAR(box.across.offset, 0.0f);
}

TEST("shift and the wheel move the columns and not the rows") {
    const BothWays box = runBothWays(-3.0f, true);
    CHECK(box.across.offset > 0.0f);
    CHECK_NEAR(box.down.offset, 0.0f);
}

/** A horizontal view's content stops short of its own bar. Without that the
 *  content covers the strip the bar is drawn in and takes every press meant for
 *  it — a bar that can be seen and not used. */
TEST("a horizontal view leaves room for its own bar") {
    Theme theme = Theme::dark();
    Interaction input;
    ScrollState state;
    Rect viewport{};
    Rect content{};

    for (int pass = 0; pass < 3; ++pass) {
        Arena arena;
        Ui ui(arena);
        {
            auto root = ui.beginColumn({.width = 200.0f, .height = 120.0f});
            {
                auto view = beginScroll(ui, input, "wide", state,
                                        {.direction = Direction::Row,
                                         .axis = ScrollAxis::Horizontal});
                for (int i = 0; i < 10; ++i) ui.add({.width = 90.0f, .height = 40.0f});
                (void)view;
            }
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, 200, 120}, context);
        input.update(arena, ui.root(), InputFrame{});
        viewport = input.frameOf("wide");
        content = input.frameOf("wide.content");
    }
    if (viewport.height <= 0.0f) return;
    CHECK(state.scrollable());
    CHECK(content.height < viewport.height);
}
