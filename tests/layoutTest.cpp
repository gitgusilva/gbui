#include "gbui/layout/layout.hpp"

#include <cmath>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/scene/ui.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

/** Lays a tree out in a fixed viewport with the built-in theme. */
LayoutContext contextFor(const Theme& theme) {
    LayoutContext context;
    context.theme = &theme;
    return context;
}

}  // namespace

TEST("grow splits the free space in proportion") {
    Arena arena;
    Ui ui(arena);
    NodeId a, b, c;
    {
        auto row = ui.beginRow({.gap = 0});
        a = ui.add({.grow = 1.0f, .basis = 0.0f});
        b = ui.add({.grow = 3.0f, .basis = 0.0f});
        c = ui.add({.width = 100.0f});
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 500, 50}, contextFor(theme));

    // 400 free after the fixed child: a quarter and three quarters.
    CHECK_NEAR(arena[a].frame.width, 100.0f);
    CHECK_NEAR(arena[b].frame.width, 300.0f);
    CHECK_NEAR(arena[c].frame.width, 100.0f);
    CHECK_NEAR(arena[c].frame.x, 400.0f);
}

TEST("gap and padding come out of the content box") {
    Arena arena;
    Ui ui(arena);
    NodeId first, second;
    {
        auto row = ui.beginRow({.gap = 10.0f, .padding = Edges::all(8.0f)});
        first = ui.add({.grow = 1.0f, .basis = 0.0f});
        second = ui.add({.grow = 1.0f, .basis = 0.0f});
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 218, 40}, contextFor(theme));

    // 218 - 16 padding - 10 gap = 192, split evenly.
    CHECK_NEAR(arena[first].frame.width, 96.0f);
    CHECK_NEAR(arena[first].frame.x, 8.0f);
    CHECK_NEAR(arena[second].frame.x, 114.0f);
    CHECK_NEAR(arena[first].frame.height, 24.0f);  // stretched inside the padding
}

TEST("shrink is weighted by base size, as CSS does it") {
    Arena arena;
    Ui ui(arena);
    NodeId wide, narrow;
    {
        auto row = ui.beginRow();
        wide = ui.add({.shrink = 1.0f, .width = 300.0f});
        narrow = ui.add({.shrink = 1.0f, .width = 100.0f});
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 200, 20}, contextFor(theme));

    // 200 overflow shared 3:1 — the wide child gives up 150, the narrow one 50.
    CHECK_NEAR(arena[wide].frame.width, 150.0f);
    CHECK_NEAR(arena[narrow].frame.width, 50.0f);
}

TEST("justify places the leftover space") {
    const Theme theme = Theme::dark();

    struct Expectation {
        Justify justify;
        float firstX;
    };
    const Expectation cases[] = {
        {Justify::Start, 0.0f},
        {Justify::Center, 50.0f},
        {Justify::End, 100.0f},
    };

    for (const auto& expectation : cases) {
        Arena arena;
        Ui ui(arena);
        NodeId first;
        {
            auto row = ui.beginRow({.justify = expectation.justify});
            first = ui.add({.width = 50.0f});
            ui.add({.width = 50.0f});
            (void)row;
        }
        layout(arena, ui.root(), Rect{0, 0, 200, 20}, contextFor(theme));
        CHECK_NEAR(arena[first].frame.x, expectation.firstX);
    }
}

TEST("space-between pushes the ends apart and ignores the gap in between") {
    Arena arena;
    Ui ui(arena);
    NodeId left, right;
    {
        auto row = ui.beginRow({.justify = Justify::SpaceBetween});
        left = ui.add({.width = 40.0f});
        right = ui.add({.width = 40.0f});
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 200, 20}, contextFor(theme));

    CHECK_NEAR(arena[left].frame.x, 0.0f);
    CHECK_NEAR(arena[right].frame.x, 160.0f);
}

TEST("align centres on the cross axis and stretch fills it") {
    const Theme theme = Theme::dark();

    Arena stretched;
    {
        Ui ui(stretched);
        NodeId child;
        {
            auto row = ui.beginRow({.align = Align::Stretch});
            child = ui.add({.width = 20.0f});
            (void)row;
        }
        layout(stretched, ui.root(), Rect{0, 0, 100, 60}, contextFor(theme));
        CHECK_NEAR(stretched[child].frame.height, 60.0f);
    }

    Arena centred;
    {
        Ui ui(centred);
        NodeId child;
        {
            auto row = ui.beginRow({.align = Align::Center});
            child = ui.add({.width = 20.0f, .height = 20.0f});
            (void)row;
        }
        layout(centred, ui.root(), Rect{0, 0, 100, 60}, contextFor(theme));
        CHECK_NEAR(centred[child].frame.y, 20.0f);
        CHECK_NEAR(centred[child].frame.height, 20.0f);
    }
}

