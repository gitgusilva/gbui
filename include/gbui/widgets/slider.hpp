// A single-value slider.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct SliderOptions {
    double minimum = 0.0;
    double maximum = 1.0;
    /** 0 is continuous; anything else snaps to a multiple of it. */
    double step = 0.0;
    bool disabled = false;
    float width = kAuto;
    /**
     * Takes the free space on the parent's main axis, which is what a slider
     * in a row wants and the only thing `grow` can mean.
     *
     * In a *column* the main axis is vertical, so this used to make a slider
     * eat the leftover height and sit alone at the bottom of its card. It no
     * longer can: the control is clamped to `height` whichever way it is
     * pointed, so growing is free horizontally and impossible vertically.
     */
    float grow = 1.0f;
    float height = 20.0f;
    /** Shows the value at the right of the track. */
    bool showValue = false;
    int decimals = 2;
    /** What this slider is called, for a reader who cannot see the caption
     *  beside it. Unnecessary when a `label` or a `field` names it — those
     *  attach the relation instead. */
    std::string_view name{};
    /**
     * What a reader hears instead of the number — "70 percent", "3 of 8".
     *
     * Empty falls back to the number, which is right for a bare fraction and
     * wrong for everything with a unit. The toolkit cannot supply this: it has
     * no locale to spell a unit in and no idea what the number counts.
     */
    std::string_view valueText{};
};

struct SliderResult {
    double value = 0.0;
    bool changed = false;
};

/** Follows the pointer while it is held, even when it wanders off the track —
 *  which is what `Interaction::dragging` is for. */
[[nodiscard]] SliderResult slider(Ui& ui, const Interaction& input, std::string_view id,
                                  double value, const SliderOptions& options = {});

}  // namespace gbui
