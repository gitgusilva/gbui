// One option of a group.
#pragma once

#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

struct RadioOptions {
    bool disabled = false;
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
    /** Zero takes the active design's size. */
    float size = 0.0f;
};

/** Returns true on the frame it was chosen; a radio that is already selected
 *  reports nothing, because choosing it again is not a change. */
bool radio(Ui& ui, const Interaction& input, std::string_view id, bool selected,
           const RadioOptions& options = {});

}  // namespace gbui
