// A row of a list: the sidebar's branches, the commit list, the changed files.
#pragma once

#include <string_view>

#include "gbui/scene/ui.hpp"

namespace gbui {

struct ListRowOptions {
    /** Passed in, never held. The row draws the state and the application owns
     *  it, which is the contract every component here has. */
    bool selected = false;
    /** As above. Separate from what the pointer is over, because a list often
     *  wants the keyboard's row lit instead. */
    bool hovered = false;
    float height = 28.0f;
    Edges padding = Edges::symmetric(0.0f, 12.0f);
    float gap = 6.0f;
    std::string_view id{};
};

/** Opens a row. Returns a Scope, so its contents are written inside the
 *  braces. `selected` washes the row in the accent at 18%, `hovered` uses
 *  `surfaceHover`, and both are passed in — components hold no state. */
Ui::Scope listRow(Ui& ui, const ListRowOptions& options = {});

}  // namespace gbui
