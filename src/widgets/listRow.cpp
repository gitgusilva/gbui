#include "gbui/widgets/listRow.hpp"



namespace gbui {

Ui::Scope listRow(Ui& ui, const ListRowOptions& options) {
    Style style;
    style.direction = Direction::Row;
    style.align = Align::Center;
    style.minHeight = options.height;
    style.padding = options.padding;
    style.gap = options.gap;
    if (options.selected) {
        style.background = Fill{Token::Accent, 0.18f};
    } else if (options.hovered) {
        style.background = Fill{Token::SurfaceHover};
    }
    // A row with an id is a row something can happen to — that is what the tag
    // is for. One without is a layout row, and a hand over it would lie.
    if (!options.id.empty()) style.cursorHint = Cursor::Pointer;

    auto scope = ui.scope(style);
    if (!options.id.empty()) ui.tag(options.id).cursor(Cursor::Pointer);
    return scope;
}

}  // namespace gbui
