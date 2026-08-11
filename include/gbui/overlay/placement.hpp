// Where a floating thing goes.
//
// A tooltip, a dropdown, a popover and a context menu all ask the same
// question: given an anchor, a size and the window, where does this sit so it
// is next to what it belongs to and still on screen? The answer is one pure
// function, which is why it lives here rather than inside any of them — and
// why it is tested without a window.
//
// The model is the one Floating UI and Popper settled on, because it is the one
// that behaves the way people expect:
//
//   1. try the preferred side;
//   2. **flip** to the opposite side if it does not fit;
//   3. **shift** along the other axis to stay inside the window;
//   4. keep a margin from the edges so nothing touches them.
#pragma once

#include <optional>
#include <string_view>

#include "gbui/core/geometry.hpp"

namespace gbui {

/** Side, then alignment along that side. `BottomStart` means below the anchor,
 *  left edges aligned — the usual place for a dropdown. */
enum class Placement {
    Auto,          ///< pick the side with the most room
    Top, TopStart, TopEnd,
    Bottom, BottomStart, BottomEnd,
    Left, LeftStart, LeftEnd,
    Right, RightStart, RightEnd,
};

/** Parses "bottom-start" and the rest, for a stylesheet or an attribute. */
std::optional<Placement> placementFromName(std::string_view name);
std::string_view placementName(Placement placement);

struct PlacementOptions {
    Placement preferred = Placement::Auto;
    /** Distance between the anchor and the floating box. */
    float gap = 6.0f;
    /** How close to the window edge the box may come. */
    float margin = 8.0f;
    /** Move to the opposite side when the preferred one does not fit. */
    bool flip = true;
    /** Slide along the cross axis to stay inside the window. */
    bool shift = true;
};

struct PlacementResult {
    Rect rect;
    /** Where it actually went, with Auto resolved and any flip applied — an
     *  arrow or a tail needs to know which side it ended up on. */
    Placement placement = Placement::Bottom;
    /** True when the preferred side did not fit and the opposite was used. */
    bool flipped = false;
};

/**
 * Places a box of `size` against `anchor`, inside `bounds`.
 *
 * `bounds` is normally the window. Everything is in the same coordinate space,
 * and the result is absolute — ready for `Style::position = Absolute`.
 */
PlacementResult place(const Rect& anchor, Vec2 size, const Rect& bounds,
                      const PlacementOptions& options = {});

/** The side of a placement, with alignment stripped. */
Placement sideOf(Placement placement);

}  // namespace gbui
