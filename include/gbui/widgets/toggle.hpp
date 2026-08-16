// A switch.
//
// Called `toggle` because `switch` is a C++ keyword and cannot be a function
// name. That is the whole of the reason, and it is worth writing down so nobody
// spends an afternoon on it: the thing is a switch, the documentation calls it
// one, and Fluent and Carbon both settled on the same word for the same reason.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct ToggleOptions {
    bool disabled = false;
    std::string_view label{};
    /** Zero takes the active design's track and thumb. */
    float width = 0.0f;
    float height = 0.0f;
};

/** Same contract as the checkbox; the difference is only that a switch reads as
 *  "on now" and a checkbox as "will apply". */
[[nodiscard]] bool toggle(Ui& ui, const Interaction& input, std::string_view id, bool on,
                          const ToggleOptions& options = {});

}  // namespace gbui
