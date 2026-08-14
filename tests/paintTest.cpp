// What `record` puts in the display list.
#include "gbui/paint/paint.hpp"

#include <vector>

#include "gbui/layout/layout.hpp"
#include "gbui/paint/canvas.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

/** Records a one-node tree holding `shapes` and returns the list. */
DisplayList recordShapes(std::vector<Shape> shapes) {
    Theme theme = Theme::dark();
    Arena arena;
    Ui ui(arena);
    {
        auto root = ui.column({});
        ui.draw({.width = 100.0f, .height = 100.0f}, std::move(shapes));
        (void)root;
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 200}, context);

    DisplayList list;
    record(arena, ui.root(), theme, list);
    return list;
}

/** The first path in a list, or nothing. */
const DrawPath* firstPath(const DisplayList& list) {
    for (const DrawCommand& command : list.commands()) {
        if (const auto* path = std::get_if<DrawPath>(&command)) return path;
    }
    return nullptr;
}

Path line() {
    Path path;
    path.moveTo({0.0f, 0.0f});
    path.lineTo({50.0f, 0.0f});
    return path;
}

}  // namespace

TEST("a shape without a gradient paints one colour") {
    const DisplayList list = recordShapes({Shape{line(), Fill{Token::Accent}, 2.0f}});
    const DrawPath* path = firstPath(list);
    CHECK(path != nullptr);
    if (!path) return;
    CHECK(!path->paint.isGradient());
    CHECK_EQ(path->paint.color.r, Theme::dark().color(Token::Accent).r);
}

TEST("a shape's gradient is resolved against the theme") {
    // Two stops of one token at different alphas, which is the only kind of
    // gradient a themed screen can write — the token stays the token.
    const Gradient fade = Gradient::linear(Fill{Token::Accent, 0.25f}, Fill{Token::Accent}, 90.0f);
    const DisplayList list = recordShapes({Shape{line(), Fill{Token::Accent}, 2.0f, fade}});

    const DrawPath* path = firstPath(list);
    CHECK(path != nullptr);
    if (!path) return;
    CHECK(path->paint.isGradient());
    CHECK_EQ(path->paint.gradient.stops.size(), std::size_t{2});
    CHECK_NEAR(path->paint.gradient.angle, 90.0f);
    // The alphas travelled; the colour is the theme's, at both ends.
    CHECK_NEAR(path->paint.gradient.stops.front().second.a, 0.25f);
    CHECK_NEAR(path->paint.gradient.stops.back().second.a, 1.0f);
    CHECK_EQ(path->paint.gradient.stops.back().second.r, Theme::dark().color(Token::Accent).r);
}

TEST("opacity scales every stop of a shape's gradient") {
    Theme theme = Theme::dark();
    Arena arena;
    Ui ui(arena);
    {
        auto root = ui.column({});
        Style style;
        style.width = 100.0f;
        style.height = 100.0f;
        style.opacity = 0.5f;
        ui.draw(style, {Shape{line(), Fill{Token::Accent}, 2.0f,
                              Gradient::linear(Fill{Token::Accent}, Fill{Token::Accent})}});
        (void)root;
    }
    LayoutContext context;
    context.theme = &theme;
    layout(arena, ui.root(), Rect{0, 0, 200, 200}, context);

    DisplayList list;
    record(arena, ui.root(), theme, list);
    const DrawPath* path = firstPath(list);
    CHECK(path != nullptr);
    if (!path) return;
    CHECK_NEAR(path->paint.gradient.stops.front().second.a, 0.5f);
}

/**
 * A square box nested in a rounded one keeps its own square corners.
 *
 * The radius belongs to the box it was written for, and nesting does not
 * transfer it. Taking the larger of the two rounded every inner box in the
 * application by its container's curve — a chart's plot inside a card came out
 * with four rounded corners of its own, which ate the ends of every gridline
 * and made two stacked plots curve away from each other where they met.
 */
TEST("a clip's radius stays with the box it was written for") {
    const Clip card{Rect{0, 0, 200, 200}, 12.0f};
    const Clip plot{Rect{40, 40, 120, 120}};   // square, well inside the card
    const Clip both = card.intersect(plot);

    CHECK_NEAR(both.rect.x, 40.0f);
    CHECK_NEAR(both.rect.width, 120.0f);
    // The curve is still the card's, and still at the card's corners.
    CHECK_NEAR(both.radius, 12.0f);
    CHECK_NEAR(both.rounded.x, 0.0f);
    CHECK_NEAR(both.rounded.width, 200.0f);
}

/** And the corner of the inner box is drawn, which is the thing that was
 *  actually wrong: it used to be cut by a curve twelve pixels across. */
TEST("the inner box's own corner is not rounded away") {
    Canvas canvas;
    canvas.resize(200, 200);
    canvas.clear(Color{0, 0, 0, 1.0f});
    const Clip both = Clip{Rect{0, 0, 200, 200}, 12.0f}.intersect(Clip{Rect{40, 40, 120, 120}});
    canvas.fillRoundedRect(Rect{40, 40, 120, 120}, 0.0f, Paint{Color{255, 255, 255, 1.0f}}, both);

    const auto at = [&](int x, int y) {
        return canvas.pixels()[static_cast<std::size_t>(y) * canvas.pitch() +
                               static_cast<std::size_t>(x) * 4];
    };
    CHECK(at(41, 41) > 200);     // the inner box's own corner, filled
    CHECK(at(158, 41) > 200);
    CHECK(at(41, 158) > 200);
    CHECK(at(100, 100) > 200);
}

/** The card's own corner still cuts, which is the half that has to keep
 *  working: content reaching the container's edge is rounded by it. */
TEST("the container's corner still cuts what reaches it") {
    Canvas canvas;
    canvas.resize(200, 200);
    canvas.clear(Color{0, 0, 0, 1.0f});
    const Clip card{Rect{0, 0, 200, 200}, 12.0f};
    canvas.fillRoundedRect(Rect{0, 0, 200, 200}, 0.0f, Paint{Color{255, 255, 255, 1.0f}}, card);

    const auto at = [&](int x, int y) {
        return canvas.pixels()[static_cast<std::size_t>(y) * canvas.pitch() +
                               static_cast<std::size_t>(x) * 4];
    };
    CHECK(at(1, 1) < 60);        // outside the curve
    CHECK(at(100, 1) > 200);     // along the top edge, inside it
}
