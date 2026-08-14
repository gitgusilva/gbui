#include "gbui/widgets/modal.hpp"

#include <algorithm>

#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"
#include "gbui/widgets/spacing.hpp"

namespace gbui {

Modal modal(Ui& ui, const Interaction& input, std::string_view id, std::string_view title,
            Vec2 position, const ModalOptions& options) {
    ModalResult result;
    const Rect bounds = input.viewport().empty() ? Rect{0, 0, 1280, 720} : input.viewport();

    const std::string headerId = std::string(id) + ".header";
    const std::string closeId = std::string(id) + ".close";
    const std::string backdropId = std::string(id) + ".backdrop";

    const Rect known = input.frameOf(id);
    // A dialog wider or taller than the window is not a dialog anybody can use:
    // the window is the ceiling, whatever the caller asked for.
    const float width = std::min(options.width, bounds.width);
    const float height = std::min(known.height > 0.0f ? known.height : 180.0f, bounds.height);

    // First frame, or a caller that has not placed it: centre it.
    Vec2 place = position;
    if (place.x == 0.0f && place.y == 0.0f) {
        place = {bounds.x + (bounds.width - width) / 2.0f,
                 bounds.y + (bounds.height - height) / 2.5f};
    }

    // Dragging by the header. The delta is applied to the caller's position,
    // so the dialog stays where it was put when the tree is rebuilt.
    if (options.draggable && input.dragging() == headerId) {
        const Vec2 delta = input.pointerDelta();
        place.x += delta.x;
        place.y += delta.y;
    }
    // Kept *wholly* inside the window, not merely reachable. Letting a dialog
    // hang off the edge with a strip still on screen is how one ends up with
    // its buttons outside the window — and this also runs when the window is
    // resized, so shrinking the window pulls the dialog back in rather than
    // leaving it beyond the new edge.
    //
    // The `max` guards the case where the dialog is as large as the window: the
    // range would otherwise invert and clamp to the wrong end.
    place.x = std::clamp(place.x, bounds.x, std::max(bounds.x, bounds.right() - width));
    place.y = std::clamp(place.y, bounds.y, std::max(bounds.y, bounds.bottom() - height));
    result.position = place;

    if (options.backdrop) {
        Style backdrop;
        backdrop.position = Position::Fixed;
        backdrop.layer = Layer::Modal;
        backdrop.left = bounds.x;
        backdrop.top = bounds.y;
        backdrop.width = bounds.width;
        backdrop.height = bounds.height;
        backdrop.radius = 0.0f;
        backdrop.background = Fill{Color{0, 0, 0}, 0.55f};
        ui.add(backdrop);
        ui.tag(backdropId);
        if (input.clicked(backdropId)) result.dismissed = true;
    }

    Style dialog;
    dialog.position = Position::Fixed;
    dialog.layer = Layer::Modal;
    dialog.left = place.x;
    dialog.top = place.y;
    dialog.width = width;
    dialog.height = options.height;
    dialog.maxWidth = bounds.width;
    dialog.maxHeight = bounds.height;
    dialog.direction = Direction::Column;
    dialog.background = Fill{Token::BgElevated};
    dialog.border = Border{1.0f, Fill{Token::BorderStrong}};
    dialog.overflow = Overflow::Hidden;

    auto body = ui.scope(dialog);
    ui.tag(id);

    {
        Style header;
        header.direction = Direction::Row;
        header.align = Align::Center;
        header.height = 44.0f;
        header.gap = 8.0f;
        header.padding = Edges::symmetric(0.0f, 14.0f);
        header.radius = 0.0f;
        header.background = Fill{Token::BgOverlay};
        auto headerScope = ui.scope(header);
        ui.tag(headerId);

        if (options.icon) {
            icon(ui, *options.icon,
                 {.color = options.danger ? Token::Removed : Token::TextMuted, .size = 15.0f});
        }
        text(ui, title, {.color = options.danger ? Token::Removed : Token::TextStrong,
                         .weight = FontWeight::SemiBold, .grow = 1.0f});

        Style close;
        close.width = 24.0f;
        close.height = 24.0f;
        close.shrink = 0.0f;
        close.justify = Justify::Center;
        close.align = Align::Center;
        close.radius = 4.0f;
        if (input.isHovered(closeId)) close.background = Fill{Token::SurfaceHover};
        if (input.isFocusVisible(closeId)) close.outline = Outline{2.0f, 1.0f, Fill{Token::Accent}};
        {
            auto closeScope = ui.scope(close);
            ui.tag(closeId).focusable();
            icon(ui, Icon::X, {.color = Token::TextMuted, .size = 14.0f});
            (void)closeScope;
        }
        if (input.clicked(closeId)) result.dismissed = true;
        (void)headerScope;
    }

    divider(ui, Direction::Column);

    for (const KeyEvent& event : input.keys()) {
        if (event.key == Key::Escape) result.dismissed = true;
    }

    return Modal{std::move(body), result};
}

Ui::Scope modalActions(Ui& ui) {
    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.justify = Justify::End;
    row.gap = 8.0f;
    row.padding = Edges::all(14.0f);
    auto scope = ui.scope(row);
    return scope;
}

}  // namespace gbui
