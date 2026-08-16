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

    auto scope = ui.scope(style);
    // A pill is a word with a colour behind it, and the colour is the part a
    // reader cannot have. `Label` with the word on it, rather than `Status`:
    // a badge that has always said "3" is not news, and a live region that
    // announced itself on every rebuild would be.
    ui.accessible({.role = Role::Label, .name = value});
    TextOptions label;
    label.color = options.foreground;
    label.size = 11.0f;
    label.weight = FontWeight::Medium;
    text(ui, value, label);
    return scope.id();
}

}  // namespace gbui
