// What everything that floats has in common.
//
// A floating box sits in a layer above the content, is positioned by the
// placement engine rather than by the flex flow, and is anchored by tag: it
// asks the interaction layer where its anchor was last frame. That is the
// rectangle the user was pointing at, and the only geometry available while the
// tree is still being built.
#pragma once

#include <initializer_list>
#include <string_view>

#include "gbui/core/geometry.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/overlay/placement.hpp"

namespace gbui {

struct FloatingOptions {
    Placement placement = Placement::Auto;
    float gap = 6.0f;
    float margin = 8.0f;
    bool flip = true;
    bool shift = true;
    /** The window, so the box can be kept inside it. Empty means "the viewport
     *  the last layout used", which is what an application wants. */
    Rect bounds{};

    // ---- what closes it ----------------------------------------------------
    /**
     * A press anywhere else puts it away.
     *
     * On, because it is what every open list on every platform does and a
     * reader who has to find the control again to close it will say the control
     * is broken. Off is for the box that has to be dismissed on purpose — a
     * form in a popover, where a stray press would throw away typing.
     *
     * "Anywhere else" is measured against the box *and* its anchor: pressing
     * the control that opened it is that control's own business, and closing on
     * the way down would make the second press re-open it.
     */
    bool dismissOnOutsideClick = true;
    /** Escape puts it away. Off for a box whose Escape means something else —
     *  a filtering `select` spends the first press clearing what was typed. */
    bool dismissOnEscape = true;
};

/**
 * Whether a press *this frame* landed outside all of `tags`.
 *
 * The test a floating box needs and the one that is easy to get subtly wrong.
 * Three things it does that the obvious version does not:
 *
 *  - it is the edge, not the level. `pointerDown()` stays true while the button
 *    is held, so a press outside would close the box on the frame it happened
 *    and then again on every frame until the button came up — which matters the
 *    moment anything re-opens on a press;
 *  - it asks the geometry rather than the hit test. "The press hit no tagged
 *    node" reads as outside when the reader pressed a decorative part of the
 *    box itself;
 *  - it takes several tags, because a popover and the control that opened it
 *    are one thing to the reader and closing on the anchor would fight whatever
 *    that control does with the same press.
 *
 * A component that owns its own open state should call this. A caller driving a
 * bare `popover` calls it too — the popover has nowhere to keep the answer.
 */
bool pressedOutside(const Interaction& input, std::initializer_list<std::string_view> tags);

}  // namespace gbui
