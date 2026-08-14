// The view a chart shows, and the gestures that move it.
#include "gbui/widgets/chart.hpp"

#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "harness.hpp"

using namespace gbui;

TEST("a view squares itself up") {
    ChartView backwards{0.8, 0.3};
    backwards.normalise();
    CHECK_NEAR(backwards.from, 0.3);
    CHECK_NEAR(backwards.to, 0.8);
}

TEST("a view narrower than the minimum is widened about its middle") {
    ChartView pinched{0.5, 0.5};
    pinched.normalise(0.1);
    CHECK_NEAR(pinched.span(), 0.1);
    CHECK_NEAR((pinched.from + pinched.to) / 2.0, 0.5);
}

/**
 * The behaviour worth pinning: a window pushed off an end **slides** back
 * rather than being squashed against it. Clamping each end on its own turns a
 * pan towards the edge into a zoom out, so the reader's window silently grows
 * every time they reach the start of the data.
 */
TEST("a view pushed past an end keeps its width") {
    ChartView past{-0.2, 0.1};
    past.normalise();
    CHECK_NEAR(past.from, 0.0);
    CHECK_NEAR(past.span(), 0.3);

    ChartView beyond{0.9, 1.2};
    beyond.normalise();
    CHECK_NEAR(beyond.to, 1.0);
    CHECK_NEAR(beyond.span(), 0.3);
}

TEST("a view wider than the data becomes exactly the data") {
    ChartView huge{-1.0, 2.0};
    huge.normalise();
    CHECK_NEAR(huge.from, 0.0);
    CHECK_NEAR(huge.to, 1.0);
    CHECK(huge.whole());
}

TEST("reset shows everything again") {
    ChartView zoomed{0.4, 0.5};
    CHECK(!zoomed.whole());
    zoomed.reset();
    CHECK(zoomed.whole());
    CHECK_NEAR(zoomed.span(), 1.0);
}

/** The scale is what makes a chart readable, and it is the one piece of
 *  arithmetic here a reader would notice being wrong. */
TEST("a scale widens to round numbers rather than to the data") {
    const Scale scale = Scale::nice(0.0, 93.0, 5);
    CHECK(scale.low <= 0.0);
    CHECK(scale.high >= 93.0);
    const std::vector<double> ticks = scale.ticks(5);
    CHECK(ticks.size() >= 2);
    // Every tick is a round step apart, and the first sits on the low end.
    const double step = ticks[1] - ticks[0];
    CHECK(step > 0.0);
    for (std::size_t i = 1; i < ticks.size(); ++i) {
        CHECK_NEAR(ticks[i] - ticks[i - 1], step);
    }
}

/** A candle whose feed reported high and low the wrong way round still draws
 *  the right way up, because `top`/`bottom` take the extremes of all four. */
TEST("a candle absorbs a high and low the wrong way round") {
    const Candle crossed{10.0, 8.0, 14.0, 12.0};
    CHECK_NEAR(crossed.top(), 14.0);
    CHECK_NEAR(crossed.bottom(), 8.0);
    CHECK(crossed.rising());
}

// ---------------------------------------------------------------------------
// The sweep
// ---------------------------------------------------------------------------

namespace {

/**
 * Drives a line chart over several frames with the pointer wherever the caller
 * puts it, and hands back the view it left behind.
 *
 * A whole `Ui` per frame because that is how the toolkit runs: the tree is
 * rebuilt every time, hit testing reads the *previous* frame's layout, and a
 * gesture that only works when those two agree is exactly what this is here to
 * check.
 */
class Plot {
public:
    Plot() : theme_(Theme::dark()) {
        for (int i = 0; i < 40; ++i) series_[0].values.push_back(i % 7);
    }

    /** One frame with the pointer at `x` across a 400-wide plot. */
    void frame(float x, bool down, float wheel = 0.0f, bool ctrl = false) {
        Arena arena;
        Ui ui(arena);
        InputFrame pointer;
        pointer.pointer = {x, 60.0f};
        pointer.pointerDown = down;
        pointer.wheel = wheel;
        pointer.modifiers.ctrl = ctrl;
        pointer.modifiers.super = ctrl;   // the same key, wherever the platform put it
        input_.update(previous_, previousRoot_, pointer);

        Style page;
        // A column, so the chart is stretched across the 400 it is laid out
        // in. In a row it would size to its content, and a plot that grows
        // from a zero basis inside a content-sized parent is 400 pixels of
        // nothing — which is a fair description of the first draft of this
        // harness.
        page.direction = Direction::Column;
        auto root = ui.scope(page);
        lineChart(ui, input_, "c", series_, view_, {.axisWidth = 0.0f, .height = 120.0f}, zoom_);
        (void)root;

        LayoutContext context;
        context.theme = &theme_;
        layout(arena, ui.root(), Rect{0, 0, 400, 120}, context);
        previous_ = std::move(arena);
        previousRoot_ = ui.root();
    }

