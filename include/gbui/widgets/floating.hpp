// What everything that floats has in common.
//
// A floating box sits in a layer above the content, is positioned by the
// placement engine rather than by the flex flow, and is anchored by tag: it
// asks the interaction layer where its anchor was last frame. That is the
// rectangle the user was pointing at, and the only geometry available while the
// tree is still being built.
#pragma once

#include "gbui/core/geometry.hpp"
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
};

}  // namespace gbui
