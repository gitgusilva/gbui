// A checkbox.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct CheckboxOptions {
    bool disabled = false;
    /** Drawn to the right of the box; empty draws the box alone. */
    std::string_view label{};
    /** Zero takes the active design's box size and corner. */
    float size = 0.0f;
};

/** Returns true on the frame the user toggled it. */
[[nodiscard]] bool checkbox(Ui& ui, const Interaction& input, std::string_view id, bool checked,
              const CheckboxOptions& options = {});

}  // namespace gbui