    ChartView& view() { return view_; }
    ChartZoom& zoom() { return zoom_; }
    /** What the pointer resolved as the wheel's target, which is the whole
     *  question for a chart sitting in a page that scrolls. */
    std::string_view wheelTarget() const { return input_.wheelTarget(); }

private:
    Theme theme_;
    Interaction input_;
    Arena previous_;
    NodeId previousRoot_{};
    ChartView view_{};
    ChartZoom zoom_{};
    std::vector<Series> series_{Series{"s", {}}};
};

}  // namespace

TEST("a sweep across the plot zooms to what it covered") {
    Plot plot;
    plot.frame(0.0f, false);           // a frame to lay out and be pointed at
    plot.frame(100.0f, false);         // the pointer arrives
    plot.frame(100.0f, true);          // pressed: the sweep is anchored
    plot.frame(300.0f, true);          // dragged across
    CHECK(plot.view().sweep.active);
    CHECK(plot.view().whole());        // and nothing has moved yet
    plot.frame(300.0f, false);         // released: now it commits
    CHECK(!plot.view().sweep.active);
    CHECK_NEAR(plot.view().from, 0.25);
    CHECK_NEAR(plot.view().to, 0.75);
}

/** Half of them start from the right-hand end, and a chart that only zoomed
 *  one way would look broken to every one of those readers. */
TEST("a sweep drawn right to left means the same range") {
    Plot plot;
    plot.frame(0.0f, false);
    plot.frame(300.0f, false);
    plot.frame(300.0f, true);
    plot.frame(100.0f, true);
    plot.frame(100.0f, false);
    CHECK_NEAR(plot.view().from, 0.25);
    CHECK_NEAR(plot.view().to, 0.75);
}

/** A reader asking for the readout on a touchpad moves a pixel or two, and
 *  zooming their chart to a sliver because of it would be a trap. */
TEST("a press that barely moves is a click, not a sweep") {
    Plot plot;
    plot.frame(0.0f, false);
    plot.frame(200.0f, false);
    plot.frame(200.0f, true);
    plot.frame(202.0f, true);
    plot.frame(202.0f, false);
    CHECK(plot.view().whole());
}

/** Shift is the pan half of the same gesture, and it only bites once there is
 *  somewhere to pan to — a whole view has nothing off screen. */
TEST("panning needs a zoomed view, and a sweep does not") {
    Plot plot;
    plot.zoom().drag = ChartDrag::Pan;
    plot.frame(0.0f, false);
    plot.frame(100.0f, false);
    plot.frame(100.0f, true);
    plot.frame(300.0f, true);
    CHECK(plot.view().whole());

    plot.view().from = 0.25;
    plot.view().to = 0.75;
    plot.frame(300.0f, true);
    plot.frame(200.0f, true);
    // Dragged left by a quarter of the plot, so the window travelled a quarter
    // of *its own* width — an eighth of the data — to the right.
    CHECK_NEAR(plot.view().from, 0.375);
    CHECK_NEAR(plot.view().to, 0.875);
}

// ---------------------------------------------------------------------------
// The group
// ---------------------------------------------------------------------------

namespace {

/**
 * Two bar charts stacked in a column over one `ChartLink`, driven a frame at a
 * time — which is the only way to test this: whether a chart follows the group
 * depends on what the *previous* frame laid out, exactly as hit testing does.
 */
class Pair {
public:
    Pair() : theme_(Theme::dark()) {}

    /** One frame with the pointer somewhere in a 400x200 box. The top chart
     *  owns y 0..100 and the bottom one y 100..200. */
    void frame(float x, float y) {
        Arena arena;
        Ui ui(arena);
        InputFrame pointer;
        pointer.pointer = {x, y};
        input_.update(previous_, previousRoot_, pointer);

        const std::vector<Series> series = {{.name = "s", .values = {4.0, 8.0, 2.0, 6.0}}};
        Style page;
        page.direction = Direction::Column;
        auto root = ui.scope(page);
        top_ = barChart(ui, input_, "top", series,
                        {.axisWidth = 0.0f, .height = 100.0f,
                         .categoryAxis = 0.0f, .link = &link_});
        bottom_ = barChart(ui, input_, "bottom", series,
                           {.axisWidth = 0.0f, .height = 100.0f,
                            .categoryAxis = 0.0f, .link = &link_});
        (void)root;

        LayoutContext context;
        context.theme = &theme_;
        layout(arena, ui.root(), Rect{0, 0, 400, 200}, context);
        previous_ = std::move(arena);
        previousRoot_ = ui.root();
    }

