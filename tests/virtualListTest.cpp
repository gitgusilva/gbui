#include "gbui/widgets/containers.hpp"

#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

constexpr float kRowHeight = 28.0f;
constexpr std::size_t kCommits = 50000;
const Rect kWindow{0, 0, 400, 280};

/** Builds the list twice, the way any real frame loop does: the first pass has
 *  no geometry to go on, the interaction records what it produced, and the
 *  second is the one under test. Returns the second pass's slice, and reports
 *  which indices were asked for and how large the arena ended up. */
struct Run {
    VirtualSlice slice;
    std::vector<std::size_t> built;
    std::size_t nodes = 0;
    float contentHeight = 0.0f;
};

Run build(ScrollState& state, const VirtualListOptions& options, int passes = 2) {
    Theme theme = Theme::dark();
    Interaction input;
    Run run;

    for (int pass = 0; pass < passes; ++pass) {
        Arena arena;
        Ui ui(arena);
        run.built.clear();
        {
            auto root = ui.beginColumn({.width = kWindow.width, .height = kWindow.height});
            run.slice = virtualList(ui, input, "list", state, options,
                                    [&](Ui&, std::size_t index) { run.built.push_back(index); });
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), kWindow, context);
        input.update(arena, ui.root(), InputFrame{});
        run.nodes = arena.size();
        run.contentHeight = input.frameOf("list.content").height;
    }
    return run;
}

}  // namespace

TEST("a virtual list builds the visible rows, not the list") {
    ScrollState state;
    const VirtualListOptions options{.count = kCommits, .rowHeight = kRowHeight};
    const Run run = build(state, options);

    // 280 px of viewport at 28 px a row is ten rows, plus overscan below.
    CHECK_EQ(run.slice.first, std::size_t{0});
    CHECK_EQ(run.slice.total, kCommits);
    CHECK(run.slice.count >= 10);
    CHECK(run.slice.count <= 14);
    CHECK_EQ(run.built.size(), run.slice.count);
    // The whole arena — viewport, bar, content, spacer and rows — stays in the
    // dozens. Building the list itself would be 50 000 nodes.
    CHECK(run.nodes < 40);
}

TEST("the content keeps the full height, so the scrollbar tells the truth") {
    ScrollState state;
    const VirtualListOptions options{.count = kCommits, .rowHeight = kRowHeight};
    const Run run = build(state, options);

    CHECK_NEAR(run.contentHeight, static_cast<float>(kCommits) * kRowHeight);
    CHECK_NEAR(state.contentSize, static_cast<float>(kCommits) * kRowHeight);
    CHECK_NEAR(state.viewportSize, kWindow.height);
    CHECK_NEAR(state.maxOffset(), static_cast<float>(kCommits) * kRowHeight - kWindow.height);
}

TEST("scrolling moves the slice, not the rows") {
    ScrollState state;
    const VirtualListOptions options{.count = kCommits, .rowHeight = kRowHeight};
    build(state, options);

    // A third of the way down a fifty-thousand row list.
    state.offset = 400000.0f;
    const Run run = build(state, options);

    const std::size_t expected = static_cast<std::size_t>(400000.0f / kRowHeight);
    CHECK_EQ(run.slice.first, expected - options.overscan);
    CHECK_EQ(run.built.front(), run.slice.first);
    CHECK_EQ(run.built.back(), run.slice.last() - 1);
    CHECK(run.nodes < 40);
}

TEST("the gap is counted once, wherever the slice starts") {
    ScrollState state;
    const VirtualListOptions options{.count = 100, .rowHeight = 20.0f, .gap = 4.0f};
    build(state, options);

    // 100 rows of 20 with 99 gaps of 4.
    const float total = 100.0f * 24.0f - 4.0f;
    Run run = build(state, options);
    CHECK_NEAR(run.contentHeight, total);

    // Scrolled into the middle, the two spacers plus the built rows still add
    // up to exactly the same height — which is what keeps a row where the
    // scrollbar says it is.
    state.offset = 1000.0f;
    run = build(state, options);
    CHECK_NEAR(run.contentHeight, total);
    CHECK_EQ(run.slice.first, static_cast<std::size_t>(1000.0f / 24.0f) - options.overscan);
}

TEST("a short list builds every row and does not scroll") {
    ScrollState state;
    const VirtualListOptions options{.count = 4, .rowHeight = kRowHeight};
    const Run run = build(state, options);

    CHECK_EQ(run.slice.first, std::size_t{0});
    CHECK_EQ(run.slice.count, std::size_t{4});
    CHECK(!state.scrollable());
}

