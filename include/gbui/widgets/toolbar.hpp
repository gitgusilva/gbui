// A horizontal bar of actions.
#pragma once

#include "gbui/scene/ui.hpp"

namespace gbui {

struct ToolbarOptions {
    float height = 40.0f;
    float gap = 8.0f;
    Edges padding = Edges::symmetric(0.0f, 12.0f);
    Token background = Token::BgElevated;
    /** **Reserved, and currently draws nothing.** `Border` is all four edges,
     *  so a toolbar's rule has to be a sibling: write
     *  `divider(ui, Direction::Column)` after the toolbar until the primitive
     *  grows per-edge widths. Named rather than removed, because the option is
     *  the right shape and only the primitive is missing. */
    bool bottomBorder = true;
};

Ui::Scope toolbar(Ui& ui, const ToolbarOptions& options = {});

}  // namespace gbui
