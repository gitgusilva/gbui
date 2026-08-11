#include "gbui/widgets/radio.hpp"

#include "detail.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

bool radio(Ui& ui, const Interaction& input, std::string_view id, bool selected,
           const RadioOptions& options) {
    const bool hovered = input.isHovered(id);
    const bool focusRing = input.isFocusVisible(id);

    auto row = beginControlRow(ui, id, options.disabled);
    {
        const float side = options.size > 0.0f ? options.size : ui.design().radioSize;
        Style ring;
        ring.width = side;
        ring.height = side;
        ring.shrink = 0.0f;
        ring.justify = Justify::Center;
        ring.align = Align::Center;
        ring.radius = side / 2.0f;
        // Disabled drops the accent for the elevated surface, so a chosen
        // option no longer looks like one that can still be chosen.
        const FieldPalette off = disabledPalette();
        ring.background = options.disabled ? off.background : Fill{Token::Bg};
        ring.border = options.disabled ? Border{1.0f, Fill{off.border}}
                      : selected       ? Border{2.0f, Fill{Token::Accent}}
                                       : ringFor(focusRing, hovered);
        // Selected already spends the border on the accent, so the focus ring
        // goes outside it — see the checkbox above.
        if (focusRing && selected && !options.disabled) {
            ring.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
        }
        auto scope = ui.begin(ring);
        if (selected) {
            Style dot;
            const float dotSize = side * 0.42f;
            dot.width = dotSize;
            dot.height = dotSize;
            dot.radius = dotSize / 2.0f;
            dot.background = Fill{options.disabled ? off.label : Token::Accent};
            ui.add(dot);
        }
        (void)scope;
    }
    if (!options.label.empty()) {
        labelGap(ui);
        text(ui, options.label, {.color = options.disabled ? Token::TextMuted : Token::Text});
    }
    (void)row;

    // Choosing what is already chosen is not a change, so it reports nothing.
    return !selected && activated(input, id, options.disabled);
}

}  // namespace gbui
