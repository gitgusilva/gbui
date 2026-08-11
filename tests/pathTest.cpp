#include "gbui/core/path.hpp"

#include <cmath>

#include "gbui/widgets/icons.hpp"
#include "harness.hpp"

using namespace gbui;

TEST("the parser reads the commands an icon is written with") {
    const Path path = parseSvgPath("M2,3 L10,3 H14 V9 Z");
    CHECK_EQ(path.contours().size(), std::size_t{1});
    if (path.contours().empty()) return;

    const auto& points = path.contours().front().points;
    CHECK_EQ(points.size(), std::size_t{4});
    CHECK(path.contours().front().closed);
    CHECK_NEAR(points[1].x, 10.0f);
    CHECK_NEAR(points[2].x, 14.0f);  // H keeps the previous y
    CHECK_NEAR(points[2].y, 3.0f);
    CHECK_NEAR(points[3].y, 9.0f);   // V keeps the previous x
    CHECK_NEAR(points[3].x, 14.0f);
}

TEST("relative commands accumulate from where the pen is") {
    const Path absolute = parseSvgPath("M2,2 L6,2 L6,6");
    const Path relative = parseSvgPath("m2,2 l4,0 l0,4");

    CHECK_EQ(absolute.contours().size(), relative.contours().size());
    if (absolute.contours().empty() || relative.contours().empty()) return;
    const auto& a = absolute.contours().front().points;
    const auto& b = relative.contours().front().points;
    CHECK_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        CHECK_NEAR(a[i].x, b[i].x);
        CHECK_NEAR(a[i].y, b[i].y);
    }
}

TEST("a cubic is flattened to something that stays on the curve") {
    const Path path = parseSvgPath("M0,0 C0,10 10,10 10,0");
    CHECK_EQ(path.contours().size(), std::size_t{1});
    if (path.contours().empty()) return;

    const auto& points = path.contours().front().points;
    CHECK(points.size() > 4);  // more than the endpoints
    CHECK_NEAR(points.back().x, 10.0f);
    CHECK_NEAR(points.back().y, 0.0f);
    // The curve's peak is 3/4 of the control height, and never above it.
    float highest = 0.0f;
    for (const Vec2& point : points) highest = std::max(highest, point.y);
    CHECK(highest > 6.0f);
    CHECK(highest <= 10.0f);
}

TEST("an arc becomes a curve rather than stopping the parse") {
    // Lucide's git-branch: a quarter turn spelled as a relative arc. A parser
    // without arcs returns two points and the icon draws in pieces.
    const Path path = parseSvgPath("M15 6a9 9 0 0 0-9 9V3");
    CHECK_EQ(path.contours().size(), std::size_t{1});
    if (path.contours().empty()) return;

    const auto& points = path.contours().front().points;
    CHECK(points.size() > 5);
    // It ends where the V takes it, which only happens if the arc was consumed.
    CHECK_NEAR(points.back().x, 6.0f);
    CHECK_NEAR(points.back().y, 3.0f);

    const Rect bounds = path.bounds();
    CHECK(bounds.width > 8.0f);
    CHECK(bounds.height > 10.0f);
}

TEST("every shipped icon parses into geometry that fits its grid") {
    for (std::size_t i = 0; i < static_cast<std::size_t>(Icon::Count); ++i) {
        const Path path = parseSvgPath(iconPath(static_cast<Icon>(i)));
        CHECK(!path.empty());
        if (path.empty()) continue;

        // Lucide draws on 24x24 with a 2-unit stroke, so geometry outside
        // [-1, 25] means something was parsed wrong rather than drawn wide.
        const Rect bounds = path.bounds();
        CHECK(bounds.x >= -1.0f);
        CHECK(bounds.y >= -1.0f);
        CHECK(bounds.right() <= 25.0f);
        CHECK(bounds.bottom() <= 25.0f);
        CHECK(bounds.width > 0.0f);
    }
}

TEST("scaling an icon moves every point with it") {
    const Path path = parseSvgPath("M0,0 L24,24").transformed(0.5f, Vec2{10.0f, 4.0f});
    const Rect bounds = path.bounds();
    CHECK_NEAR(bounds.x, 10.0f);
    CHECK_NEAR(bounds.y, 4.0f);
    CHECK_NEAR(bounds.width, 12.0f);
    CHECK_NEAR(bounds.height, 12.0f);
}

TEST("a malformed path yields what was readable, not a crash") {
    const Path path = parseSvgPath("M0,0 L10,10 Q");
    CHECK(!path.empty());
    CHECK_EQ(path.contours().front().points.size(), std::size_t{2});
}
