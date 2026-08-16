#include "gbui/widgets/drawer.hpp"

#include <algorithm>
#include <string>

#include "gbui/widgets/floating.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/spacing.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

namespace {

/** True for the two edges a drawer slides along horizontally. */
bool horizontal(DrawerSide side) {
    return side == DrawerSide::Left || side == DrawerSide::Right;
}

}  // namespace

Drawer drawer(Ui& ui, const Interaction& input, std::string_view id, std::string_view title,
              bool open, const DrawerOptions& options) {
    DrawerResult result;
    const Rect bounds = !options.bounds.empty() ? options.bounds
                        : input.viewport().empty() ? Rect{0, 0, 1280, 720}
                                                   : input.viewport();

    // ---- how far in it is --------------------------------------------------
    //
    // One number, 0 shut and 1 open, and everything below is a function of it.
    // Without an animator `animate` hands back the target, so the panel simply
    // appears — which is what every animated thing in this library does when
    // nobody is driving a clock.
    const float shown = ui.animate(id, "drawer.open", open ? 1.0f : 0.0f,
                                   {.duration = std::max(0.0f, options.duration),
                                    .easing = Easing::EaseOut});

    // Closed, and finished closing. A zero-sized clipped box out of the flow,
    // so a caller can write into `body` unconditionally without it landing in
    // whatever container happens to be current — and without paying for a
    // panel nobody can see.
    if (shown <= 0.001f) {
        Style away;
        away.position = Position::Absolute;
        away.width = 0.0f;
        away.height = 0.0f;
        away.minWidth = 0.0f;
        away.minHeight = 0.0f;
        away.overflow = Overflow::Hidden;
        auto empty = ui.scope(away);
        ui.accessible({.hidden = true});
        return Drawer{std::move(empty), result};
    }
    result.visible = true;

    const std::string headerId = std::string(id) + ".header";
    const std::string closeId = std::string(id) + ".close";
    const std::string backdropId = std::string(id) + ".backdrop";
    const bool blocking = options.modal;

    // The window is the ceiling. A 400-pixel drawer on a 320-pixel window is a
    // 320-pixel drawer, not one with its close button past the edge.
    const float across = horizontal(options.side) ? bounds.width : bounds.height;
    const float size = std::clamp(options.size, 0.0f, across);
    // How much of it is still outside, which is what the slide actually is.
    const float hidden = size * (1.0f - shown);

    // ---- the backdrop ------------------------------------------------------
    //
    // Only when it blocks: a non-modal drawer dimming a window it does not
    // block would be telling the reader they cannot use something they can.
    if (blocking && options.backdrop) {
        Style backdrop;
        backdrop.position = Position::Fixed;
        backdrop.layer = Layer::Modal;
        backdrop.left = bounds.x;
        backdrop.top = bounds.y;
        backdrop.width = bounds.width;
        backdrop.height = bounds.height;
        backdrop.radius = 0.0f;
        // Fades with the panel rather than snapping, or the dim arrives before
        // the thing it is dimming for.
        backdrop.background = Fill{Color{0, 0, 0}, 0.55f * shown};
        ui.add(backdrop);
        ui.tag(backdropId);
        if (options.dismissOnBackdrop && input.clicked(backdropId)) result.dismissed = true;
    }

    // ---- the panel ---------------------------------------------------------
    Style panel;
    panel.position = Position::Fixed;
    panel.layer = Layer::Modal;
    panel.direction = Direction::Column;
    panel.background = Fill{Token::BgElevated};
    panel.overflow = Overflow::Hidden;
    panel.radius = 0.0f;
    // Only the edge it is not attached to gets a border: the other three sit
    // against the window, where a line is a line drawn on the window frame.
    switch (options.side) {
        case DrawerSide::Left:
            panel.left = bounds.x - hidden;
            panel.top = bounds.y;
            panel.width = size;
            panel.height = bounds.height;
            break;
        case DrawerSide::Right:
            panel.left = bounds.right() - size + hidden;
            panel.top = bounds.y;
            panel.width = size;
            panel.height = bounds.height;
            break;
        case DrawerSide::Top:
            panel.left = bounds.x;
            panel.top = bounds.y - hidden;
            panel.width = bounds.width;
            panel.height = size;
            break;
        case DrawerSide::Bottom:
            panel.left = bounds.x;
            panel.top = bounds.bottom() - size + hidden;
            panel.width = bounds.width;
            panel.height = size;
            break;
    }
    panel.border = Border{1.0f, Fill{Token::BorderStrong}};

    auto body = ui.scope(panel);
    // The keyboard is confined to it while it blocks, for the same reason
    // `modal` does it: Tab walking out of the back and into a page the backdrop
    // says cannot be used is a keyboard nobody can find. A non-modal drawer
    // does *not* trap — it is a pane beside the work, and trapping there would
    // strand the reader in an inspector.
    ui.tag(id);
    if (blocking) ui.trapsFocus();
    ui.accessible({.role = Role::Dialog, .name = title});

    if (options.header) {
        Style bar;
        bar.direction = Direction::Row;
        bar.align = Align::Center;
        bar.height = 44.0f;
        bar.shrink = 0.0f;
        bar.gap = 8.0f;
        bar.padding = Edges::symmetric(0.0f, 14.0f);
        bar.radius = 0.0f;
        bar.background = Fill{Token::BgOverlay};
        auto barScope = ui.scope(bar);
        ui.tag(headerId);

        if (options.icon) icon(ui, *options.icon, {.color = Token::TextMuted, .size = 15.0f});
        text(ui, title,
             {.color = Token::TextStrong, .weight = FontWeight::SemiBold, .grow = 1.0f});

        if (options.closeButton) {
            Style close;
            close.width = 24.0f;
            close.height = 24.0f;
            close.shrink = 0.0f;
            close.justify = Justify::Center;
            close.align = Align::Center;
            close.radius = 4.0f;
            if (input.isHovered(closeId)) close.background = Fill{Token::SurfaceHover};
            if (input.isFocusVisible(closeId)) {
                close.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
            }
            {
                auto closeScope = ui.scope(close);
                ui.tag(closeId).focusable();
                // An × has no name to borrow, so it is given one.
                ui.accessible({.role = Role::Button, .name = "Close"});
                icon(ui, Icon::X, {.color = Token::TextMuted, .size = 14.0f});
                (void)closeScope;
            }
            if (input.clicked(closeId)) result.dismissed = true;
        }
        barScope.close();
        divider(ui, Direction::Column);
    }

    // Escape, whatever has the keyboard inside the panel. A modal drawer holds
    // the keyboard so this is always reached; a non-modal one only sees the
    // keys when something inside it is focused, which is the right answer —
    // Escape in a pane beside the work should not close it from across the
    // window.
    if (options.dismissOnEscape) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Escape) result.dismissed = true;
        }
    }

    return Drawer{std::move(body), result};
}

}  // namespace gbui
