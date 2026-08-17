#include "gbui/widgets/chip.hpp"

#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

using namespace detail;

ChipResult chip(Ui& ui, const Interaction& input, std::string_view id, std::string_view label,
                const ChipOptions& options) {
    ChipResult result;
    const std::string removeId = std::string(id) + ".remove";
    const bool hovered = input.isHovered(id);

    Style pill;
    pill.direction = Direction::Row;
    pill.align = Align::Center;
    pill.gap = 6.0f;
    pill.height = 24.0f;
    pill.minHeight = 0.0f;
    pill.shrink = 0.0f;
    // Tighter on the trailing side when there is an × there, so the cross sits
    // in from the edge by the same amount the label does on the other side.
    pill.padding = Edges{0.0f, options.removable ? 6.0f : 10.0f, 0.0f, 10.0f};
    pill.radius = 12.0f;
    pill.opacity = opacityFor(options.disabled);
    pill.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Pointer;
    if (options.selected) {
        pill.background = Fill{options.color, hovered ? 0.30f : 0.22f};
        pill.border = Border{1.0f, Fill{options.color, 0.55f}};
    } else {
        pill.background = Fill{Token::BgOverlay, hovered ? 0.85f : 0.5f};
        pill.border = Border{1.0f, Fill{hovered ? Token::BorderStrong : Token::Border}};
    }
    if (input.isFocusVisible(id)) pill.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};

    auto scope = ui.scope(pill);
    ui.tag(id).focusable(!options.disabled).cursor(pill.cursorHint);
    // A toggle button, which is what a filter chip is: `pressed` is the state
    // that says "this one is on", and a reader running through a filter bar is
    // told which of them are without having to see the fill.
    ui.accessible({
        .role = Role::Button,
        .name = options.name.empty() ? label : options.name,
        .state = {.pressed = flag(options.selected), .disabled = flag(options.disabled)},
    });

    if (options.leading) {
        icon(ui, *options.leading,
             {.color = options.selected ? options.color : Token::TextMuted, .size = 13.0f});
    }
    text(ui, label, {.color = options.selected ? Token::TextStrong : Token::Text, .size = 12.0f});

    if (options.removable) {
        Style cross;
        cross.width = 16.0f;
        cross.height = 16.0f;
        cross.minWidth = 0.0f;
        cross.minHeight = 0.0f;
        cross.shrink = 0.0f;
        cross.justify = Justify::Center;
        cross.align = Align::Center;
        cross.radius = 8.0f;
        if (input.isHovered(removeId)) cross.background = Fill{Token::SurfaceHover};
        if (input.isFocusVisible(removeId)) {
            cross.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
        }
        {
            auto crossScope = ui.scope(cross);
            ui.tag(removeId).focusable(!options.disabled).cursor(Cursor::Pointer);
            // Its own name, built from the label: a filter bar of six chips
            // whose remove buttons are all called "Remove" is six buttons a
            // reader cannot tell apart.
            ui.accessible({.role = Role::Button,
                           .name = "Remove " + std::string(label),
                           .state = {.disabled = flag(options.disabled)}});
            icon(ui, Icon::X, {.color = Token::TextMuted, .size = 11.0f});
            (void)crossScope;
        }
    }
    scope.close();

    if (options.disabled) return result;

    if (options.removable) {
        if (activated(input, removeId, false)) result.removed = true;
        // Delete and Backspace on the chip itself, which is what every tag
        // field on the web does and the only way to take one off from the
        // keyboard without tabbing into a second control for each.
        if (input.isFocused(id)) {
            for (const KeyEvent& event : input.keys()) {
                if (event.key == Key::Delete || event.key == Key::Backspace) result.removed = true;
            }
        }
    }
    // A press on the × is not a press on the chip: removing one would toggle
    // it on the way out, and the reader would see the filter flip as it left.
    if (!result.removed && activated(input, id, false)) result.pressed = true;
    return result;
}

}  // namespace gbui