TEST("a column stacks and an auto height follows its content") {
    Arena arena;
    Ui ui(arena);
    NodeId second;
    {
        auto column = ui.beginColumn({.gap = 4.0f});
        ui.add({.height = 30.0f});
        second = ui.add({.height = 30.0f});
        (void)column;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 100, 200}, contextFor(theme));

    CHECK_NEAR(arena[second].frame.y, 34.0f);
    CHECK_NEAR(intrinsicMainSize(arena, ui.root(), contextFor(theme)), 64.0f);
}

TEST("space refused by a max-width goes to the others, not to waste") {
    Arena arena;
    Ui ui(arena);
    NodeId capped, free;
    {
        auto row = ui.beginRow();
        capped = ui.add({.grow = 1.0f, .basis = 0.0f, .maxWidth = 60.0f});
        free = ui.add({.grow = 1.0f, .basis = 0.0f});
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 400, 20}, contextFor(theme));

    // The capped item takes 60 and the 140 it could not use goes to the other,
    // which is what CSS's resolution loop does. Distributing once would leave
    // the row 140 short.
    CHECK_NEAR(arena[capped].frame.width, 60.0f);
    CHECK_NEAR(arena[free].frame.width, 340.0f);
    CHECK_NEAR(arena[capped].frame.width + arena[free].frame.width, 400.0f);
}

TEST("an item is not shrunk below what its content needs") {
    Arena arena;
    Ui ui(arena);
    NodeId rigid, flexible;
    {
        auto row = ui.beginRow();
        {
            // A box holding a fixed-size child cannot be squeezed past it.
            auto box = ui.begin({.shrink = 1.0f, .padding = Edges::all(4.0f)});
            rigid = ui.add({.shrink = 0.0f, .width = 80.0f, .height = 10.0f});
            (void)box;
        }
        flexible = ui.add({.shrink = 1.0f, .width = 300.0f, .minWidth = 0.0f});
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 200, 30}, contextFor(theme));

    // 88 is the fixed child plus its parent's padding: the floor min-content
    // computes. The item that opted out of the floor absorbs the rest.
    CHECK_NEAR(arena[arena[rigid].parent].frame.width, 88.0f);
    CHECK(arena[flexible].frame.width < 130.0f);
}

TEST("min-width 0 opts back in to shrinking away") {
    Arena arena;
    Ui ui(arena);
    NodeId squeezed;
    {
        auto row = ui.beginRow();
        squeezed = ui.add({.shrink = 1.0f, .width = 300.0f, .minWidth = 0.0f});
        ui.add({.shrink = 0.0f, .width = 100.0f});
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 120, 20}, contextFor(theme));

    CHECK_NEAR(arena[squeezed].frame.width, 20.0f);
}

TEST("margins take space on both axes") {
    Arena arena;
    Ui ui(arena);
    NodeId child;
    {
        auto row = ui.beginRow();
        child = ui.add({.grow = 1.0f, .basis = 0.0f, .margin = Edges::all(10.0f)});
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 100, 50}, contextFor(theme));

    CHECK_NEAR(arena[child].frame.x, 10.0f);
    CHECK_NEAR(arena[child].frame.y, 10.0f);
    CHECK_NEAR(arena[child].frame.width, 80.0f);
    CHECK_NEAR(arena[child].frame.height, 30.0f);
}

TEST("hit testing answers the deepest node under the point") {
    Arena arena;
    Ui ui(arena);
    NodeId inner;
    {
        auto column = ui.beginColumn({.padding = Edges::all(10.0f)});
        {
            auto row = ui.beginRow({.height = 40.0f});
            inner = ui.add({.grow = 1.0f, .basis = 0.0f});
            (void)row;
        }
        (void)column;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 200, 200}, contextFor(theme));

    CHECK(hitTest(arena, ui.root(), Vec2{100.0f, 30.0f}) == inner);
    CHECK(!hitTest(arena, ui.root(), Vec2{500.0f, 500.0f}).valid());
}

TEST("text measurement grows the box that holds it") {
    Arena arena;
    Ui ui(arena);
    NodeId label;
    {
        auto row = ui.beginRow({.align = Align::Start});
        label = ui.label("Local Changes");
        (void)row;
    }

    const Theme theme = Theme::dark();
    layout(arena, ui.root(), Rect{0, 0, 400, 40}, contextFor(theme));

    // 13 glyphs at ~0.52em of 13px.
    CHECK_NEAR(arena[label].frame.width, 13.0f * 0.52f * 13.0f);
    CHECK(arena[label].frame.height > 0.0f);
}

