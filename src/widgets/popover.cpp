#include "gbui/widgets/popover.hpp"

#include <algorithm>
#include <string>

#include <algorithm>

#include "detail.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

Ui::Scope popover(Ui& ui, const Interaction& input, std::string_view id,
                  std::string_view anchorId, const PopoverOptions& options) {
    const Rect anchor = input.frameOf(anchorId);
    const Rect bounds = boundsFor(input, options);

    float width = options.matchAnchorWidth && anchor.width > 0.0f
                      ? anchor.width
                      : std::clamp(input.frameOf(id).width, options.minWidth, options.maxWidth);
    // The window is a ceiling over `minWidth`, not the other way round. A
    // caller's minimum is what the contents need to be usable, and on a window
    // narrower than that the honest answer is "as much as there is": a box
    // placed wider than its bounds hangs off the edge, and the placement engine
    // can only slide it sideways, never make it fit. The calendar is where this
    // shows — 7 cells and 6 gaps is a real minimum — and the contents shrink to
    // whatever they are given, so handing them less is the recoverable half.
    const float widthAvailable = std::max(0.0f, bounds.width - 2.0f * options.margin);
    if (widthAvailable > 0.0f) width = std::min(width, widthAvailable);

    Vec2 size = estimateSize(input, id, {width, 120.0f});
    // A ceiling is part of the *placement* question, not just the drawing one:
    // a box that will be clipped to 240 px must be placed as a 240-px box, or
    // it flips above an anchor it would have fitted below.
    if (!isAuto(options.maxHeight)) size.y = std::min(size.y, options.maxHeight);

    PlacementResult placed = place(anchor, {width, size.y}, bounds, placementOptionsFrom(options));

    // How much room the side it landed on actually has. The placement engine
    // already flipped to the roomier side if it could; this is what is left
    // there, and a popup taller than that would hang off the window with its
    // last rows unreachable.
    float ceiling = options.maxHeight;
    if (!options.allowOverflow) {
        const bool above = placed.placement == Placement::Top;
        const float room = above ? anchor.y - bounds.y - options.gap - options.margin
                                 : bounds.bottom() - anchor.bottom() - options.gap - options.margin;
        const float available = std::max(0.0f, room);
        ceiling = isAuto(ceiling) ? available : std::min(ceiling, available);
        if (size.y > ceiling) {
            // Re-placed at the size it will really be, so a popup pinned to the
            // ceiling still sits against its anchor rather than where a taller
            // one would have started.
            size.y = ceiling;
            placed = place(anchor, {width, size.y}, bounds, placementOptionsFrom(options));
        }
    }

    // Without an inner scroll the surface itself carries the padding and the
    // gap; with one they belong to the scrolled content, or the padding would
    // scroll away with it.
    const bool scrolls = options.scrollState != nullptr && options.scroll != ScrollAxis::None;
    auto scope = floating(ui, Rect{placed.rect.x, placed.rect.y, width, 0.0f}, Layer::Overlay,
                          scrolls ? Edges{} : options.padding,
                          scrolls ? 0.0f : options.gapBetweenItems, Direction::Column, ceiling);
    ui.tag(id);
    if (!scrolls) return scope;

    ScrollOptions inner;
    inner.axis = options.scroll;
    inner.direction = Direction::Column;
    inner.padding = options.padding;
    inner.gap = options.gapBetweenItems;
    inner.grow = 0.0f;
    // The popover is already bounded; the view inside it simply fills whatever
    // that left, and scrolls when its content does not.
    inner.maxHeight = isAuto(ceiling) ? kAuto : ceiling - options.padding.vertical();
    // The surface owns the keyboard stop, not the view inside it.
    inner.focusable = false;
    auto view = scrollArea(ui, input, std::string(id) + ".view", *options.scrollState, inner);
    // One scope closes both, the same way `scrollArea` hands back one for its
    // viewport and its content.
    view.adopt();
    scope.disown();
    return view;
}

}  // namespace gbui
