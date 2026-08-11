#include "gbui/widgets/toolbar.hpp"



namespace gbui {

Ui::Scope beginToolbar(Ui& ui, const ToolbarOptions& options) {
    Style style;
    style.direction = Direction::Row;
    style.align = Align::Center;
    style.height = options.height;
    style.gap = options.gap;
    style.padding = options.padding;
    style.background = Fill{options.background};
    style.radius = 0.0f;
    if (options.bottomBorder) {
        // Only the bottom edge is wanted, and the Border primitive is all four.
        // A one-pixel child at the end of a column would be the honest way to
        // do it; until the primitive grows per-edge widths, the toolbar draws
        // its rule as a sibling. Callers get the border by adding
        // divider(ui, Direction::Column) after the toolbar.
    }
    return ui.begin(style);
}

}  // namespace gbui
