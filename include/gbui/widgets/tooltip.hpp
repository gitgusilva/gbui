// A tooltip, shown while its anchor is hovered.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/floating.hpp"

namespace gbui {

struct TooltipOptions : FloatingOptions {
    float maxWidth = 260.0f;
    /**
     * How long the pointer must rest on the anchor first, in seconds.
     *
     * Without one, dragging the pointer across a toolbar flashes a tooltip for
     * every control it passes over, which is noise rather than help. Needs an
     * animator for the clock; with none, the tooltip shows at once and behaves
     * as it always did.
     */
    float delay = 0.4f;
    /** A long tooltip wraps rather than eliding — a description cut off at an
     *  ellipsis is worse than no description. */
    bool wrap = true;
};

/** Draws nothing when the anchor is not hovered, so the call can sit
 *  unconditionally beside the control it describes. */
void tooltip(Ui& ui, const Interaction& input, std::string_view anchorId, std::string_view text,
             const TooltipOptions& options = {});

}  // namespace gbui
