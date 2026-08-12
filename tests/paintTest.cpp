// What `record` puts in the display list.
#include "gbui/paint/paint.hpp"

#include <vector>

#include "gbui/layout/layout.hpp"
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
        auto root = ui.beginColumn({});
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
        auto root = ui.beginColumn({});
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