    int top() const { return top_.hoveredIndex; }
    int bottom() const { return bottom_.hoveredIndex; }
    const ChartLink& link() const { return link_; }

private:
    Theme theme_;
    Interaction input_;
    Arena previous_;
    NodeId previousRoot_{};
    ChartLink link_{};
    ChartResult top_{};
    ChartResult bottom_{};
};

}  // namespace

TEST("a chart in a group follows the one being pointed at") {
    Pair pair;
    pair.frame(0.0f, 0.0f);      // a frame to lay out and be pointed at
    pair.frame(150.0f, 50.0f);   // the second slot of the top chart

    CHECK(pair.top() == 1);
    // Built after the chart being pointed at, so it follows in the same frame.
    CHECK(pair.bottom() == 1);
    CHECK(pair.link().source == "top");
}

/** The other direction costs a frame, because the chart that follows was built
 *  before the one that published. At sixty frames a second nobody sees it, and
 *  a test that pretended otherwise would be testing the wrong thing. */
TEST("the group agrees whichever of them is pointed at") {
    Pair pair;
    pair.frame(0.0f, 0.0f);
    pair.frame(350.0f, 150.0f);   // the fourth slot of the bottom chart
    CHECK(pair.bottom() == 3);
    pair.frame(350.0f, 150.0f);
    CHECK(pair.top() == 3);
    CHECK(pair.bottom() == 3);
}

TEST("a group goes quiet together") {
    Pair pair;
    pair.frame(0.0f, 0.0f);
    pair.frame(150.0f, 50.0f);
    CHECK(pair.bottom() == 1);

    // Off both plots. The chart that published withdraws, and the other has
    // nothing left to follow.
    pair.frame(150.0f, 400.0f);
    pair.frame(150.0f, 400.0f);
    CHECK(pair.top() < 0);
    CHECK(pair.bottom() < 0);
    CHECK(pair.link().index < 0);
    CHECK(pair.link().source.empty());
}

/**
 * Two charts over one `ChartView`, which is how a price and its volume zoom
 * together — and the one arrangement that used to break the sweep outright: the
 * second chart was handed a gesture it had not started, read "a sweep is
 * running and the pointer is not on me" as a release, and committed it on the
 * frame it began.
 */
namespace {

class Shared {
public:
    Shared() : theme_(Theme::dark()) {
        for (int i = 0; i < 40; ++i) values_.push_back(i % 7);
    }

    void frame(float x, float y, bool down) {
        Arena arena;
        Ui ui(arena);
        InputFrame pointer;
        pointer.pointer = {x, y};
        pointer.pointerDown = down;
        input_.update(previous_, previousRoot_, pointer);

        const std::vector<Series> series = {{.name = "s", .values = values_}};
        Style page;
        page.direction = Direction::Column;
        auto root = ui.scope(page);
        barChart(ui, input_, "top", series, view_,
                 {.axisWidth = 0.0f, .height = 100.0f, .categoryAxis = 0.0f});
        barChart(ui, input_, "bottom", series, view_,
                 {.axisWidth = 0.0f, .height = 100.0f, .categoryAxis = 0.0f});
        (void)root;

        LayoutContext context;
        context.theme = &theme_;
        layout(arena, ui.root(), Rect{0, 0, 400, 200}, context);
        previous_ = std::move(arena);
        previousRoot_ = ui.root();
    }

    ChartView& view() { return view_; }

private:
    Theme theme_;
    Interaction input_;
    Arena previous_;
    NodeId previousRoot_{};
    ChartView view_{};
    std::vector<double> values_;
};

}  // namespace

TEST("a sweep survives a second chart sharing the view") {
    Shared pair;
    pair.frame(0.0f, 0.0f, false);
    pair.frame(100.0f, 50.0f, false);
    pair.frame(100.0f, 50.0f, true);    // pressed on the top chart
    CHECK(pair.view().sweep.active);
    CHECK(pair.view().sweep.on == "top.plot");
    pair.frame(300.0f, 50.0f, true);
    // Still being drawn: the chart underneath must not have ended it.
    CHECK(pair.view().sweep.active);
    CHECK(pair.view().whole());

    pair.frame(300.0f, 50.0f, false);
    CHECK(!pair.view().sweep.active);
    CHECK_NEAR(pair.view().from, 0.25);
    CHECK_NEAR(pair.view().to, 0.75);
}

/** And the other chart is looking at the same window, because it is the same
 *  window — which is the whole reason a view is the caller's. */
