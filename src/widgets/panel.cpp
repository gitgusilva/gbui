#include "gbui/widgets/panel.hpp"



namespace gbui {

Ui::Scope beginPanel(Ui& ui, const PanelOptions& options) {
    Style style;
    style.direction = options.direction;
    style.padding = options.padding;
    style.gap = options.gap;
    style.background = Fill{options.background};
    style.radius = options.radius;
    if (options.border) style.border = Border{1.0f, Fill{Token::Border}};
    style.grow = options.grow;
    if (options.grow > 0.0f) style.basis = 0.0f;
    return ui.begin(style);
}

}  // namespace gbui
