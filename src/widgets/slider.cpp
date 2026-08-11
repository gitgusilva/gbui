#include "gbui/widgets/slider.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

SliderResult slider(Ui& ui, const Interaction& input, std::string_view id, double value,
                    const SliderOptions& options) {
    const bool hovered = input.isHovered(id) || input.isHovered(std::string(id) + ".track");
    const bool focused = input.isFocused(id);
    const bool ring = input.isFocusVisible(id);
    const double span = std::max(1e-9, options.maximum - options.minimum);
    const std::string trackId = std::string(id) + ".track";

    const Rect trackFrame = input.frameOf(trackId);
    const float knobSize = focused || hovered ? 14.0f : 12.0f;

    double next = value;
    if (!options.disabled) {
        // The drag continues while the button is held, wherever the pointer
        // goes — and it starts on the press, not on the first move, so a click
        // on the track jumps the value there.
        const bool dragging = input.dragging() == id || input.dragging() == trackId;
        if (dragging && trackFrame.width > knobSize) {
            // The knob's centre can only reach half a knob from each end, so
            // the usable travel is the track minus one knob. Mapping against
            // the full width made the value stick at both extremes.
            const float travel = trackFrame.width - knobSize;
            const double ratio =
                std::clamp((static_cast<double>(input.pointer().x - trackFrame.x) -
                            knobSize / 2.0) / static_cast<double>(travel),
                           0.0, 1.0);
            next = options.minimum + ratio * span;
        }
        if (focused) {
            const double keyStep = options.step > 0.0 ? options.step : span / 20.0;
            for (const KeyEvent& event : input.keys()) {
                if (event.key == Key::Left || event.key == Key::Down) next -= keyStep;
                if (event.key == Key::Right || event.key == Key::Up) next += keyStep;
                if (event.key == Key::Home) next = options.minimum;
                if (event.key == Key::End) next = options.maximum;
            }
        }
    }

    const float ratio = static_cast<float>(std::clamp((value - options.minimum) / span, 0.0, 1.0));

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 10.0f;
    row.width = options.width;
    row.grow = options.grow;
    row.height = options.height;
    row.opacity = opacityFor(options.disabled);

    auto scope = ui.begin(row);
    ui.tag(id).focusable(!options.disabled);

    {
        Style track;
        track.grow = 1.0f;
        track.basis = 0.0f;
        track.height = options.height;
        track.align = Align::Center;
        track.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Pointer;
        auto trackScope = ui.begin(track);
        ui.tag(trackId).cursor(track.cursorHint);

        // The rail is one node spanning the whole track, so the fill can never
        // be laid out around the knob — which is what made the mapping
        // non-linear. It grows: without that its main size is its content, and
        // a node with no content is zero wide.
        Style rail;
        rail.grow = 1.0f;
        rail.basis = 0.0f;
        rail.height = 4.0f;
        rail.radius = 2.0f;
        rail.background = Fill{Token::BgOverlay};
        ui.add(rail);

        if (trackFrame.width > 0.0f) {
            const float travel = std::max(0.0f, trackFrame.width - knobSize);

            // Measured from the track's own content box, not from the window:
            // only the track's *size* comes from the last frame. Adding its
            // origin as well — which this did — puts the fill and the knob a
            // whole pane to the right of the rail they belong to, which is why
            // a slider drew as an empty line.
            Style filled;
            filled.position = Position::Absolute;
            filled.left = 0.0f;
            filled.top = (trackFrame.height - 4.0f) / 2.0f;
            filled.width = knobSize / 2.0f + travel * ratio;
            filled.height = 4.0f;
            filled.radius = 2.0f;
            // Disabled loses the accent here too: a filled bar in the accent
            // reads as a value you can still drag.
            filled.background = Fill{options.disabled ? Token::BorderStrong : Token::Accent};
            ui.add(filled);

            // Absolutely positioned and added last, so it is painted over the
            // rail instead of under it.
            Style knob;
            knob.position = Position::Absolute;
            knob.left = travel * ratio;
            knob.top = (trackFrame.height - knobSize) / 2.0f;
            knob.width = knobSize;
            knob.height = knobSize;
            knob.radius = knobSize / 2.0f;
            knob.background = options.disabled ? disabledPalette().background
                                               : Fill{Token::AccentFg};
            knob.border = Border{2.0f, Fill{options.disabled ? Token::BorderStrong
                                                             : Token::Accent}};
            if (ring) knob.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
            ui.add(knob);
        }
        (void)trackScope;
    }

    if (options.showValue) {
        Style valueBox;
        valueBox.width = 48.0f;
        valueBox.shrink = 0.0f;
        auto valueScope = ui.begin(valueBox);
        text(ui, formatNumber(value, options.decimals, {}),
             {.color = Token::TextMuted, .size = 11.0f, .align = TextAlign::End});
        (void)valueScope;
    }
    (void)scope;

    const double clamped = clampAndSnap(next, options.minimum, options.maximum, options.step);
    return {clamped, std::fabs(clamped - value) > 1e-9};
}

}  // namespace gbui