TEST("an empty list builds nothing at all") {
    ScrollState state;
    const Run run = build(state, {.count = 0, .rowHeight = kRowHeight});

    CHECK(run.slice.empty());
    CHECK(run.built.empty());
}

TEST("revealRow scrolls the least distance that shows a row") {
    const VirtualListOptions options{.count = 1000, .rowHeight = 20.0f};
    ScrollState state;
    state.viewportSize = 200.0f;
    state.contentSize = 20000.0f;

    // Already visible: nothing moves.
    revealRow(state, options.rows(), 3);
    CHECK_NEAR(state.offset, 0.0f);

    // One row below the fold scrolls by exactly one row.
    revealRow(state, options.rows(), 10);
    CHECK_NEAR(state.offset, 20.0f);

    // Back up, and the row lands against the top edge rather than the bottom.
    revealRow(state, options.rows(), 0);
    CHECK_NEAR(state.offset, 0.0f);

    // The last row cannot scroll past the end of the content.
    revealRow(state, options.rows(), 999);
    CHECK_NEAR(state.offset, state.maxOffset());
}

TEST("revealRow before the first layout does not collapse the offset") {
    const VirtualListOptions options{.count = 1000, .rowHeight = 20.0f};
    // Nothing measured yet: a restored offset, and no idea where the fold is.
    ScrollState state;
    state.offset = 400.0f;

    // A row below the offset cannot be reasoned about, so nothing moves…
    revealRow(state, options.rows(), 500);
    CHECK_NEAR(state.offset, 400.0f);

    // …and a row above it still scrolls up, which needs no viewport.
    revealRow(state, options.rows(), 5);
    CHECK_NEAR(state.offset, 100.0f);
}

namespace {

/** The example's arrow-key loop, run for real: keys resolved against the tree
 *  the reader was looking at, the selection moved, the row revealed, and the
 *  list rebuilt from where that left the offset. */
struct Walk {
    ScrollState state;
    std::size_t selected = 0;
    VirtualSlice slice;
    bool selectionVisible = false;
};

Walk walkWith(Key key, int frames, const VirtualListOptions& options) {
    Theme theme = Theme::dark();
    Interaction input;
    Walk walk;

    for (int frame = 0; frame < frames + 2; ++frame) {
        Arena arena;
        Ui ui(arena);
        {
            auto root = ui.beginColumn({.width = kWindow.width, .height = kWindow.height});
            if (input.isFocusedWithin("list")) {
                const std::size_t before = walk.selected;
                for (const KeyEvent& event : input.keys()) {
                    if (event.key == Key::Down && walk.selected + 1 < options.count) {
                        ++walk.selected;
                    }
                    if (event.key == Key::Up && walk.selected > 0) --walk.selected;
                }
                if (walk.selected != before) {
                    revealRow(walk.state, options.rows(), walk.selected);
                }
            }
            walk.slice = virtualList(ui, input, "list", walk.state, options,
                                     [&](Ui&, std::size_t) {});
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), kWindow, context);

        InputFrame events;
        // Two frames of Tab to put the keyboard on the list, then the arrow.
        if (frame == 0) events.keys.push_back({Key::Tab});
        else events.keys.push_back({key});
        input.update(arena, ui.root(), events);
    }

    walk.selectionVisible =
        walk.selected >= walk.slice.first && walk.selected < walk.slice.last();
    return walk;
}

}  // namespace

TEST("holding an arrow key walks the selection to the end of the list") {
    const VirtualListOptions options{.count = 200, .rowHeight = kRowHeight};
    const Walk walk = walkWith(Key::Down, 400, options);

    CHECK_EQ(walk.selected, options.count - 1);
    CHECK(walk.selectionVisible);
    CHECK_NEAR(walk.state.offset, walk.state.maxOffset());
}

TEST("holding an arrow key back up returns to the top") {
    const VirtualListOptions options{.count = 200, .rowHeight = kRowHeight};
    Walk down = walkWith(Key::Down, 400, options);
    CHECK_EQ(down.selected, options.count - 1);

    const Walk up = walkWith(Key::Up, 400, options);
    CHECK_EQ(up.selected, std::size_t{0});
    CHECK_NEAR(up.state.offset, 0.0f);
}
