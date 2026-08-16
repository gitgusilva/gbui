// Two things in the same rectangle, with a handle that says how much of each.
//
// A before and an after: a photograph retouched, a chart rescaled, a diff of
// two renders. Both are drawn at the full size of the box and one is revealed
// over the other, which is what makes the comparison work — the reader is
// looking at the *same pixels in the same place*, and any layout that put them
// side by side would be asking them to remember instead of to see.
//
// ---- the part that is not obvious ------------------------------------------
//
// The revealed side is clipped by a **percentage**, not by a measured width.
// A box whose size is only known after layout would draw its seam a frame late
// and jump on every resize; a percentage resolves against the container during
// layout, so the seam is right on the first frame and stays right while the
// window is being dragged. The content inside that clip is then a percentage of
// *it* — `100 / position` — which comes back out to the full width of the box.
//
// The handle is placed the same way, by two flexible spacers rather than by an
// offset in pixels. Same reason, and it is also what keeps the handle wholly
// inside the box at either end instead of half out of it.
//
// ---- what it is, to a reader who cannot see it -----------------------------
//
// A slider, and genuinely one: it has a value from 0 to 1, it answers the arrow
// keys, and the thing it changes is how much of each side is showing. PrimeVue
// reaches the same answer through a hidden `<input type=range>`; here the role
// *is* the control, so there is nothing hidden to keep in step.
#pragma once

#include <functional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

/** Which way the handle travels. */
enum class CompareOrientation { Horizontal, Vertical };

struct CompareOptions {
    /** Which way the seam travels. Horizontal for two pictures, vertical for
     *  two things read top to bottom — a chart against its own baseline. */
    CompareOrientation orientation = CompareOrientation::Horizontal;
    /**
     * The handle follows the pointer without anything being pressed.
     *
     * Off, because a comparison is something a reader *sets* and then looks at,
     * and a seam that moves whenever the pointer crosses the picture cannot be
     * left anywhere. On for a gallery of thumbnails where the gesture is the
     * whole interaction and there is nothing to leave it at.
     */
    bool slideOnHover = false;
    /** How wide the bar down the seam is. The grip on it is drawn from this. */
    float handleWidth = 3.0f;
    /** What is being compared — "Before and after retouching". */
    std::string_view name{};
    /**
     * What each side is.
     *
     * Announced with the value, because "60 percent" says nothing on its own:
     * sixty percent of *what*, revealing *what*. A reader who cannot see either
     * picture has only these two words to tell them which way the handle is
     * moving them.
     */
    std::string_view beforeLabel{};
    /** As above, for the side the handle reveals — and the one the value is
     *  announced in terms of, since that is the one moving. */
    std::string_view afterLabel{};
    float height = 240.0f;
    float width = kAuto;
    float grow = 0.0f;
};

struct CompareResult {
    /** Where the seam is now, 0 to 1 along the orientation. */
    float position = 0.5f;
    bool changed = false;
};

/**
 * Draws `before` under `after`, with `after` revealed from the leading edge up
 * to `position`.
 *
 * Both callbacks are built at the **full size of the box** and both are built
 * every frame — that is what makes the two comparable, and it is also why they
 * should be cheap: this is a picture against a picture, not two dashboards.
 */
CompareResult compare(Ui& ui, const Interaction& input, std::string_view id, float position,
                      const std::function<void(Ui&)>& before,
                      const std::function<void(Ui&)>& after,
                      const CompareOptions& options = {});

}  // namespace gbui
