#include "gbui/widgets/spacing.hpp"



namespace gbui {

NodeId spacer(Ui& ui, float grow) {
    Style style;
    style.grow = grow;
    style.shrink = 1.0f;
    style.basis = 0.0f;
    return ui.add(style);
}

NodeId divider(Ui& ui, Direction containerDirection) {
    Style style;
    style.background = Fill{Token::Border};
    // A rule spans the cross axis and is one pixel on the main one.
    if (containerDirection == Direction::Column) {
        style.height = 1.0f;
        style.width = kAuto;
    } else {
        style.width = 1.0f;
        style.height = kAuto;
    }
    style.radius = 0.0f;
    const NodeId id = ui.add(style);
    // A rule is a boundary, and a boundary is the one purely visual thing a
    // reader still needs told about — it is what says two lists are two lists.
    // A `spacer` above says nothing, because empty space is not a boundary.
    ui.role(Role::Separator);
    return id;
}

}  // namespace gbui