TEST("both charts of a shared view show the same window") {
    Shared pair;
    pair.frame(0.0f, 0.0f, false);
    pair.frame(120.0f, 150.0f, false);
    pair.frame(120.0f, 150.0f, true);   // pressed on the bottom one this time
    CHECK(pair.view().sweep.on == "bottom.plot");
    pair.frame(280.0f, 150.0f, true);
    pair.frame(280.0f, 150.0f, false);
    CHECK_NEAR(pair.view().from, 0.3);
    CHECK_NEAR(pair.view().to, 0.7);
}

// ---------------------------------------------------------------------------
// The wheel
// ---------------------------------------------------------------------------

/**
 * The guard that used to make wheel zoom dead code: a chart only ever sees the
 * wheel if its plot declared `Overflow::Scroll`, and the plot never did.
 */
TEST("the wheel zooms about the pointer") {
    Plot plot;
    plot.zoom().wheel = true;
    plot.zoom().wheelModifier = false;
    plot.frame(0.0f, false);
    plot.frame(100.0f, false);   // a quarter across, so the claim is in the tree
    CHECK(plot.wheelTarget() == "c.plot");

    plot.frame(100.0f, false, 3.0f);
    CHECK(!plot.view().whole());
    // Whatever was under the cursor stays under it: a quarter of the way into
    // the old window is a quarter of the way into the new one.
    const double at = plot.view().from + plot.view().span() * 0.25;
    CHECK_NEAR(at, 0.25);
}

/**
 * And the other half of that guard, which is why the claim is made per frame.
 *
 * A chart that wants Ctrl and the wheel must not hold the wheel the rest of the
 * time, or it is a hole in the middle of a page that scrolls: the reader rolls
 * past it and nothing moves.
 */
TEST("a chart wanting a modifier leaves the plain wheel to the page") {
    Plot plot;
    plot.zoom().wheel = true;
    plot.frame(0.0f, false);
    plot.frame(100.0f, false);
    CHECK(plot.wheelTarget().empty());
    plot.frame(100.0f, false, 3.0f);
    CHECK(plot.view().whole());

    // Held down, the claim lands on the next frame and the wheel is taken.
    plot.frame(100.0f, false, 0.0f, true);
    plot.frame(100.0f, false, 3.0f, true);
    CHECK(plot.wheelTarget() == "c.plot");
    CHECK(!plot.view().whole());
}

// ---------------------------------------------------------------------------
// The toolbar
// ---------------------------------------------------------------------------

namespace {

/** A toolbar on its own, clicked by finding its buttons where they landed. */
class Toolbar {
public:
    Toolbar() : theme_(Theme::dark()) {}

    void frame(Vec2 pointer, bool down) {
        Arena arena;
        Ui ui(arena);
        InputFrame in;
        in.pointer = pointer;
        in.pointerDown = down;
        input_.update(previous_, previousRoot_, in);

        Style page;
        page.direction = Direction::Row;
        auto root = ui.scope(page);
        chartToolbar(ui, input_, "t", view_);
        (void)root;

        LayoutContext context;
        context.theme = &theme_;
        layout(arena, ui.root(), Rect{0, 0, 300, 40}, context);
        previous_ = std::move(arena);
        previousRoot_ = ui.root();
    }

    /** Presses whichever button the id names, wherever layout put it. Hit
     *  testing reads the previous frame, so a frame goes by before the buttons
     *  have anywhere to be pressed. */
    void press(const char* button) {
        frame({-1.0f, -1.0f}, false);
        const Rect box = input_.frameOf(std::string("t.") + button);
        const Vec2 at{box.x + box.width / 2.0f, box.y + box.height / 2.0f};
        frame(at, false);
        frame(at, true);
        frame(at, false);
    }

    ChartView& view() { return view_; }

private:
    Theme theme_;
    Interaction input_;
    Arena previous_;
    NodeId previousRoot_{};
    ChartView view_{};
};

}  // namespace

TEST("the toolbar zooms in about the middle and puts it back") {
    Toolbar bar;
    bar.frame({-1.0f, -1.0f}, false);   // a frame to lay the buttons out

    bar.press("in");
    CHECK(!bar.view().whole());
    // About the middle, which is the only part of the window a reader pressing
    // a button can be sure stays.
    CHECK_NEAR((bar.view().from + bar.view().to) / 2.0, 0.5);
    const double once = bar.view().span();
    CHECK(once < 1.0);

    bar.press("in");
    CHECK(bar.view().span() < once);

    bar.press("out");
    CHECK_NEAR(bar.view().span(), once);

    bar.press("reset");
    CHECK(bar.view().whole());
}
