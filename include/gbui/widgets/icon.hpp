// One icon from the built-in Lucide set.
#pragma once

#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct IconOptions {
    Token color = Token::Text;
    /** Side of the square the icon is drawn in. */
    float size = 16.0f;
    /** Stroke width on Lucide's 24-unit grid; scaled with the icon. */
    float stroke = 2.0f;
};

NodeId icon(Ui& ui, Icon which, const IconOptions& options = {});

}  // namespace gbui
