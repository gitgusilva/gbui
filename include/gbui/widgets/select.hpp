// A closed box that opens a list.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/floating.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

struct SelectOptions : FloatingOptions {
    std::string_view placeholder = "Select…";
    bool disabled = false;
    float width = kAuto;
    float grow = 0.0f;
    /** Zero takes the active design's control height, so a select lines up
     *  with every other control on its row without anyone matching numbers. */
    float height = 0.0f;
    /** How many rows fit before the list scrolls. The rest are reachable by
     *  scrolling or by walking to them with the arrow keys. */
    std::size_t maxVisible = 12;
    /** A hard ceiling in pixels, which wins over `maxVisible` when both would
     *  apply — a row count cannot know how tall the window is. `kAuto` leaves
     *  the row count in charge. */
    float maxListHeight = kAuto;
    /** Whether the open list scrolls at all. `None` clips it instead, for a
     *  caller that would rather constrain the list than let it move. */
    ScrollAxis listScroll = ScrollAxis::Vertical;
};

/**
 * What a select remembers between frames, owned by the application like every
 * other piece of state the toolkit reads.
 *
 * `highlighted` is not the value. Walking a list is not choosing from it: the
 * highlight moves with Up and Down, Return commits it, and Escape throws it
 * away and leaves the value alone. That separation is the whole reason an open
 * list is usable from the keyboard.
 */
struct SelectState {
    bool open = false;
    std::optional<std::size_t> highlighted;
    /** Where the open list is scrolled to. Written by the component. */
    ScrollState list;
};

struct SelectResult {
    /** Index chosen this frame, or nothing. */
    std::optional<std::size_t> chosen;
};

/**
 * A closed box that opens a list.
 *
 * Closed and focused, Up and Down step the value and Return or Space opens the
 * list. Open, they walk the highlight, Home and End jump to the ends, Return or
 * Space commits, and Escape closes without changing anything. The list keeps
 * the highlighted row in view as it moves, and the box — not the rows — keeps
 * the keyboard, so Tab leaves the control rather than walking into the popup.
 */
[[nodiscard]] SelectResult select(Ui& ui, const Interaction& input, std::string_view id,
                    const std::vector<std::string>& items, std::optional<std::size_t> selected,
                    SelectState& state, const SelectOptions& options = {});

}  // namespace gbui
