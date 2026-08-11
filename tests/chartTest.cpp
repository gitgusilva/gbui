// The view a chart shows, and the gestures that move it.
#include "gbui/widgets/chart.hpp"

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
