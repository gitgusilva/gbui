#include "gbui/widgets/button.hpp"

#include <cmath>

#include "detail.hpp"

#include "gbui/widgets/icon.hpp"

namespace gbui {

namespace {

struct ButtonPalette {
    Fill background;
    Token label;
    Border border;
};

ButtonPalette paletteFor(ButtonVariant variant) {
    switch (variant) {
        case ButtonVariant::Primary:
            return {Fill{Token::Accent}, Token::AccentFg, Border{}};
        case ButtonVariant::Danger:
            return {Fill{Token::Removed}, Token::AccentFg, Border{}};
        case ButtonVariant::Ghost:
            return {Fill{}, Token::Text, Border{}};
        case ButtonVariant::Secondary:
            break;
    }
    return {Fill{Token::BgOverlay}, Token::Text, Border{1.0f, Fill{Token::BorderStrong}}};
}

/**
 * A circle of ink growing from where the press landed.
 *
 * It starts on the press rather than on the click, because that is the event
 * the user is waiting to see acknowledged — a ripple that waits for the button
 * to come up arrives after the decision it was meant to confirm.
 *
 * The radius runs to whichever corner is furthest, so the ink always reaches
 * every edge no matter where in the button the press was; the growth eases out
 * and the alpha does not, which is what keeps it from vanishing while it is
 * still visibly moving.
 */
void drawRipple(Ui& ui, const Interaction& input, const ButtonPalette& palette,
                const Style& style, const ButtonOptions& options) {
    const Transition kInk{.duration = 0.45f, .easing = Easing::Linear};

    const bool started = input.pressStarted(options.id);
    const float progress = ui.pulse(options.id, "ripple", started, kInk);
    if (progress >= 1.0f) return;

    // Where the press landed, in the button's own content box — which is what
    // `Position::Absolute` is measured from.
    const Rect frame = input.frameOf(options.id);
    const float inset = style.border.visible() ? style.border.width : 0.0f;
    const float localX = ui.latch(options.id, "ripple.x",
                                  input.pointer().x - frame.x - inset - style.padding.left,
                                  started);
    const float localY = ui.latch(options.id, "ripple.y",
                                  input.pointer().y - frame.y - inset - style.padding.top,
                                  started);

    const float contentW = std::max(0.0f, frame.width - inset * 2.0f - style.padding.horizontal());
    const float contentH = std::max(0.0f, frame.height - inset * 2.0f - style.padding.vertical());
    // The furthest corner, not the furthest edge: the ink has to clear the
    // diagonal or a press near one side leaves the opposite corner untouched.
    const float dx = std::max(localX, contentW - localX);
    const float dy = std::max(localY, contentH - localY);
    const float radius = std::hypot(dx, dy) * ease(Easing::EaseOut, progress);
    if (radius <= 0.0f) return;

    Style ink;
    ink.position = Position::Absolute;
    ink.left = localX - radius;
    ink.top = localY - radius;
    ink.width = radius * 2.0f;
    ink.height = radius * 2.0f;
    ink.radius = radius;
    // The label's own colour: on a filled button that is the readable contrast
    // against it, and on a ghost button it is the text. A hardcoded white would
    // be invisible on half the themes.
    ink.background = Fill{palette.label, ui.design().rippleAlpha * (1.0f - progress)};
    ui.add(ink);
}

}  // namespace

namespace {

NodeId buildButton(Ui& ui, const Interaction* input, std::string_view label,
                   const ButtonOptions& options) {
    ButtonPalette palette = paletteFor(options.variant);
    // Disabled is a state, not a faded variant: a primary button at 45% is
    // still the loudest thing on the row, and a danger button still reads as
    // the dangerous one. So it gives up its variant's colours entirely and
    // wears the same surface every other disabled control wears.
    if (options.disabled) {
        const detail::FieldPalette off = detail::disabledPalette();
        palette.background = off.background;
        palette.border = Border{1.0f, Fill{off.border}};
        palette.label = off.label;
    }

    Style style;
    style.direction = Direction::Row;
    style.justify = Justify::Center;
    style.align = Align::Center;
    // A floor, not a fixed size: at 22 px the label is taller than a 30-pixel
    // button, and a control that keeps its height regardless simply crops the
    // text it exists to show. This is what CSS's `min-height` is for.
    style.minHeight = options.height > 0.0f ? options.height : ui.design().controlHeight;
    style.padding = Edges::symmetric(0.0f, 12.0f);
    style.background = palette.background;
    style.border = palette.border;
    style.radius = ui.design().controlRadius;
    // A design with no outline on its controls says so once, here, rather than
    // in every variant.
    if (ui.design().borderWidth <= 0.0f && options.variant == ButtonVariant::Secondary) {
        style.border = Border{};
        style.background = Fill{Token::BgOverlay};
    } else if (style.border.visible()) {
        style.border.width = ui.design().borderWidth;
    }
    // Same reasoning as the badge: a button's label is the button.
    style.shrink = 0.0f;
    if (options.block) {
        style.grow = 1.0f;
        style.shrink = 1.0f;
    }
    style.opacity = detail::opacityFor(options.disabled);
    style.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Pointer;

    // The design decides unless this button insists otherwise.
    const bool wantsInk =
        options.ripple.value_or(ui.design().press == PressFeedback::Ripple);
    // The ink is clipped to the button, so the button has to clip.
    const bool ripple = wantsInk && input && !options.disabled && !options.id.empty();
    if (ripple) style.overflow = Overflow::Hidden;

    auto scope = ui.begin(style);
    if (!options.id.empty()) ui.tag(options.id);
    // On the button itself, not only on a tagged one: an unnamed button is
    // still a button under the pointer.
    ui.cursor(style.cursorHint);

    if (ripple) drawRipple(ui, *input, palette, style, options);

    if (options.leading) {
        icon(ui, *options.leading, {.color = palette.label, .size = 14.0f});
        // The gap belongs to the button, not to the caller.
        Style spacing;
        spacing.width = 6.0f;
        spacing.shrink = 0.0f;
        ui.add(spacing);
    }

    TextStyle textStyle;
    textStyle.weight = FontWeight::Medium;
    textStyle.align = TextAlign::Center;
    textStyle.color = Fill{palette.label};
    ui.label(label, textStyle);

    return scope.id();
}

}  // namespace

NodeId button(Ui& ui, std::string_view label, const ButtonOptions& options) {
    return buildButton(ui, nullptr, label, options);
}

NodeId button(Ui& ui, const Interaction& input, std::string_view label,
              const ButtonOptions& options) {
    return buildButton(ui, &input, label, options);
}

}  // namespace gbui