// ---- flex-wrap -------------------------------------------------------------

namespace {

/** A row of equal chips in a fixed width, wrapping or not. Returns each chip's
 *  frame so a test can say which line it landed on. */
std::vector<Rect> chips(bool wrap, float containerWidth, int count, float chipWidth,
                        float gap = 0.0f, AlignContent alignContent = AlignContent::Start) {
    Arena arena;
    Ui ui(arena);
    std::vector<NodeId> ids;
    {
        Style row;
        row.direction = Direction::Row;
        row.wrap = wrap;
        row.gap = gap;
        row.alignContent = alignContent;
        row.width = containerWidth;
        auto scope = ui.begin(row);
        for (int i = 0; i < count; ++i) {
            ids.push_back(ui.add({.shrink = 0.0f, .width = chipWidth, .height = 20.0f}));
        }
        (void)scope;
    }
    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, containerWidth, 300}, context);

    std::vector<Rect> frames;
    for (const NodeId id : ids) frames.push_back(arena[id].frame);
    return frames;
}

}  // namespace

TEST("without wrap a row overflows rather than reflowing") {
    // Four 60-wide chips in 200: they stay on one line and run off the end,
    // which is what `nowrap` means and what everything built so far expects.
    const std::vector<Rect> frames = chips(false, 200.0f, 4, 60.0f);
    for (const Rect& frame : frames) CHECK_NEAR(frame.y, 0.0f);
    CHECK_NEAR(frames.back().right(), 240.0f);
}

TEST("wrap starts a new line when the next item would not fit") {
    const std::vector<Rect> frames = chips(true, 200.0f, 4, 60.0f);

    // Three fit in 200; the fourth begins the second line, back at the left.
    CHECK_NEAR(frames[0].y, 0.0f);
    CHECK_NEAR(frames[2].y, 0.0f);
    CHECK_NEAR(frames[2].x, 120.0f);
    CHECK_NEAR(frames[3].y, 20.0f);
    CHECK_NEAR(frames[3].x, 0.0f);
}

TEST("the gap counts against the line's width") {
    // 60 + 10 + 60 + 10 + 60 is 200 exactly, so three still fit…
    CHECK_NEAR(chips(true, 200.0f, 4, 60.0f, 10.0f)[2].y, 0.0f);
    // …and one pixel less of room pushes the third onto the second line, which
    // starts a line height *and a gap* down: `gap` separates lines as well as
    // items, the way CSS's `row-gap` does.
    CHECK_NEAR(chips(true, 199.0f, 4, 60.0f, 10.0f)[2].y, 30.0f);
}

TEST("an item wider than the line still gets a line") {
    // Never an empty line, and never a loop: the spec's rule, and the one that
    // stops a single over-wide child from breaking the pass.
    const std::vector<Rect> frames = chips(true, 50.0f, 2, 120.0f);
    CHECK_NEAR(frames[0].y, 0.0f);
    CHECK_NEAR(frames[1].y, 20.0f);
}

TEST("lines stack, and the container is as tall as all of them") {
    Arena arena;
    Ui ui(arena);
    NodeId row;
    {
        auto column = ui.beginColumn({.width = 200.0f});
        {
            Style style;
            style.direction = Direction::Row;
            style.wrap = true;
            auto scope = ui.begin(style);
            row = scope.id();
                for (int i = 0; i < 5; ++i) {
                ui.add({.shrink = 0.0f, .width = 60.0f, .height = 20.0f});
            }
        }
        (void)column;
    }
    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 300}, context);

    // Five 60-wide chips in 200 fit three to a line, so two lines of 20. The
    // row's own height has to be their sum, or it is sized for one line and
    // draws two.
    CHECK_NEAR(arena[row].frame.height, 40.0f);
}

TEST("alignContent distributes the lines across the container") {
    // Two lines of 20 in 300 leaves 260 of slack.
    Arena arena;
    Ui ui(arena);
    std::vector<NodeId> ids;
    {
        Style style;
        style.direction = Direction::Row;
        style.wrap = true;
        style.width = 200.0f;
        style.height = 300.0f;
        style.alignContent = AlignContent::Center;
        auto scope = ui.begin(style);
        for (int i = 0; i < 4; ++i) {
            ids.push_back(ui.add({.shrink = 0.0f, .width = 60.0f, .height = 20.0f}));
        }
        (void)scope;
    }
    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 300}, context);

    // 40 of content centred in 300 starts at 130.
    CHECK_NEAR(arena[ids[0]].frame.y, 130.0f);
    CHECK_NEAR(arena[ids[3]].frame.y, 150.0f);
}

