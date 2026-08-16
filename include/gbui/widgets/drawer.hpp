// A panel that comes in from an edge.
//
// The shape behind a navigation menu on a narrow window, a filter pane, a
// details sidebar, a settings sheet on a phone. It is a dialog that arrives
// from a side rather than the middle, and the parts it shares with `modal` —
// the backdrop, the focus trap, the header, Escape — are the same parts.
//
// ---- why this one takes `open` and `modal` does not -------------------------
//
// Everywhere else in this library a floating thing is shown by *being built*
// and hidden by not being. That works because appearing is instant. A drawer's
// whole point is that it slides, and **a component that stops being called
// cannot animate its own exit** — the node is gone on the frame the caller
// stops asking for it, and there is nothing left to move off screen.
//
// So the flag comes in and the component decides what to draw: sliding in,
// fully out, sliding away, or nothing at all once it has finished leaving. The
// application still owns the boolean, which is the rule that matters; what it
// no longer owns is the frame the panel disappears on.
//
// ---- modal or not -----------------------------------------------------------
//
// `DrawerOptions::modal` is the difference between a sheet that blocks the
// window and a pane that sits beside it — a backdrop and a focus trap, or
// neither. Both are real: a navigation drawer on a phone is modal, and an
// inspector on a desktop is not. It is one component with an option rather than
// two components, for the reason this library keeps rediscovering: two things
// that differ only in what they refuse are one thing with an option.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

/** Which edge it comes in from. */
enum class DrawerSide { Left, Right, Top, Bottom };

struct DrawerOptions {
    /** Which edge it comes in from, and therefore which way it slides. */
    DrawerSide side = DrawerSide::Right;
    /**
     * How far across the window it reaches, in pixels.
     *
     * Clamped to the window, so a 400-pixel drawer on a 320-pixel window is a
     * 320-pixel drawer rather than one with its close button off the edge.
     */
    float size = 320.0f;
    /**
     * Blocks the window: a backdrop behind it and the keyboard trapped inside.
     *
     * On, because a panel that arrives over the content and leaves the content
     * usable is an unusual thing to want and a confusing thing to meet. Off is
     * the desktop inspector — a pane beside the work rather than in front of
     * it.
     */
    bool modal = true;
    /** Dims what is behind it. Only meaningful when `modal`; a non-modal
     *  drawer with a backdrop would dim a window it does not block. */
    bool backdrop = true;
    /** The title bar. Off leaves the whole panel to the caller, which is what
     *  a drawer holding its own toolbar wants. */
    bool header = true;
    /** The × in the header. A drawer with no other way out should keep it. */
    bool closeButton = true;
    /** Beside the title. */
    std::optional<Icon> icon{};
    /** A press on the backdrop puts it away. Only when `modal` — without a
     *  backdrop there is nothing that press could mean. */
    bool dismissOnBackdrop = true;
    bool dismissOnEscape = true;
    /** How long the slide takes, in seconds. Needs an animator; without one it
     *  simply appears, which is what every animated thing here does. */
    float duration = 0.22f;
    /** The window. Empty means the viewport the last layout used. */
    Rect bounds{};
};

struct DrawerResult {
    /** The close button, the backdrop or Escape. Set the caller's flag from
     *  it; the panel then slides away over the next few frames on its own. */
    bool dismissed = false;
    /** Still on screen — sliding in, open, or sliding out.
     *
     *  Worth reading when a drawer is expensive to fill: the body callback is
     *  not run once this is false, so a caller doing work *around* the drawer
     *  can stop too. */
    bool visible = false;
};

struct Drawer {
    /** The panel's content box. A zero-sized clipped box out of the flow when
     *  the drawer is not on screen, so writing into it draws nothing. */
    Ui::Scope body;
    DrawerResult result;
};

/**
 * A panel from an edge.
 *
 *     bool showFilters = …;
 *     auto sheet = drawer(ui, input, "filters", "Filters", showFilters);
 *     if (sheet.result.dismissed) showFilters = false;
 *     {
 *         // contents, written into sheet.body like any other container
 *     }
 *
 * The call sits unconditionally in the frame, the way `tooltip` does. Closed
 * and finished leaving, `body` is a zero-sized clipped box out of the flow, so
 * whatever the caller writes into it costs a few nodes and draws nothing —
 * rather than landing in whichever container happened to be current.
 */
Drawer drawer(Ui& ui, const Interaction& input, std::string_view id, std::string_view title,
              bool open, const DrawerOptions& options = {});

}  // namespace gbui
