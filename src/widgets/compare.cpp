#include "gbui/widgets/compare.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#include "detail.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** Where a point falls inside a rectangle, 0 to 1 on each axis. */
Vec2 fractionIn(const Rect& box, Vec2 at) {
    if (box.width <= 0.0f || box.height <= 0.0f) return {};
    return {std::clamp((at.x - box.x) / box.width, 0.0f, 1.0f),
            std::clamp((at.y - box.y) / box.height, 0.0f, 1.0f)};
}

/** "60% Nord", or "60%" when the caller named nothing. The number alone says
 *  sixty percent of what, revealing what. */
std::string spoken(float position, std::string_view revealed) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", static_cast<double>(position) * 100.0);
    std::string out(buffer);
    if (!revealed.empty()) {
        out += ' ';
        out += revealed;
    }
    return out;
}

}  // namespace

CompareResult compare(Ui& ui, const Interaction& input, std::string_view id, float position,
                      const std::function<void(Ui&)>& before,
                      const std::function<void(Ui&)>& after,
                      const CompareOptions& options) {
    const bool vertical = options.orientation == CompareOrientation::Vertical;
    const std::string handleId = std::string(id) + ".handle";

    float next = std::clamp(position, 0.0f, 1.0f);

    // ---- the gestures ------------------------------------------------------
    // Last frame's rectangle and the live pointer, so a drag keeps working
    // after the pointer leaves the box — which is the whole point at either
    // end, where the interesting part of a comparison is.
    const Rect frame = input.frameOf(id);
    const bool dragging = input.dragging() == id || input.dragging() == handleId;
    const bool hovered = input.isHovered(id) || input.isHovered(handleId);
    if (!frame.empty() && (dragging || (options.slideOnHover && hovered))) {
        const Vec2 at = fractionIn(frame, input.pointer());
        next = vertical ? at.y : at.x;
    }

    // ---- the keys ----------------------------------------------------------
    //
    // Two percent an arrow rather than the five the colour picker uses, and the
    // difference is the target: a colour is aimed at a *region* of a square and
    // a seam is aimed at an edge in a photograph, which is a pixel or two wide.
    // Page steps ten, which is what a reader sweeping the whole comparison
    // wants and what the same keys do on a range input everywhere else.
    if (input.isFocused(id)) {
        for (const KeyEvent& event : input.keys()) {
            switch (event.key) {
                case Key::Left:
                case Key::Down: next -= 0.02f; break;
                case Key::Right:
                case Key::Up: next += 0.02f; break;
                case Key::PageUp: next += 0.10f; break;
                case Key::PageDown: next -= 0.10f; break;
                case Key::Home: next = 0.0f; break;
                case Key::End: next = 1.0f; break;
                default: break;
            }
        }
    }
    next = std::clamp(next, 0.0f, 1.0f);

    Style box;
    box.width = options.width;
    box.height = options.height;
    box.grow = options.grow;
    if (options.grow > 0.0f) box.basis = 0.0f;
    box.overflow = Overflow::Hidden;
    box.radius = ui.design().controlRadius;
    box.cursorHint = vertical ? Cursor::ResizeVertical : Cursor::ResizeHorizontal;
    if (input.isFocusVisible(id)) box.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};

    auto scope = ui.scope(box);
    ui.tag(id).focusable().cursor(box.cursorHint);
    // A slider, and genuinely one — it has a value, a range and the arrow keys.
    // The value in words, because a bare "60 percent" leaves out both the thing
    // being revealed and the thing being covered.
    ui.accessible({
        .role = Role::Slider,
        .name = options.name,
        .value = {.present = true,
                  .now = static_cast<double>(next),
                  .minimum = 0.0,
                  .maximum = 1.0,
                  .text = spoken(next, options.afterLabel)},
    });

    /** One layer, filling the box. Absolute, so the two are on top of each
     *  other rather than beside each other, which is the whole idea. */
    const auto layer = [&](const std::function<void(Ui&)>& content, std::string_view label) {
        Style fill;
        fill.position = Position::Absolute;
        fill.left = 0.0f;
        fill.top = 0.0f;
        fill.width = Length::percent(100);
        fill.height = Length::percent(100);
        auto layerScope = ui.scope(fill);
        // Named, so a reader is told which of the two they are hearing. Both
        // are in the tree whatever the handle is doing: a reader is not
        // comparing them by eye and taking one away would leave them with half
        // the comparison.
        if (!label.empty()) ui.accessible({.role = Role::Group, .name = label});
        if (content) content(ui);
        (void)layerScope;
    };

    layer(before, options.beforeLabel);

    // ---- the revealed side -------------------------------------------------
    //
    // Clipped by a percentage rather than by a measured width, so the seam is
    // right on the first frame and stays right while the window is dragged.
    // The content inside is `100 / position` percent of *that*, which comes
    // back out to the full width of the box — a layout identity rather than an
    // arithmetic one, and the reason none of this needs last frame's geometry.
    //
    // Below a thousandth it is not drawn at all, which is the division guarded
    // and also the honest answer: nothing of it is showing.
    if (next > 0.001f) {
        const float share = next * 100.0f;
        Style clip;
        clip.position = Position::Absolute;
        clip.left = 0.0f;
        clip.top = 0.0f;
        clip.width = vertical ? Length::percent(100) : Length::percent(share);
        clip.height = vertical ? Length::percent(share) : Length::percent(100);
        clip.overflow = Overflow::Hidden;
        auto clipScope = ui.scope(clip);

        Style full;
        full.position = Position::Absolute;
        full.left = 0.0f;
        full.top = 0.0f;
        full.width = vertical ? Length::percent(100) : Length::percent(100.0f / next);
        full.height = vertical ? Length::percent(100.0f / next) : Length::percent(100);
        auto fullScope = ui.scope(full);
        if (!options.afterLabel.empty()) {
            ui.accessible({.role = Role::Group, .name = options.afterLabel});
        }
        if (after) after(ui);
        (void)fullScope;
        (void)clipScope;
    }

    // ---- the handle --------------------------------------------------------
    //
    // Placed by two flexible spacers rather than by an offset in pixels. Same
    // reason as the clip — no measured geometry, so no frame of lag — and it
    // has a second effect worth having: at either end the handle is *inside*
    // the box rather than half out of it, because the space it takes is space
    // the spacers never had.
    {
        Style rail;
        rail.position = Position::Absolute;
        rail.left = 0.0f;
        rail.top = 0.0f;
        rail.width = Length::percent(100);
        rail.height = Length::percent(100);
        rail.direction = vertical ? Direction::Column : Direction::Row;
        rail.align = Align::Center;
        auto railScope = ui.scope(rail);

        Style leading;
        leading.grow = std::max(next, 0.0001f);
        leading.basis = 0.0f;
        ui.add(leading);

        Style bar;
        if (vertical) {
            bar.height = options.handleWidth;
            bar.width = Length::percent(100);
        } else {
            bar.width = options.handleWidth;
            bar.height = Length::percent(100);
        }
        bar.shrink = 0.0f;
        // The grip is bigger than the bar and is *meant* to overflow it. Both
        // minimums have to be turned off for that: `minWidth` and `minHeight`
        // default to the content minimum — the automatic minimum from v0.2 —
        // so the bar was silently as wide as its own knob and drew an
        // eighteen-pixel white band down the middle of the comparison.
        bar.minWidth = 0.0f;
        bar.minHeight = 0.0f;
        bar.radius = 0.0f;
        bar.justify = Justify::Center;
        bar.align = Align::Center;
        bar.background = Fill{Token::AccentFg};
        bar.cursorHint = box.cursorHint;
        {
            auto barScope = ui.scope(bar);
            ui.tag(handleId).ignoresPointer().cursor(box.cursorHint);
            // The grip, which is what says the bar can be dragged. Bigger than
            // the bar on purpose: a three-pixel line is a target nobody hits.
            //
            // Two mechanisms, one per axis, and each is the only one that
            // works without measuring anything. **Across** the bar it is a
            // constant offset — the grip is a fixed size and so is the bar, so
            // half the difference is a number — because `Justify::Center`
            // clamps a child wider than its container to the leading edge
            // rather than hanging it off both sides, and the knob came out
            // beside the seam instead of on it. **Along** the bar it is flex:
            // the sleeve is a full-height column and the knob is smaller than
            // it, which is the case centring does handle.
            const float grip = std::max(18.0f, options.handleWidth * 6.0f);
            const float overhang = -(grip - options.handleWidth) / 2.0f;

            Style sleeve;
            sleeve.position = Position::Absolute;
            sleeve.left = vertical ? 0.0f : overhang;
            sleeve.top = vertical ? overhang : 0.0f;
            sleeve.width = vertical ? Length::percent(100) : Length{grip};
            sleeve.height = vertical ? Length{grip} : Length::percent(100);
            sleeve.direction = vertical ? Direction::Row : Direction::Column;
            sleeve.justify = Justify::Center;
            sleeve.align = Align::Center;
            auto sleeveScope = ui.scope(sleeve);

            Style knob;
            knob.width = grip;
            knob.height = grip;
            knob.shrink = 0.0f;
            knob.radius = grip / 2.0f;
            knob.background = Fill{Token::AccentFg};
            knob.border = Border{2.0f, Fill{Token::Accent}};
            ui.add(knob);
            (void)sleeveScope;
            (void)barScope;
        }

        Style trailing;
        trailing.grow = std::max(1.0f - next, 0.0001f);
        trailing.basis = 0.0f;
        ui.add(trailing);
        (void)railScope;
    }
    (void)scope;

    return {next, std::abs(next - position) > 1e-4f};
}

}  // namespace gbui
