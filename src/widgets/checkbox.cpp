#include "gbui/widgets/checkbox.hpp"

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

bool checkbox(Ui& ui, const Interaction& input, std::string_view id, bool checked,
              const CheckboxOptions& options) {
    const bool hovered = input.isHovered(id);
    const bool ring = input.isFocusVisible(id);

    auto row = controlRow(ui, id, options.disabled);
    // On the row rather than on the box, because the row is what Tab lands on
    // and what a click activates. Announcing the drawn square instead would
    // name a node the keyboard can never reach.
    ui.accessible({
        .role = Role::Checkbox,
        .name = options.name.empty() ? options.label : options.name,
        .state = {.checked = flag(checked), .disabled = flag(options.disabled)},
    });
    {
        const float side = options.size > 0.0f ? options.size : ui.design().checkboxSize;
        Style box;
        box.width = side;
        box.height = side;
        box.shrink = 0.0f;
        box.justify = Justify::Center;
        box.align = Align::Center;
        box.radius = options.size > 0.0f ? 4.0f : ui.design().checkboxRadius;
        // Disabled loses the accent entirely: a checked box in the accent still
        // reads as an available choice however faint it is.
        const FieldPalette off = disabledPalette();
        box.background = options.disabled ? off.background
                         : checked        ? Fill{Token::Accent}
                                          : Fill{Token::Bg};
        box.border = options.disabled ? Border{1.0f, Fill{off.border}}
                     : checked        ? Border{}
                                      : ringFor(ring, hovered);
        // A ring in the accent, around a box filled with the accent, is not a
        // ring. Once the box is filled it moves outside, with a gap, so the
        // surface behind the control separates the two.
        if (ring && checked && !options.disabled) {
            box.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
        }
        auto scope = ui.scope(box);
        if (checked) {
            icon(ui, Icon::Check,
                 {.color = options.disabled ? off.label : Token::AccentFg,
                  .size = side - 4.0f, .stroke = 3.0f});
        }
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
