// One option of a group.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct RadioOptions {
    bool disabled = false;
    std::string_view label{};
    /** Zero takes the active design's size. */
    float size = 0.0f;
};

/** Returns true on the frame it was chosen; a radio that is already selected
 *  reports nothing, because choosing it again is not a change. */
bool radio(Ui& ui, const Interaction& input, std::string_view id, bool selected,
           const RadioOptions& options = {});

}  // namespace gbui
