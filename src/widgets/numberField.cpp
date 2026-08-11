#include "gbui/widgets/numberField.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

NumberFieldResult numberField(Ui& ui, const Interaction& input, std::string_view id, double value,
                              const NumberFieldOptions& options) {
    const bool hovered = input.isHovered(id);
    const bool focused = input.isFocused(id);
    const bool editable = !options.disabled && !options.readOnly;

    double next = value;
    if (editable) {
        if (focused) {
            for (const KeyEvent& event : input.keys()) {
                if (event.key == Key::Up || event.key == Key::Plus) next += options.step;
                if (event.key == Key::Down || event.key == Key::Minus) next -= options.step;
                if (event.key == Key::Home) next = options.minimum;
                if (event.key == Key::End) next = options.maximum;
            }
        }
        // The wheel steps the field under the pointer, which is what makes a
        // spin box usable without clicking into it first.
        if (hovered && input.wheel() != 0.0f) {
            next += options.step * static_cast<double>(input.wheel());
        }
    }

    // A field narrower than the side-by-side form needs stacks instead: the
    // value squeezing to "7…" between two buttons is worse than smaller arrows.
    const Rect known = input.frameOf(id);
    const float measuredWidth = known.width > 0.0f ? known.width : options.width;
    StepperPlacement steppers = options.steppers;
    if (steppers == StepperPlacement::Sides && measuredWidth < options.stackedBelow) {
        steppers = StepperPlacement::Stacked;
    }
    if (!editable) steppers = StepperPlacement::None;

    const FieldPalette palette = paletteForField(options.disabled, options.readOnly, hovered);

    Style box;
    box.direction = Direction::Row;
    box.align = Align::Center;
    const float controlHeight =
        options.height > 0.0f ? options.height : ui.design().controlHeight;
    box.minHeight = controlHeight;
    box.width = options.width;
    // The value is the point of the control, so it keeps room for itself.
    box.minWidth = 72.0f;
    box.padding = Edges::symmetric(0.0f, 4.0f);
    box.gap = 2.0f;
    box.background = palette.background;
    box.border = Border{1.0f, Fill{palette.border}};
    // Takes typing, so it rings on focus rather than on focus-visible — see
    // `textField`.
    if (focused && editable) box.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
    box.overflow = Overflow::Hidden;

    auto scope = ui.begin(box);
    ui.tag(id).focusable(!options.disabled);

    const std::string decrementId = std::string(id) + ".decrement";
    const std::string incrementId = std::string(id) + ".increment";

    const auto stepButton = [&](const std::string& buttonId, Icon glyph, double delta,
                                float width, float height) {
        Style button;
        button.width = width;
        button.height = height;
        button.shrink = 0.0f;
        button.justify = Justify::Center;
        button.align = Align::Center;
        button.radius = 3.0f;
        button.background = input.isHovered(buttonId) ? Fill{Token::SurfaceHover} : Fill{};
        button.cursorHint = Cursor::Pointer;
        auto buttonScope = ui.begin(button);
        ui.tag(buttonId).cursor(Cursor::Pointer);
        icon(ui, glyph, {.color = Token::TextMuted, .size = std::min(12.0f, height - 2.0f)});
        (void)buttonScope;
        if (editable && input.clicked(buttonId)) next += delta;
    };

    if (steppers == StepperPlacement::Sides) {
        stepButton(decrementId, Icon::Minus, -options.step, 20.0f, controlHeight - 8.0f);
    }

    text(ui, formatNumber(value, options.decimals, options.suffix),
         {.color = palette.label,
          .align = steppers == StepperPlacement::Sides ? TextAlign::Center : TextAlign::Start,
          .grow = 1.0f});

    if (steppers == StepperPlacement::Sides) {
        stepButton(incrementId, Icon::Plus, options.step, 20.0f, controlHeight - 8.0f);
    } else if (steppers == StepperPlacement::Stacked) {
        // Two half-height arrows on the right: the classic spin box, and the
        // narrowest arrangement that still affords clicking.
        Style column;
        column.direction = Direction::Column;
        column.width = 16.0f;
        column.shrink = 0.0f;
        column.gap = 1.0f;
        auto columnScope = ui.begin(column);
        const float half = (controlHeight - 10.0f) / 2.0f;
        stepButton(incrementId, Icon::ChevronDown, options.step, 16.0f, half);
        stepButton(decrementId, Icon::ChevronDown, -options.step, 16.0f, half);
        (void)columnScope;
    }
    (void)scope;

    const double clamped = clampAndSnap(next, options.minimum, options.maximum,
                                        options.decimals > 0 ? 0.0 : 1.0);
    return {clamped, std::fabs(clamped - value) > 1e-9};
}

}  // namespace gbui
