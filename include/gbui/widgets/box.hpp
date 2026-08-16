// The general container, the way `<div>` is one.
//
// `ui.scope(Style{…})` already builds any box the layout engine can express, so
// `box` is not a new capability — it is the ergonomics. A sidebar, a navbar, a
// card, a section and a plain wrapper are all the same node with different
// values, and writing six lines of `Style` at each of those call sites is how a
// codebase ends up with six slightly different sidebars.
//
// Presets are functions returning options, not a second API:
//
//     auto card = box(ui, BoxStyle::card());
//     auto bar  = box(ui, BoxStyle::navbar({.height = 44.0f}));
#pragma once

#include <limits>
#include <optional>
#include <string_view>

#include "gbui/scene/ui.hpp"
#include "gbui/style/style.hpp"

namespace gbui {

struct BoxOptions {
    // layout
    Direction direction = Direction::Row;
    Justify justify = Justify::Start;
    Align align = Align::Stretch;
    float gap = 0.0f;
    Edges padding{};
    Edges margin{};

    // size
    float width = kAuto;
    float height = kAuto;
    float minWidth = kAuto;
    float minHeight = kAuto;
    float maxWidth = std::numeric_limits<float>::infinity();
    float maxHeight = std::numeric_limits<float>::infinity();
    float grow = 0.0f;
    float shrink = 1.0f;
    float basis = kAuto;

    // appearance
    Fill background{};
    Gradient backgroundGradient{};
    std::optional<Token> border{};
    float borderWidth = 1.0f;
    float radius = kAuto;
    float opacity = 1.0f;
    Overflow overflow = Overflow::Visible;
    Layer layer = Layer::Content;
    Cursor cursor = Cursor::Default;

    // identity
    std::string_view id{};
    bool focusable = false;
    /**
     * What this container is to a reader who cannot see it.
     *
     * `None` by default, and that is the right default for the general
     * container: most boxes are layout, and a tree full of anonymous groups is
     * a tree a reader has to walk through rather than one they can navigate.
     * Set it where the box *is* something — a `Form`, a `List`, a named
     * `Group`, a `Figure` around a drawing.
     */
    Role role = Role::None;
    /** What it is called. A `Group` with no name is not worth announcing, and
     *  the tree treats it as though the role were `None`. */
    std::string_view name{};
};

/** Opens a container. Everything until the returned scope dies is inside it. */
Ui::Scope box(Ui& ui, const BoxOptions& options = {});

/** Presets for the containers an application actually repeats. Each returns
 *  options, so a caller overrides whatever it likes. */
namespace BoxStyle {

/** A raised surface with a border — a card, a dialog body, a docked pane. */
BoxOptions card(BoxOptions base = {});
/** A full-height column at the side of the window. */
BoxOptions sidebar(BoxOptions base = {});
/** A horizontal bar with a rule under it. */
BoxOptions navbar(BoxOptions base = {});
/** A plain content wrapper: padding and a gap, nothing drawn. */
BoxOptions section(BoxOptions base = {});
/** Centres its contents on both axes. */
BoxOptions centre(BoxOptions base = {});

}  // namespace BoxStyle

}  // namespace gbui
