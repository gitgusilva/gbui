#include "gbui/widgets/banner.hpp"

#include <string>

#include "detail.hpp"
#include "gbui/widgets/button.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

namespace {

Token toneOf(BannerKind kind) {
    switch (kind) {
        case BannerKind::Success: return Token::Added;
        case BannerKind::Warning: return Token::Modified;
        case BannerKind::Danger: return Token::Removed;
        case BannerKind::Info: break;
    }
    return Token::Accent;
}

/**
 * A glyph per kind, and the reason each of them is a different *shape*.
 *
 * This palette has twenty-four tokens and no amber in it — the token list is a
 * contract shared with the theme registry, not somewhere to invent a colour —
 * so `Warning` borrows `Modified`, which is a blue. Blue against the accent's
 * blue makes a warning and a note look alike, and colour was never a safe
 * signal on its own anyway: a reader who cannot separate the two hues is a
 * reader the colour told nothing.
 *
 * So the shape carries it. A triangle is a warning everywhere on earth, a
 * circled `i` is a note, a tick is done and a circled `!` is a failure, and all
 * four read at a glance in one colour.
 */
Icon glyphOf(BannerKind kind) {
    switch (kind) {
        case BannerKind::Success: return Icon::CircleCheck;
        case BannerKind::Warning: return Icon::TriangleAlert;
        case BannerKind::Danger: return Icon::CircleAlert;
        case BannerKind::Info: break;
    }
    return Icon::Info;
}

}  // namespace

BannerResult banner(Ui& ui, const Interaction& input, std::string_view id, std::string_view title,
                    const BannerOptions& options) {
    BannerResult result;
    const Token tone = toneOf(options.kind);
    const std::string closeId = std::string(id) + ".close";
    const std::string actionId = std::string(id) + ".action";

    Style box;
    box.direction = Direction::Row;
    box.align = Align::Start;
    box.gap = 10.0f;
    box.padding = Edges::symmetric(10.0f, 12.0f);
    box.radius = 8.0f;
    box.background = Fill{tone, 0.12f};
    box.border = Border{1.0f, Fill{tone, 0.4f}};

    auto scope = ui.scope(box);
    ui.tag(id);
    // `Alert` interrupts and `Status` waits for a pause. A merge conflict is
    // the first and "saved" is the second, and picking the wrong one either
    // talks over the reader or tells them too late.
    ui.accessible({
        .role = options.kind == BannerKind::Danger || options.kind == BannerKind::Warning
                    ? Role::Alert
                    : Role::Status,
        .name = title,
    });

    icon(ui, options.icon.value_or(glyphOf(options.kind)), {.color = tone, .size = 16.0f});

    {
        Style column;
        column.direction = Direction::Column;
        column.gap = 2.0f;
        column.grow = 1.0f;
        column.basis = 0.0f;
        column.minWidth = 0.0f;
        auto columnScope = ui.scope(column);
        text(ui, title,
             {.color = Token::TextStrong, .weight = FontWeight::Medium,
              .overflow = TextOverflow::Wrap});
        if (!options.detail.empty()) {
            text(ui, options.detail,
                 {.color = Token::TextMuted, .size = 12.0f, .overflow = TextOverflow::Wrap});
        }
        (void)columnScope;
    }

    if (!options.action.empty()) {
        if (button(ui, input, options.action,
                   {.variant = ButtonVariant::Secondary, .height = 26.0f, .id = actionId})) {
            result.acted = true;
        }
    }

    if (options.closable) {
        Style close;
        close.width = 22.0f;
        close.height = 22.0f;
        close.minWidth = 0.0f;
        close.minHeight = 0.0f;
        close.shrink = 0.0f;
        close.justify = Justify::Center;
        close.align = Align::Center;
        close.radius = 4.0f;
        if (input.isHovered(closeId)) close.background = Fill{Token::SurfaceHover};
        if (input.isFocusVisible(closeId)) close.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
        {
            auto closeScope = ui.scope(close);
            ui.tag(closeId).focusable().cursor(Cursor::Pointer);
            // Named after the banner rather than "Close": a screen with three
            // of these otherwise has three buttons a reader cannot tell apart.
            ui.accessible({.role = Role::Button, .name = "Dismiss " + std::string(title)});
            icon(ui, Icon::X, {.color = Token::TextMuted, .size = 13.0f});
            (void)closeScope;
        }
        if (detail::activated(input, closeId, false)) result.dismissed = true;
    }
    scope.close();
    return result;
}

}  // namespace gbui
