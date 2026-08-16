#include "gbui/widgets/menu.hpp"

#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

bool menuItem(Ui& ui, const Interaction& input, std::string_view id, std::string_view label,
              const MenuItemOptions& options) {
    const bool hovered = input.isHovered(id);
    const bool focused = input.isFocused(id);

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.height = kMenuItemHeight;
    row.gap = 8.0f;
    row.padding = Edges::symmetric(0.0f, 8.0f);
    row.radius = 4.0f;
    row.opacity = options.disabled ? 0.45f : 1.0f;
    if ((hovered || options.highlighted) && !options.disabled) {
        row.background = Fill{Token::SurfaceHover};
    }
    if (input.isFocusVisible(id)) row.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};

    const Token labelColor = options.danger ? Token::Removed
                             : options.selected ? Token::TextStrong
                                                : Token::Text;

    row.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Pointer;

    auto scope = ui.scope(row);
    ui.tag(id).focusable(options.focusable && !options.disabled).cursor(row.cursorHint);
    // A plain command says nothing about being checked unless it *is*, because
    // "not checked" on every row of a menu is nine words of nothing. A row that
    // is a checkbox says so either way, which is the whole difference between
    // `MenuItem` and `MenuItemCheckbox`.
    const bool listOption = options.role == Role::Option;
    const bool toggles = options.role == Role::MenuItemCheckbox ||
                         options.role == Role::MenuItemRadio;
    ui.accessible({
        .role = options.role,
        .name = label,
        .description = options.shortcut,
        .state = {.checked = listOption           ? Flag::Unset
                             : toggles            ? flag(options.selected)
                             : options.selected   ? Flag::True
                                                  : Flag::Unset,
                  .selected = listOption ? flag(options.selected) : Flag::Unset,
                  .disabled = flag(options.disabled)},
        .positionInSet = options.positionInSet,
        .setSize = options.setSize,
    });

    const bool tickLeading =
        options.selected && options.checkSide == CheckSide::Leading && !options.leading;
    if (options.leading) {
        icon(ui, *options.leading, {.color = labelColor, .size = 14.0f});
    } else if (tickLeading) {
        icon(ui, Icon::Check, {.color = Token::Accent, .size = 14.0f});
    }

    text(ui, label, {.color = labelColor, .grow = 1.0f});

    if (!options.shortcut.empty()) {
        text(ui, options.shortcut, {.color = Token::TextMuted, .role = FontRole::Mono,
                                    .size = 11.0f});
    }
    if (options.selected && options.checkSide == CheckSide::Trailing) {
        // In a box that refuses to shrink. The label before it grows to fill
        // the row, which leaves nothing for anything after it — a trailing icon
        // with the default shrink is squeezed to nothing and simply never
        // appears. The leading tick never hit this because it is laid out
        // before the label takes the space.
        Style slot;
        slot.width = 16.0f;
        slot.shrink = 0.0f;
        slot.justify = Justify::End;
        slot.align = Align::Center;
        auto slotScope = ui.scope(slot);
        icon(ui, Icon::Check, {.color = Token::Accent, .size = 14.0f});
        (void)slotScope;
    }
    (void)scope;

    if (options.disabled) return false;
    if (input.clicked(id)) return true;
    if (!focused) return false;
    for (const KeyEvent& event : input.keys()) {
        if (event.key == Key::Return || event.key == Key::Space) return true;
    }
    return false;
}

void menuSeparator(Ui& ui) {
    Style rule;
    rule.height = 1.0f;
    rule.margin = Edges::symmetric(4.0f, 0.0f);
    rule.radius = 0.0f;
    rule.background = Fill{Token::Border};
    ui.add(rule);
    // A rule between groups is a boundary a reader should hear about: it is the
    // only thing saying that "Delete branch" is not in the same group as the
    // three items above it.
    ui.role(Role::Separator);
}

}  // namespace gbui
