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

Ui::Scope Ui::scope(const Style& style) {
    const NodeId id = attach(style);
    stack_.push_back(id);
    return Scope(*this, id);
}

Ui::Scope Ui::row(Style style) {
    style.direction = Direction::Row;
    return scope(style);
}

Ui::Scope Ui::column(Style style) {
    style.direction = Direction::Column;
    return scope(style);
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

NodeId Ui::picture(const Bitmap& source, Style style, ImageFit fit, float opacity) {
    const NodeId id = attach(style);
    arena_[id].image = ImageContent{source, fit, opacity};
    return id;
}

Ui& Ui::tag(std::string_view id) {
    if (last_.valid()) arena_[last_].id = arena_.intern(id);
    return *this;
}

Ui::IdScope Ui::ids(std::string_view name) {
    const std::size_t restoreTo = idPrefix_.size();
    if (!name.empty()) {
        if (!idPrefix_.empty()) idPrefix_ += '.';
        idPrefix_ += name;
    }
    return IdScope(*this, restoreTo);
}

std::string_view Ui::qualify(std::string_view name) {
    if (idPrefix_.empty()) return arena_.intern(name);
    if (name.empty()) return arena_.intern(idPrefix_);

    std::string full;
    full.reserve(idPrefix_.size() + 1 + name.size());
    full += idPrefix_;
    full += '.';
    full += name;
    return arena_.intern(full);
}

Ui& Ui::focusable(bool value) {
    if (last_.valid()) arena_[last_].focusable = value;
    return *this;
}

Ui& Ui::accessible(const Accessibility& info) { return accessible(last_, info); }

Ui& Ui::accessible(NodeId node, const Accessibility& info) {
    if (!node.valid()) return *this;
    Accessibility& target = arena_.accessibilityFor(node);

    // Merged, not assigned. Two calls compose — a component sets its role and a
    // wrapper adds the relation that names it — and an unset field means "I had
    // nothing to say about this", never "set it back to the default". Assigning
    // the whole record instead would make the second call silently erase the
    // first, which is the kind of bug that shows up as a control that used to
    // announce itself and quietly stopped.
    if (info.role != Role::None) target.role = info.role;
    if (info.hidden) target.hidden = true;
    if (!info.name.empty()) target.name = arena_.intern(info.name);
    if (!info.description.empty()) target.description = arena_.intern(info.description);

    const auto mergeFlag = [](Flag& into, Flag from) {
        if (from != Flag::Unset) into = from;
    };
    mergeFlag(target.state.checked, info.state.checked);
    mergeFlag(target.state.expanded, info.state.expanded);
    mergeFlag(target.state.selected, info.state.selected);
    mergeFlag(target.state.pressed, info.state.pressed);
    mergeFlag(target.state.disabled, info.state.disabled);
    mergeFlag(target.state.readOnly, info.state.readOnly);
    mergeFlag(target.state.invalid, info.state.invalid);
    mergeFlag(target.state.busy, info.state.busy);
    mergeFlag(target.state.required, info.state.required);
    if (info.state.sorted != Sort::Unset) target.state.sorted = info.state.sorted;

    if (info.value.present) {
        target.value = info.value;
        target.value.text = arena_.intern(info.value.text);
    }

    const auto mergeTag = [&](std::string_view& into, std::string_view from) {
        if (!from.empty()) into = arena_.intern(from);
    };
    mergeTag(target.relations.labelledBy, info.relations.labelledBy);
    mergeTag(target.relations.describedBy, info.relations.describedBy);
    mergeTag(target.relations.controls, info.relations.controls);
    mergeTag(target.relations.owns, info.relations.owns);
    mergeTag(target.relations.activeDescendant, info.relations.activeDescendant);
    mergeTag(target.relations.labels, info.relations.labels);
    mergeTag(target.relations.describes, info.relations.describes);

    if (info.setSize != 0) {
        target.positionInSet = info.positionInSet;
        target.setSize = info.setSize;
    }
    if (info.level != 0) target.level = info.level;

    return *this;
}

Ui& Ui::role(Role value) { return accessible({.role = value}); }

Ui& Ui::name(std::string_view value) { return accessible({.name = value}); }

Ui& Ui::ignoresPointer(bool value) {
    if (last_.valid()) arena_[last_].ignoresPointer = value;
    return *this;
}

Ui& Ui::trapsFocus(bool value) {
    if (last_.valid()) arena_[last_].trapsFocus = value;
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
