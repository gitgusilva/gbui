#include "gbui/overlay/placement.hpp"

#include <algorithm>
#include <array>

namespace gbui {
namespace {

struct Entry {
    Placement placement;
    std::string_view name;
};

constexpr std::array<Entry, 13> kNames{{
    {Placement::Auto, "auto"},
    {Placement::Top, "top"},
    {Placement::TopStart, "top-start"},
    {Placement::TopEnd, "top-end"},
    {Placement::Bottom, "bottom"},
    {Placement::BottomStart, "bottom-start"},
    {Placement::BottomEnd, "bottom-end"},
    {Placement::Left, "left"},
    {Placement::LeftStart, "left-start"},
    {Placement::LeftEnd, "left-end"},
    {Placement::Right, "right"},
    {Placement::RightStart, "right-start"},
    {Placement::RightEnd, "right-end"},
}};

bool isVertical(Placement side) {
    return side == Placement::Top || side == Placement::Bottom;
}

/** Alignment along the side: -1 start, 0 centre, +1 end. */
int alignmentOf(Placement placement) {
    switch (placement) {
        case Placement::TopStart:
        case Placement::BottomStart:
        case Placement::LeftStart:
        case Placement::RightStart: return -1;
        case Placement::TopEnd:
        case Placement::BottomEnd:
        case Placement::LeftEnd:
        case Placement::RightEnd: return 1;
        default: return 0;
    }
}

Placement withAlignment(Placement side, int alignment) {
    switch (side) {
        case Placement::Top:
            return alignment < 0 ? Placement::TopStart
                                 : (alignment > 0 ? Placement::TopEnd : Placement::Top);
        case Placement::Bottom:
            return alignment < 0 ? Placement::BottomStart
                                 : (alignment > 0 ? Placement::BottomEnd : Placement::Bottom);
        case Placement::Left:
            return alignment < 0 ? Placement::LeftStart
                                 : (alignment > 0 ? Placement::LeftEnd : Placement::Left);
        default:
            return alignment < 0 ? Placement::RightStart
                                 : (alignment > 0 ? Placement::RightEnd : Placement::Right);
    }
}

Placement opposite(Placement side) {
    switch (side) {
        case Placement::Top: return Placement::Bottom;
        case Placement::Bottom: return Placement::Top;
        case Placement::Left: return Placement::Right;
        default: return Placement::Left;
    }
}

/** Room on each side of the anchor, once the gap and margin are paid for. */
float roomOn(Placement side, const Rect& anchor, const Rect& bounds, float cost) {
    switch (side) {
        case Placement::Top: return anchor.y - bounds.y - cost;
        case Placement::Bottom: return bounds.bottom() - anchor.bottom() - cost;
        case Placement::Left: return anchor.x - bounds.x - cost;
        default: return bounds.right() - anchor.right() - cost;
    }
}

Rect positionOn(Placement side, int alignment, const Rect& anchor, Vec2 size, float gap) {
    Rect rect{0, 0, size.x, size.y};
    if (isVertical(side)) {
        rect.y = side == Placement::Top ? anchor.y - gap - size.y : anchor.bottom() + gap;
        rect.x = alignment < 0   ? anchor.x
                 : alignment > 0 ? anchor.right() - size.x
                                 : anchor.x + (anchor.width - size.x) / 2.0f;
    } else {
        rect.x = side == Placement::Left ? anchor.x - gap - size.x : anchor.right() + gap;
        rect.y = alignment < 0   ? anchor.y
                 : alignment > 0 ? anchor.bottom() - size.y
                                 : anchor.y + (anchor.height - size.y) / 2.0f;
    }
    return rect;
}

bool fitsOn(Placement side, const Rect& rect, const Rect& bounds, float margin) {
    switch (side) {
        case Placement::Top: return rect.y >= bounds.y + margin;
        case Placement::Bottom: return rect.bottom() <= bounds.bottom() - margin;
        case Placement::Left: return rect.x >= bounds.x + margin;
        default: return rect.right() <= bounds.right() - margin;
    }
}

}  // namespace

std::string_view placementName(Placement placement) {
    for (const Entry& entry : kNames) {
        if (entry.placement == placement) return entry.name;
    }
    return "auto";
}

std::optional<Placement> placementFromName(std::string_view name) {
    for (const Entry& entry : kNames) {
        if (entry.name == name) return entry.placement;
    }
    return std::nullopt;
}

Placement sideOf(Placement placement) {
    switch (placement) {
        case Placement::Top:
        case Placement::TopStart:
        case Placement::TopEnd: return Placement::Top;
        case Placement::Left:
        case Placement::LeftStart:
        case Placement::LeftEnd: return Placement::Left;
        case Placement::Right:
        case Placement::RightStart:
        case Placement::RightEnd: return Placement::Right;
        default: return Placement::Bottom;
    }
}

PlacementResult place(const Rect& anchor, Vec2 size, const Rect& bounds,
                      const PlacementOptions& options) {
    const int alignment = alignmentOf(options.preferred);

    Placement side;
    if (options.preferred == Placement::Auto) {
        // The side with the most room wins. Below first on a tie, because that
        // is where a reader expects a menu and where the pointer is not.
        side = Placement::Bottom;
        float best = -1e30f;
        for (const Placement candidate :
             {Placement::Bottom, Placement::Top, Placement::Right, Placement::Left}) {
            const float needed = isVertical(candidate) ? size.y : size.x;
            const float room = roomOn(candidate, anchor, bounds, options.gap + options.margin);
            // A side that fits outright ends the search; otherwise keep the
            // roomiest, so a box too big for anywhere still lands sensibly.
            if (room >= needed) {
                side = candidate;
                best = 1e30f;
                break;
            }
            if (room > best) {
                best = room;
                side = candidate;
            }
        }
    } else {
        side = sideOf(options.preferred);
    }

    Rect rect = positionOn(side, alignment, anchor, size, options.gap);
    bool flipped = false;

    if (options.flip && !fitsOn(side, rect, bounds, options.margin)) {
        const Placement other = opposite(side);
        const Rect alternative = positionOn(other, alignment, anchor, size, options.gap);
        // Only flip when the other side is genuinely better; flipping into an
        // equally bad position just makes the thing jump.
        if (fitsOn(other, alternative, bounds, options.margin)) {
            rect = alternative;
            side = other;
            flipped = true;
        }
    }

    if (options.shift) {
        // Slide along the cross axis — never along the side axis, which would
        // detach the box from its anchor.
        if (isVertical(side)) {
            rect.x = std::clamp(rect.x, bounds.x + options.margin,
                                std::max(bounds.x + options.margin,
                                         bounds.right() - options.margin - rect.width));
        } else {
            rect.y = std::clamp(rect.y, bounds.y + options.margin,
                                std::max(bounds.y + options.margin,
                                         bounds.bottom() - options.margin - rect.height));
        }
    }

    return {rect, withAlignment(side, alignment), flipped};
}

}  // namespace gbui
