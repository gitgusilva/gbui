// Two panes and a divider the reader can drag.
//
// Every IDE-shaped application is built from this, nested: a sidebar beside an
// editor beside a panel. It is a container by the taxonomy's first question —
// its job is the content inside it — and it takes that content as two callbacks
// for the same reason `compare` does: there are parts on both sides of the
// thing it draws between them.
//
// ---- the split is a fraction, and the layout does the arithmetic ------------
//
// The panes are laid out by **flex grow ratios**, not by measured widths. A
// split placed from last frame's geometry lags a resize by a frame and jumps
// while the window is being dragged; a ratio is resolved during layout and is
// right on the first frame. `compare` reaches the same conclusion by the same
// road, and the minimums come for free — flexbox already refuses to shrink a
// child below its `minWidth`, so the two panes cannot squeeze each other out.
//
// The one thing that *is* measured is the drag, because turning a pointer
// position into a fraction needs to know how wide the container is. That is one
// frame behind and it is the frame the reader was pointing at.
//
// ---- a divider is a control ------------------------------------------------
//
// It takes the keyboard, it has a value, and it answers the arrow keys — ARIA
// calls this the window splitter and it is the half everybody forgets. A split
// only draggable with a pointer is a layout most people cannot change.
#pragma once

#include <functional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

/** Which way the panes sit. `Horizontal` puts them side by side, with a
 *  divider that moves left and right. */
enum class SplitOrientation { Horizontal, Vertical };

struct SplitPaneOptions {
    /** Which way the panes sit, and therefore which pair of arrow keys moves
     *  the divider. */
    SplitOrientation orientation = SplitOrientation::Horizontal;
    /**
     * The smallest each pane may become, in pixels.
     *
     * Enforced by the layout rather than by the drag, which is what makes it
     * hold when the *window* shrinks as well as when the divider moves — the
     * case a clamp in the drag handler silently misses.
     */
    float minLeading = 120.0f;
    /** As above, for the other one. */
    float minTrailing = 120.0f;
    /** How wide the grab strip is. Wider than the hairline drawn in it: a
     *  one-pixel target is a target nobody hits. */
    float dividerWidth = 7.0f;
    /** What the divider is called. "Sidebar width" says more than "Resize",
     *  and it is the only thing a reader has to know which of two panes the
     *  arrow keys are about to change. */
    std::string_view name{};
    /** What each side is, announced as a group around it. Empty leaves the
     *  pane's own contents to speak for it. */
    std::string_view leadingLabel{};
    /** As above, for the other one. */
    std::string_view trailingLabel{};
    float width = kAuto;
    float height = kAuto;
    float grow = 1.0f;
};

struct SplitPaneResult {
    /** Where the divider is now, as a fraction of the container. */
    float position = 0.5f;
    bool changed = false;
};

/**
 * Draws `leading`, a divider, and `trailing`.
 *
 * `position` is the share the leading pane asks for, 0 to 1. It is a *request*:
 * the minimums win over it, so a container too narrow for both gives each what
 * it needs and the fraction stops being reachable — which is the right answer
 * and the one a caller does not have to write.
 */
SplitPaneResult splitPane(Ui& ui, const Interaction& input, std::string_view id, float position,
                          const std::function<void(Ui&)>& leading,
                          const std::function<void(Ui&)>& trailing,
                          const SplitPaneOptions& options = {});

}  // namespace gbui
