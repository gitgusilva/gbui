#include "gbui/widgets/toggle.hpp"

#include <algorithm>

#include "detail.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

bool toggle(Ui& ui, const Interaction& input, std::string_view id, bool on,
                  const ToggleOptions& options) {
    const bool hovered = input.isHovered(id);
    const bool ring = input.isFocusVisible(id);

    // Where the switch is, rather than where it was told to be. Everything
    // below is a function of this one number, so the knob, the track and the
    // dot all arrive together instead of one of them snapping.
    const float travel = ui.animate(id, "on", on ? 1.0f : 0.0f,
                                    {.duration = 0.18f, .easing = Easing::EaseOut});

    auto row = controlRow(ui, id, options.disabled);
    {
        const Design& design = ui.design();
        // The caller can still pin a size; unset, the design decides — which is
        // what makes a Material switch 52x32 and an iOS one 51x31 without a
        // single number in this file.
        const float trackWidth = options.width > 0.0f ? options.width : design.switchWidth;
        const float trackHeight = options.height > 0.0f ? options.height : design.switchHeight;

        constexpr float kBorder = 1.0f;
        constexpr float kInset = 2.0f;
        const float edge = kBorder + kInset;

        Style track;
        track.width = trackWidth;
        track.height = trackHeight;
        track.shrink = 0.0f;
        track.padding = Edges::all(kInset);
        track.radius = trackHeight / 2.0f;
        const FieldPalette off = disabledPalette();
        track.background = options.disabled ? off.background : Fill{Token::BgOverlay};
        track.border = Border{kBorder, Fill{options.disabled ? off.border
                                            : hovered        ? Token::BorderStrong
                                                             : Token::Border}};
        // The track is the accent when the switch is on, so the ring cannot be
        // its border either — same reasoning as the checkbox.
        if (ring) track.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
        auto scope = ui.scope(track);

        // The accent arrives as a wash over the base rather than as a second
        // colour, because the builder has no theme and so cannot interpolate
        // two tokens. A token at an alpha is one value, which *is* animatable —
        // and at full alpha it is exactly the colour it replaced.
        if (travel > 0.0f && !options.disabled) {
            Style wash;
            wash.position = Position::Absolute;
            wash.left = -edge;  // out of the content box, back onto the frame
            wash.top = -edge;
            wash.width = trackWidth;
            wash.height = trackHeight;
            wash.radius = track.radius;
            wash.background = Fill{Token::Accent, travel};
            ui.add(wash);
        }

        // The thumb, which Material grows as it turns on and the others do not.
        // It is capped by the track so a design cannot ask for one that spills.
        const float room = trackHeight - 2.0f * edge;
        const float knobSize =
            std::min(room, design.switchKnob + (design.switchKnobOn - design.switchKnob) * travel);
        Style knob;
        // Positioned rather than justified: `justify` has two values and this
        // has every value in between.
        knob.position = Position::Absolute;
        knob.left = travel * (trackWidth - 2.0f * edge - knobSize);
        // Centred across the track, since the thumb may be smaller than the room.
        knob.top = (room - knobSize) / 2.0f;
        knob.width = knobSize;
        knob.height = knobSize;
        knob.shrink = 0.0f;
        knob.radius = knobSize / 2.0f;
        knob.background = Fill{Token::TextMuted};
        auto knobScope = ui.scope(knob);
        // The knob takes the accent's foreground when the track is filled, so a
        // theme with a pale accent still shows it — faded in the same way.
        if (travel > 0.0f && !options.disabled) {
            Style face;
            face.position = Position::Absolute;
            face.left = 0.0f;
            face.top = 0.0f;
            face.width = knobSize;
            face.height = knobSize;
            face.radius = knob.radius;
            face.background = Fill{Token::AccentFg, travel};
            ui.add(face);
        }
        (void)knobScope;
        (void)scope;
    }
    if (!options.label.empty()) {
        labelGap(ui);
        text(ui, options.label, {.color = options.disabled ? Token::TextMuted : Token::Text});
    }
    (void)row;

    return activated(input, id, options.disabled);
}

}  // namespace gbui