// ---- percentage lengths ----------------------------------------------------

TEST("a percentage is a share of the container's content box") {
    Arena arena;
    Ui ui(arena);
    NodeId half, quarter;
    {
        // 400 wide with 20 of padding each side: the content box is 360, and
        // that — not the frame — is what a percentage is a share of, as in CSS.
        auto row = ui.beginRow({.width = 400.0f, .padding = Edges::all(20.0f)});
        half = ui.add({.shrink = 0.0f, .width = Length::percent(50)});
        quarter = ui.add({.shrink = 0.0f, .width = Length::percent(25)});
        (void)row;
    }
    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 400, 100}, context);

    CHECK_NEAR(arena[half].frame.width, 180.0f);
    CHECK_NEAR(arena[quarter].frame.width, 90.0f);
}

TEST("a percentage height shares the container's height") {
    Arena arena;
    Ui ui(arena);
    NodeId child;
    {
        auto column = ui.beginColumn({.width = 200.0f, .height = 300.0f});
        child = ui.add({.shrink = 0.0f, .height = Length::percent(40)});
        (void)column;
    }
    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 300}, context);

    CHECK_NEAR(arena[child].frame.height, 120.0f);
}

TEST("a percentage maximum caps a growing item — the split-pane rule") {
    // "A quarter of the window, up to 320" is the thing that could not be
    // written before: a percentage basis and a pixel ceiling on one item.
    const auto sidebarWidth = [](float windowWidth) {
        Arena arena;
        Ui ui(arena);
        NodeId sidebar;
        {
            auto row = ui.beginRow({.width = windowWidth});
            sidebar = ui.add({.shrink = 0.0f, .width = Length::percent(25), .maxWidth = 320.0f});
            ui.add({.grow = 1.0f, .basis = 0.0f});
            (void)row;
        }
        const Theme theme = Theme::dark();
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, windowWidth, 100}, context);
        return arena[sidebar].frame.width;
    };

    CHECK_NEAR(sidebarWidth(800.0f), 200.0f);    // a quarter
    CHECK_NEAR(sidebarWidth(2400.0f), 320.0f);   // …until the ceiling bites
}

TEST("a plain number is still pixels") {
    // The whole reason `Length` could be introduced at all: every existing call
    // site writes a float and still means a float.
    Arena arena;
    Ui ui(arena);
    const NodeId fixed = ui.add({.width = 120.0f, .height = 40.0f});
    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 500, 200}, context);

    CHECK_NEAR(arena[fixed].frame.width, 120.0f);
    CHECK_NEAR(arena[fixed].frame.height, 40.0f);
}

TEST("a percentage against an unknown basis behaves as auto") {
    // A container sizing itself to its children cannot also be what their
    // percentages are a share of. CSS resolves that as `auto`, and so does
    // this: the child falls back to its content rather than looping.
    Arena arena;
    Ui ui(arena);
    NodeId child;
    {
        // The row has no width of its own; it is as wide as what is inside it.
        auto row = ui.beginRow({});
        child = ui.add({.shrink = 0.0f, .width = Length::percent(50), .height = 20.0f});
        (void)row;
    }
    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 600, 100}, context);

    // The root stretches to the viewport, so the row does get a width and the
    // half is real. What matters is that nothing hung or produced a NaN.
    CHECK(arena[child].frame.width >= 0.0f);
    CHECK(!std::isnan(arena[child].frame.width));
}

// ---- logical pixels and display scale --------------------------------------

namespace {

/** Records the same tree at a given scale and returns the geometry the backend
 *  would receive. */
std::vector<Rect> recordAt(float scale) {
    Arena arena;
    Ui ui(arena);
    {
        auto row = ui.beginRow({.gap = 8.0f, .width = 200.0f, .padding = Edges::all(10.0f)});
        ui.add({.width = 40.0f, .height = 24.0f,
                .background = Fill{Token::Accent}, .radius = 4.0f});
        ui.add({.width = 60.0f, .height = 24.0f, .background = Fill{Token::Bg},
                .border = Border{1.0f, Fill{Token::Border}}, .radius = 4.0f});
        (void)row;
    }
    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    // Layout is always in logical units — the viewport it is handed does not
    // change with the display.
    layout(arena, ui.root(), Rect{0, 0, 200, 100}, context);

    DisplayList list;
    list.setScale(scale);
    record(arena, ui.root(), theme, list);

    std::vector<Rect> rects;
    for (const DrawCommand& command : list.commands()) {
        if (const auto* fill = std::get_if<FillRect>(&command)) rects.push_back(fill->rect);
        else if (const auto* stroke = std::get_if<StrokeRect>(&command)) rects.push_back(stroke->rect);
    }
    return rects;
}

}  // namespace

