// A surface with a border and a radius — a card, a dialog body, a docked pane.
#pragma once

#include "gbui/scene/ui.hpp"

namespace gbui {

struct PanelOptions {
    Token background = Token::BgElevated;
    bool border = true;
    Edges padding = Edges::all(12.0f);
    float gap = 8.0f;
    Direction direction = Direction::Column;
    float radius = kAuto;
    /** Takes the space left on the main axis, the way a pane holding a list
     *  has to. It starts from nothing, as `text` and the scroll view do, so it
     *  cannot push its siblings out of the column. */
    float grow = 0.0f;
};

Ui::Scope panel(Ui& ui, const PanelOptions& options = {});

}  // namespace gbui
