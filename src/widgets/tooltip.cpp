#include "gbui/widgets/tooltip.hpp"

#include <algorithm>

#include "detail.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

void tooltip(Ui& ui, const Interaction& input, std::string_view anchorId, std::string_view text,
             const TooltipOptions& options) {
    const std::string id = std::string(anchorId) + ".tooltip";
    const bool hovered = input.isHovered(anchorId);

    // When the pointer arrived. Latched on every frame it is *not* on the
    // anchor, so the moment it lands the stored time is the moment before —
    // which is why this runs before the early return rather than after it.
    const float restingSince = ui.latch(id, "since", ui.now(), !hovered);
    if (!hovered) return;
    if (ui.animator() && ui.now() - restingSince < options.delay) return;

    const Rect anchor = input.frameOf(anchorId);
    if (anchor.width <= 0.0f) return;   // the anchor has not been laid out yet

    constexpr float kFontSize = 12.0f;
    const Edges kPadding = Edges::symmetric(5.0f, 9.0f);
    constexpr float kBorder = 1.0f;
    const float frameWidth = kPadding.horizontal() + kBorder * 2.0f;
    const float frameHeight = kPadding.vertical() + kBorder * 2.0f;

    TextStyle runStyle;
    runStyle.color = Fill{Token::TextStrong};
    runStyle.size = kFontSize;
    if (options.wrap) runStyle.overflow = TextOverflow::Wrap;

    // Measured, not estimated.
    //
    // This is the whole of the bug that made a tooltip appear in the wrong
    // place and then jump: it was placed from a guessed width, and corrected
    // once layout had run. The guess and the truth differ by however wrong the
    // guess was, so the box visibly moved — which reads as a botched entry
    // animation rather than as the mistake it is. The builder has the same
    // measurer layout uses, so there is no reason to guess.
    Vec2 size;
    if (ui.canMeasure()) {
        TextStyle natural = runStyle;
        natural.overflow = TextOverflow::Clip;  // ask for the unwrapped width
        const float wanted = ui.measure(text, natural).width + frameWidth;
        size.x = std::min(options.maxWidth, wanted);
        size.y = ui.measure(text, runStyle, size.x - frameWidth).height + frameHeight;
    } else {
        const float estimated = std::min(options.maxWidth,
                                         measureTextWidth(text, kFontSize) + frameWidth);
        size = estimateSize(input, id, {estimated, 26.0f});
    }

    const PlacementResult placed =
        place(anchor, size, boundsFor(input, options), placementOptionsFrom(options));

    Style surface;
    surface.position = Position::Fixed;
    surface.layer = Layer::Overlay;
    surface.left = placed.rect.x;
    surface.top = placed.rect.y;
    // The width is decided here rather than left to the content, so what was
    // placed and what is drawn are the same box.
    surface.width = size.x;
    surface.maxWidth = options.maxWidth;
    surface.padding = kPadding;
    surface.align = Align::Center;
    surface.background = Fill{Token::BgOverlay};
    surface.border = Border{kBorder, Fill{Token::BorderStrong}};

    auto scope = ui.begin(surface);
    ui.tag(id).ignoresPointer();
    gbui::text(ui, text,
               {.color = Token::TextStrong, .size = kFontSize,
                .overflow = options.wrap ? TextOverflow::Wrap : TextOverflow::Ellipsis});
    (void)scope;
}

}  // namespace gbui
