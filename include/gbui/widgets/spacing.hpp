// The two nodes that exist to take up room, or to refuse to.
#pragma once

#include "gbui/scene/ui.hpp"

namespace gbui {

/** Fills the free space on the main axis — the flexible gap between a title and
 *  the actions on the far side of a toolbar. */
NodeId spacer(Ui& ui, float grow = 1.0f);

/** A hairline. Horizontal in a column, vertical in a row: pass the direction of
 *  the container it sits in. */
NodeId divider(Ui& ui, Direction containerDirection);

}  // namespace gbui