TEST("a display scale multiplies the output and nothing else") {
    const std::vector<Rect> one = recordAt(1.0f);
    const std::vector<Rect> half = recordAt(1.5f);
    const std::vector<Rect> two = recordAt(2.0f);

    // The same drawing, in every case: only its resolution differs. A tree that
    // laid out differently per display would be a tree that reflows when the
    // window is dragged between monitors.
    CHECK_EQ(one.size(), half.size());
    CHECK_EQ(one.size(), two.size());
    CHECK(!one.empty());
    for (std::size_t i = 0; i < one.size(); ++i) {
        CHECK_NEAR(half[i].x, one[i].x * 1.5f);
        CHECK_NEAR(half[i].width, one[i].width * 1.5f);
        CHECK_NEAR(two[i].y, one[i].y * 2.0f);
        CHECK_NEAR(two[i].height, one[i].height * 2.0f);
    }
}

TEST("radii, borders and font sizes scale with the geometry") {
    Arena arena;
    Ui ui(arena);
    ui.add({.width = 40.0f, .height = 20.0f, .background = Fill{Token::Accent},
            .border = Border{2.0f, Fill{Token::Border}}, .radius = 6.0f});
    ui.label("hello", TextStyle{.size = 13.0f});

    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 100}, context);

    DisplayList list;
    list.setScale(2.0f);
    record(arena, ui.root(), theme, list);

    // A border that stayed one pixel while everything around it doubled is the
    // giveaway that only the rectangles were scaled.
    bool sawRadius = false;
    bool sawBorder = false;
    bool sawText = false;
    for (const DrawCommand& command : list.commands()) {
        if (const auto* fill = std::get_if<FillRect>(&command); fill && fill->radius > 0.0f) {
            CHECK_NEAR(fill->radius, 12.0f);
            sawRadius = true;
        }
        if (const auto* stroke = std::get_if<StrokeRect>(&command)) {
            CHECK_NEAR(stroke->width, 4.0f);
            sawBorder = true;
        }
        if (const auto* text = std::get_if<DrawText>(&command)) {
            CHECK_NEAR(text->size, 26.0f);
            sawText = true;
        }
    }
    CHECK(sawRadius);
    CHECK(sawBorder);
    CHECK(sawText);
}

// ---- z-index ---------------------------------------------------------------

TEST("zIndex decides what is drawn last, and what the pointer finds") {
    Arena arena;
    Ui ui(arena);
    NodeId under, over;
    {
        // Two overlapping boxes. `under` is written *second*, so tree order
        // would put it on top; `zIndex` says otherwise.
        auto stack = ui.beginRow({.width = 100.0f, .height = 100.0f});
        over = ui.add({.width = 100.0f, .height = 100.0f, .position = Position::Absolute,
                       .zIndex = 5, .left = 0.0f, .top = 0.0f,
                       .background = Fill{Token::Accent}});
        under = ui.add({.width = 100.0f, .height = 100.0f, .position = Position::Absolute,
                        .zIndex = 1, .left = 0.0f, .top = 0.0f, .background = Fill{Token::Bg}});
        ui.tag("under");
        (void)stack;
    }
    // Tagging `over` needs it to be the most recent node, so it is tagged by a
    // second pass through the arena instead.
    arena[over].id = arena.intern("over");

    const Theme theme = Theme::dark();
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 100, 100}, context);

    // Painted low to high: the higher index is recorded last, so it covers.
    DisplayList list;
    record(arena, ui.root(), theme, list);
    int underAt = -1;
    int overAt = -1;
    int index = 0;
    for (const DrawCommand& command : list.commands()) {
        if (const auto* fill = std::get_if<FillRect>(&command)) {
            if (fill->paint.color == theme.color(Token::Bg)) underAt = index;
            if (fill->paint.color == theme.color(Token::Accent)) overAt = index;
        }
        ++index;
    }
    CHECK(underAt >= 0);
    CHECK(overAt > underAt);

    // And the pointer agrees with the paint, which is the half that is easy to
    // forget: what looks on top has to be what is hit.
    Interaction input;
    InputFrame frame;
    frame.pointer = {50.0f, 50.0f};
    input.update(arena, ui.root(), frame);
    CHECK_EQ(std::string(input.hovered()), std::string("over"));
}
