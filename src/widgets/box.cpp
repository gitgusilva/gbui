#include "gbui/widgets/box.hpp"

#include <algorithm>

namespace gbui {

Ui::Scope box(Ui& ui, const BoxOptions& options) {
    Style style;
    style.direction = options.direction;
    style.justify = options.justify;
    style.align = options.align;
    style.gap = options.gap;
    style.padding = options.padding;
    style.margin = options.margin;

    style.width = options.width;
    style.height = options.height;
    style.minWidth = options.minWidth;
    style.minHeight = options.minHeight;
    style.maxWidth = options.maxWidth;
    style.maxHeight = options.maxHeight;
    style.grow = options.grow;
    style.shrink = options.shrink;
    style.basis = options.basis;

    style.background = options.background;
    style.backgroundGradient = options.backgroundGradient;
    if (options.border) style.border = Border{options.borderWidth, Fill{*options.border}};
    style.radius = options.radius;
    style.opacity = options.opacity;
    style.overflow = options.overflow;
    style.layer = options.layer;
    style.cursorHint = options.cursor;

    auto scope = ui.scope(style);
    if (!options.id.empty()) ui.tag(options.id);
    if (options.focusable) ui.focusable();
    if (options.cursor != Cursor::Default) ui.cursor(options.cursor);
    if (options.role != Role::None || !options.name.empty()) {
        ui.accessible({.role = options.role, .name = options.name});
    }
    return scope;
}

namespace BoxStyle {

BoxOptions card(BoxOptions base) {
    base.direction = Direction::Column;
    base.background = Fill{Token::BgElevated};
    base.border = Token::Border;
    if (base.padding.top == 0.0f && base.padding.left == 0.0f) base.padding = Edges::all(16.0f);
    if (base.gap == 0.0f) base.gap = 10.0f;
    return base;
}

BoxOptions sidebar(BoxOptions base) {
    base.direction = Direction::Column;
    base.background = Fill{Token::Bg};
    // A sidebar keeps its width while everything else gives way: it is a
    // navigation surface, and navigation that shrinks is navigation that goes.
    base.shrink = 0.0f;
    if (isAuto(base.width)) base.width = 220.0f;
    if (base.gap == 0.0f) base.gap = 2.0f;
    base.radius = 0.0f;
    return base;
}

BoxOptions navbar(BoxOptions base) {
    base.direction = Direction::Row;
    base.align = Align::Center;
    base.background = Fill{Token::BgElevated};
    if (isAuto(base.height)) base.height = 44.0f;
    if (base.gap == 0.0f) base.gap = 8.0f;
    if (base.padding.left == 0.0f) base.padding = Edges::symmetric(0.0f, 12.0f);
    base.radius = 0.0f;
    return base;
}

BoxOptions section(BoxOptions base) {
    base.direction = Direction::Column;
    if (base.padding.top == 0.0f) base.padding = Edges::all(12.0f);
    if (base.gap == 0.0f) base.gap = 8.0f;
    return base;
}

BoxOptions centre(BoxOptions base) {
    base.justify = Justify::Center;
    base.align = Align::Center;
    return base;
}

}  // namespace BoxStyle

}  // namespace gbui
