// A horizontal bar of actions.
#pragma once

#include "gbui/scene/ui.hpp"

namespace gbui {

struct ToolbarOptions {
    float height = 40.0f;
    float gap = 8.0f;
    Edges padding = Edges::symmetric(0.0f, 12.0f);
    Token background = Token::BgElevated;
    bool bottomBorder = true;
};

Ui::Scope toolbar(Ui& ui, const ToolbarOptions& options = {});

}  // namespace gbui
