// Floating components: tooltip, popover, menu, select and modal — as one
// include. Each also has a header of its own.
//
// All of them sit in a layer above the content, are positioned by the placement
// engine rather than by the flex flow, and share one rule: **the application
// owns whether they are open.**
#pragma once

#include "gbui/widgets/floating.hpp"
#include "gbui/widgets/menu.hpp"
#include "gbui/widgets/modal.hpp"
#include "gbui/widgets/popover.hpp"
#include "gbui/widgets/select.hpp"
#include "gbui/widgets/tooltip.hpp"
