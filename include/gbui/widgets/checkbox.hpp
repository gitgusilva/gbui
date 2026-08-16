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
    /**
     * What a reader is told this is, when the label is not it.
     *
     * The label by default, which is right whenever there is one. Needed where
     * the words are drawn *beside* the control rather than by it — a row that
     * lays out its own caption and passes an empty `label` — because then there
     * is nothing for the control to borrow and "switch, on" is all a reader
     * gets.
     */
    std::string_view name{};
    /** Zero takes the active design's box size and corner. */
    float size = 0.0f;
};

/** Returns true on the frame the user toggled it. */
[[nodiscard]] bool checkbox(Ui& ui, const Interaction& input, std::string_view id, bool checked,
              const CheckboxOptions& options = {});

}  // namespace gbui
