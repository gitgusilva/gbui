// The overlays, as one include: tooltip, popover, menu and modal.
//
// The second question the taxonomy asks — see the order at the top of
// `elements.hpp`. All of them leave the flex flow, sit in a layer above the
// content, are positioned by the placement engine, and share one rule: **the
// application owns whether they are open.**
//
// `select` used to be listed here and is an element now. It opens a popover,
// but so does a native `<select>`, and where a control happens to draw its list
// is not what decides whether the control is a primitive — the question is
// asked of the control, not of its menu. It comes in through `elements.hpp`.
//
// Each also has a header of its own.
#pragma once

#include "gbui/widgets/floating.hpp"
#include "gbui/widgets/menu.hpp"
#include "gbui/widgets/modal.hpp"
#include "gbui/widgets/popover.hpp"
#include "gbui/widgets/toast.hpp"
#include "gbui/widgets/tooltip.hpp"
