#include "gbui/widgets/splitPane.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "detail.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

SplitPaneResult splitPane(Ui& ui, const Interaction& input, std::string_view id, float position,
                          const std::function<void(Ui&)>& leading,
                          const std::function<void(Ui&)>& trailing,
                          const SplitPaneOptions& options) {
    const bool vertical = options.orientation == SplitOrientation::Vertical;
    const std::string dividerId = std::string(id) + ".divider";

    float next = std::clamp(position, 0.0f, 1.0f);

    // ---- the drag ----------------------------------------------------------
    //
    // The one thing here that needs a measured size, because turning a pointer
    // into a fraction cannot be done without one. Last frame's, which is the
    // frame the reader was pointing at — and the drag keeps working after the
    // pointer leaves the container, which is what makes either extreme
    // reachable without a flick.
    const Rect frame = input.frameOf(id);
    const float span = vertical ? frame.height : frame.width;
    if (input.dragging() == dividerId && span > 0.0f) {
        const float at = vertical ? input.pointer().y - frame.y : input.pointer().x - frame.x;
        next = std::clamp(at / span, 0.0f, 1.0f);
    }

    // ---- the keys ----------------------------------------------------------
    //
    // ARIA's window splitter, and the half everybody forgets: a split only
    // draggable with a pointer is a layout most people cannot change. Home and
    // End go to the extremes the *minimums* leave, rather than to 0 and 1,
    // because a pane squeezed to nothing is a pane with no way back.
    if (input.isFocused(dividerId)) {
        for (const KeyEvent& event : input.keys()) {
            const float step = event.modifiers.shift ? 0.10f : 0.02f;
            switch (event.key) {
                case Key::Left:
                case Key::Up: next -= step; break;
                case Key::Right:
                case Key::Down: next += step; break;
                case Key::Home: next = 0.0f; break;
                case Key::End: next = 1.0f; break;
                default: break;
            }
        }
    }
    next = std::clamp(next, 0.0f, 1.0f);

    Style outer;
    outer.direction = vertical ? Direction::Column : Direction::Row;
    outer.width = options.width;
    outer.height = options.height;
    outer.grow = options.grow;
    if (options.grow > 0.0f) outer.basis = 0.0f;
    // Without this the container is as wide as its content wants, and two panes
    // that each asked for their minimum would push it past whatever holds it.
    outer.minWidth = 0.0f;
    outer.minHeight = 0.0f;

    auto outerScope = ui.scope(outer);
    ui.tag(id);

    /** One pane, sized by its share and floored by its minimum. */
    const auto pane = [&](const std::function<void(Ui&)>& content, float share, float floorSize,
                          std::string_view label) {
        Style box;
        // ---- a percentage basis, and why not `grow` --------------------
        //
        // A share resolved during layout, so it is right on the first frame and
        // stays right while the window is dragged — the same reason `compare`
        // clips by a percentage rather than by a measured width.
        //
        // **`grow` was the obvious way and gives the wrong number.** This
        // engine computes its free space from the *hypothetical* sizes, which
        // are already clamped to each item's minimum, so two panes with a
        // 120-pixel floor each take their 240 first and then split what is
        // left by the ratio: asking for a quarter of 600 got 208 rather than
        // 148, and with large minimums the fraction stopped meaning anything
        // at all.
        //
        // A basis of `p%` plus `shrink` gets it exactly. The two bases and the
        // divider total 100% plus the divider's width, and shrink takes that
        // overflow back **in proportion to the bases** — so the leading pane
        // ends at `p × (width − divider)`, which is the definition.
        box.basis = Length::percent(std::max(share, 0.0f) * 100.0f);
        box.grow = 0.0f;
        box.shrink = 1.0f;
        box.overflow = Overflow::Hidden;
        // The minimum lives here, and that is the point: flexbox refuses to
        // shrink a child below it, so it holds when the *window* shrinks as
        // well as when the divider moves — the case a clamp in the drag handler
        // silently misses.
        if (vertical) {
            box.minHeight = floorSize;
            box.minWidth = 0.0f;
        } else {
            box.minWidth = floorSize;
            box.minHeight = 0.0f;
        }
        auto paneScope = ui.scope(box);
        if (!label.empty()) ui.accessible({.role = Role::Group, .name = label});
        if (content) content(ui);
        (void)paneScope;
    };

    pane(leading, next, options.minLeading, options.leadingLabel);

    // ---- the divider -------------------------------------------------------
    {
        Style grip;
        grip.shrink = 0.0f;
        grip.justify = Justify::Center;
        grip.align = Align::Center;
        grip.radius = 0.0f;
        if (vertical) {
            grip.height = options.dividerWidth;
            grip.width = Length::percent(100);
            grip.cursorHint = Cursor::ResizeVertical;
        } else {
            grip.width = options.dividerWidth;
            grip.height = Length::percent(100);
            grip.cursorHint = Cursor::ResizeHorizontal;
        }
        const bool active = input.dragging() == dividerId || input.isHovered(dividerId);
        if (active) grip.background = Fill{Token::Accent, 0.18f};
        if (input.isFocusVisible(dividerId)) {
            grip.outline = Outline{2.0f, 0.0f, Fill{Token::Accent}};
        }
        auto gripScope = ui.scope(grip);
        // A `Separator` with a value, which is ARIA's window splitter: a
        // separator that is only a rule takes no keyboard and has nothing to
        // report, and this one is a control.
        ui.tag(dividerId).focusable().cursor(grip.cursorHint).accessible({
            .role = Role::Separator,
            .name = options.name,
            .value = {.present = true,
                      .now = static_cast<double>(next),
                      .minimum = 0.0,
                      .maximum = 1.0},
        });

        // The hairline inside the strip. The strip is the target and the line
        // is the drawing, which is why they are two different widths — a
        // one-pixel target is a target nobody hits.
        Style rule;
        if (vertical) {
            rule.height = 1.0f;
            rule.width = Length::percent(100);
        } else {
            rule.width = 1.0f;
            rule.height = Length::percent(100);
        }
        rule.radius = 0.0f;
        rule.background = Fill{active ? Token::Accent : Token::Border};
        ui.add(rule);
        (void)gripScope;
    }

    pane(trailing, 1.0f - next, options.minTrailing, options.trailingLabel);
    (void)outerScope;

    return {next, std::abs(next - position) > 1e-4f};
}

}  // namespace gbui
