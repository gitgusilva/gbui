#include "gbui/widgets/hyperlink.hpp"

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/platform/shell.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

bool hyperlink(Ui& ui, const Interaction& input, std::string_view id, std::string_view label,
          const HyperlinkOptions& options) {
    const bool hovered = input.isHovered(id);
    const bool ring = input.isFocusVisible(id);
    const bool underlined = options.underlineOnHover ? (hovered || ring) : true;

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 4.0f;
    row.opacity = opacityFor(options.disabled);
    row.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Hand;
    if (ring) row.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};

    auto scope = ui.scope(row);
    ui.tag(id).focusable(!options.disabled).cursor(row.cursorHint);
    // The href is the description rather than the name: a reader wants to hear
    // "Open on GitHub, link" and to be able to ask where it goes, not to have
    // a URL read out in place of the words on screen.
    ui.accessible({
        .role = Role::Link,
        .name = label,
        .description = options.href,
        .state = {.disabled = flag(options.disabled)},
    });

    {
        // The rule is a sibling under the text rather than a text decoration,
        // because the painter has no notion of one — and this way its colour
        // and thickness are the component's to decide.
        Style stack;
        stack.direction = Direction::Column;
        stack.gap = 1.0f;
        auto stackScope = ui.scope(stack);
        text(ui, label, {.color = hovered && !options.disabled ? Token::AccentHover : options.color,
                         .size = options.size});
        Style rule;
        rule.height = 1.0f;
        rule.radius = 0.0f;
        rule.background = underlined ? Fill{hovered ? Token::AccentHover : options.color}
                                     : Fill{};
        ui.add(rule);
        (void)stackScope;
    }

    if (options.trailing) {
        icon(ui, *options.trailing, {.color = options.color, .size = 12.0f});
    }
    (void)scope;

    if (!activated(input, id, options.disabled)) return false;

    // The chord, if one was asked for. Compared field by field rather than by
    // "at least these": a link wanting Ctrl should not also fire on
    // Ctrl+Shift, which is usually somebody's shortcut for something else.
    const Modifiers& held = input.modifiers();
    const Modifiers& want = options.chord;
    const bool chordMet = held.shift == want.shift && held.ctrl == want.ctrl &&
                          held.alt == want.alt && held.super == want.super;
    if (!chordMet) return false;

    // Reported as followed either way. A caller that gave no `href` is doing
    // its own navigation, and one whose URL the platform refused still asked.
    if (!options.href.empty()) (void)openUrl(options.href);
    return true;
}

}  // namespace gbui
