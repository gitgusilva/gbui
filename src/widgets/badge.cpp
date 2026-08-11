#include "gbui/widgets/badge.hpp"

#include "gbui/widgets/text.hpp"

namespace gbui {

NodeId badge(Ui& ui, std::string_view value, const BadgeOptions& options) {
    Style style;
    style.direction = Direction::Row;
    style.align = Align::Center;
    style.justify = Justify::Center;
    style.padding = Edges::symmetric(2.0f, 8.0f);
    style.background = Fill{options.background};
    style.radius = 999.0f;
    // A pill is as wide as its word. Letting it shrink turns a branch name into
    // an ellipsis while the row still has room for the text beside it.
    style.shrink = 0.0f;  // fully rounded; the painter clamps to half the height

    auto scope = ui.begin(style);
    TextOptions label;
    label.color = options.foreground;
    label.size = 11.0f;
    label.weight = FontWeight::Medium;
    text(ui, value, label);
    return scope.id();
}

}  // namespace gbui
