#include "gbui/widgets/icon.hpp"



namespace gbui {

NodeId icon(Ui& ui, Icon which, const IconOptions& options) {
    Style style;
    style.width = options.size;
    style.height = options.size;
    style.shrink = 0.0f;  // an icon is its size; shrinking it is never right
    return ui.vector(iconPath(which), style, Fill{options.color}, options.stroke);
}

}  // namespace gbui
