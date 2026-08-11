// A switch.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct SwitchOptions {
    bool disabled = false;
    std::string_view label{};
    /** Zero takes the active design's track and thumb. */
    float width = 0.0f;
    float height = 0.0f;
};

/** Same contract as the checkbox; the difference is only that a switch reads as
 *  "on now" and a checkbox as "will apply". */
[[nodiscard]] bool switchToggle(Ui& ui, const Interaction& input, std::string_view id, bool on,
                  const SwitchOptions& options = {});

}  // namespace gbui
