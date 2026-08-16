// An empty floating surface the caller fills.
//
// **The application owns whether it is open.** A component that decided that
// for itself would need to keep state, and a toolkit that keeps state cannot
// rebuild its tree.
//
//     if (input.clicked("toolbar.branch")) state.menuOpen = !state.menuOpen;
//     if (state.menuOpen) {
//         auto menu = popover(ui, input, "toolbar.branch.menu", "toolbar.branch");
//         …
//     }
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/floating.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

struct PopoverOptions : FloatingOptions {
    float minWidth = 160.0f;
    float maxWidth = 420.0f;
    /**
     * A ceiling on how tall it may grow.
     *
     * `kAuto` does *not* mean unbounded: it means **the room actually available
     * on the side it lands on**, less the margin. A popup that would run past
     * the bottom of the window stops at it and scrolls inside instead — which
     * is what JetBrains' popups do, and the only behaviour that works when the
     * anchor is near an edge. A number overrides that, and a smaller one wins.
     */
    float minHeight = kAuto;
    float maxHeight = kAuto;
    /** Lets a popup grow past the window's edge, for a caller that would rather
     *  clip than scroll. Off, because the default should be reachable. */
    bool allowOverflow = false;
    /**
     * Scrolls its own contents once `maxHeight` bites, rather than making the
     * caller wrap everything in a scroll view. The axis is the caller's:
     * `ScrollAxis::None` clips instead, which is what a popover that must never
     * scroll wants.
     *
     * **`scrollState` has to be set too, or this does nothing.** Both of them
     * together are what turns the ceiling into a scroll; an axis on its own
     * leaves the box clipped, which looks like the popup simply lost its last
     * rows. Three callers had set exactly this and only this, and the calendar
     * opened near the bottom of a window was missing a week with no way to
     * reach it. There is no default state to fall back on: state belongs to the
     * application here as it does everywhere else.
     */
    ScrollAxis scroll = ScrollAxis::Vertical;
    /**
     * Where the scroll position lives, when the popover scrolls its own
     * contents.
     *
     * Null means it does not: the box is still bounded by `maxHeight` and still
     * clips, but nothing moves. State belongs to the application here as it does
     * everywhere else — a popover that kept its own offset would be a component
     * with memory, and the tree is rebuilt every frame.
     */
    ScrollState* scrollState = nullptr;
    /** Matches the anchor's width — what a select's list wants. */
    bool matchAnchorWidth = false;
    Edges padding = Edges::all(6.0f);
    float gapBetweenItems = 2.0f;
    /**
     * What the surface is, to a reader who cannot see it float.
     *
     * `None` by default and on purpose: a popover is a *placement*, not a kind
     * of thing, and the same box holds a menu, a list of values and a calendar.
     * Whoever opened it knows which — `select` says `ListBox`, a context menu
     * says `Menu` — and a default of "menu" would put that word in front of
     * every date picker in the tree.
     */
    Role role = Role::None;
    /** What it is called, when the role is worth announcing. */
    std::string_view name{};
};

/** Returns a scope, so its contents are written inside the braces like any
 *  other container. */
Ui::Scope popover(Ui& ui, const Interaction& input, std::string_view id,
                  std::string_view anchorId, const PopoverOptions& options = {});

}  // namespace gbui
