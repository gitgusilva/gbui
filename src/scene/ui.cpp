#include "gbui/scene/ui.hpp"

namespace gbui {

NodeId Ui::attach(const Style& style) {
    const NodeId id = arena_.create(style);
    if (const NodeId parent = current(); parent.valid()) {
        arena_.addChild(parent, id);
    } else {
        // The first node built becomes the root. A second one at top level
        // would be unreachable, so it is parented to the root instead of
        // silently replacing it.
        root_ = id;
    }
    last_ = id;
    return id;
}

Ui::Scope Ui::begin(const Style& style) {
    const NodeId id = attach(style);
    stack_.push_back(id);
    return Scope(*this, id);
}

Ui::Scope Ui::beginRow(Style style) {
    style.direction = Direction::Row;
    return begin(style);
}

Ui::Scope Ui::beginColumn(Style style) {
    style.direction = Direction::Column;
    return begin(style);
}

NodeId Ui::add(const Style& style) { return attach(style); }

NodeId Ui::label(std::string_view text, TextStyle textStyle, Style style) {
    const NodeId id = attach(style);
    Node& node = arena_[id];
    node.text = arena_.intern(text);
    node.textStyle = textStyle;
    return id;
}

NodeId Ui::draw(const Style& style, std::vector<Shape> shapes) {
    const NodeId id = attach(style);
    Node& node = arena_[id];
    node.shapeCount = static_cast<std::uint32_t>(shapes.size());
    node.firstShape = arena_.addShapes(std::move(shapes));
    last_ = id;
    return id;
}

NodeId Ui::vector(std::string_view pathData, Style style, Fill color, float stroke) {
    const NodeId id = attach(style);
    arena_[id].icon = IconContent{pathData, stroke, color};
    return id;
}

Ui& Ui::tag(std::string_view id) {
    if (last_.valid()) arena_[last_].id = arena_.intern(id);
    return *this;
}

Ui& Ui::focusable(bool value) {
    if (last_.valid()) arena_[last_].focusable = value;
    return *this;
}

Ui& Ui::ignoresPointer(bool value) {
    if (last_.valid()) arena_[last_].ignoresPointer = value;
    return *this;
}

Ui& Ui::cursor(Cursor value) {
    if (last_.valid()) arena_[last_].cursor = value;
    return *this;
}

void Ui::pop() {
    if (!stack_.empty()) stack_.pop_back();
}

}  // namespace gbui
