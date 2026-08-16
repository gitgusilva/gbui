// A colour picker: a saturation/value square, a hue rail and an alpha rail.
#pragma once

#include <string_view>
#include <vector>

#include "gbui/core/color.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

/**
 * What a picker remembers between frames, owned by the application.
 *
 * It holds `Hsv` rather than a `Color` on purpose. The round trip through RGB
 * is lossy exactly where a picker is used: at zero saturation every hue is the
 * same grey, and at zero value every colour is black — so dragging into a
 * corner and back out would lose the hue the user had picked. The colour is the
 * *output*; this is the state.
 */
struct ColorPickerState {
    Hsv value{};
    /** Whether the popover form is showing. Ignored by the inline form, which
     *  is always open by definition. */
    bool open = false;
    /** Where the open picker is scrolled to, when it opened somewhere too short
     *  for it. The square, two rails, a readout and a row of swatches is a tall
     *  popup, and against the bottom of a window it has to be reachable. */
    ScrollState popup;
    /** Set once from a `Color` — afterwards the picker owns the hue. */
    void set(Color color) { value = Hsv::fromColor(color); }
    Color color() const { return value.toColor(); }
};

/** Which rails a picker offers beyond the square. */
struct ColorPickerOptions {
    /** The alpha rail and the alpha in the readout. Off for a picker choosing a
     *  theme token, where a translucent colour is not a valid answer. */
    bool alpha = true;
    /** The hue rail. Off leaves a picker that only shades one hue, which is
     *  what a "tint of the accent" control wants. */
    bool hue = true;
    /** The `#rrggbb` readout under the rails. */
    bool showHex = true;
    /** A row of fixed swatches under everything else. Empty draws none. */
    std::vector<Color> swatches{};

    float width = 240.0f;
    float squareHeight = 150.0f;
    float railHeight = 14.0f;
    float gap = 10.0f;
    /** What colour this is *of* — "Accent", "Series 3". The value announces
     *  itself as a hex code; only the caller knows what it colours. */
    std::string_view name{};
};

struct ColorPickerResult {
    /** The colour changed this frame. */
    bool changed = false;
    Color color{};
};

/**
 * Draws the picker and edits `state` in place.
 *
 * The square is built the way every picker on the web is: the pure hue behind,
 * a white-to-transparent gradient across it and a transparent-to-black gradient
 * down it. Two gradients over a fill is a two-dimensional ramp, and it costs
 * three nodes rather than a per-pixel shader the painter does not have.
 */
ColorPickerResult colorPicker(Ui& ui, const Interaction& input, std::string_view id,
                              ColorPickerState& state, const ColorPickerOptions& options = {});

struct ColorFieldOptions : ColorPickerOptions {
    /** The trigger's own size. The popover takes `width` from the picker
     *  options above, as everything else does. */
    float height = 28.0f;
    bool showHexOnTrigger = true;
    bool disabled = false;
    /**
     * A press anywhere else closes it.
     *
     * On, because a popup that outlives the attention that opened it is a
     * popup the reader has to dismiss on purpose. Off keeps it open until the
     * application says otherwise — which is what a picker being dragged from
     * inside a dialog wants, where the click outside belongs to the dialog.
     */
    bool dismissOnOutsideClick = true;
    /** Escape closes it. Separate from the above: a caller may want the key
     *  without the click, and a modal that owns Escape may want neither. */
    bool dismissOnEscape = true;
};

/**
 * The same picker behind a swatch, opened in a popover.
 *
 * Two entry points rather than a flag, because they are two different
 * components in every way that matters: the inline one is always open and owns
 * its space, and this one is a control in a form that borrows space when asked.
 * They share the state and the drawing, which is the part worth sharing.
 *
 * **The application owns whether it is open**, as it does for every other
 * overlay here — `state.open` is a plain bool it can set, restore or ignore.
 */
ColorPickerResult colorField(Ui& ui, const Interaction& input, std::string_view id,
                             ColorPickerState& state, const ColorFieldOptions& options = {});

}  // namespace gbui
