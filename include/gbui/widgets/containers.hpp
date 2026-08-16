// The containers, as one include: everything whose job is the content inside it.
//
// This is the **first** question the taxonomy asks, ahead of whether a thing is
// a leaf or composed — see the order set out at the top of `elements.hpp`. It
// is why `panel`, `listRow` and `toolbar` are here rather than among the
// composed components they plainly are: what they are *for* is arranging their
// children, and a reader looking for "the thing that holds a titled group of
// rows" looks under containers.
//
// Two shapes qualify, and both count:
//
//   * it returns a `Ui::Scope`, so the caller writes the children inside the
//     braces — `box`, `panel`, `listRow`, `toolbar`, `scrollArea`;
//   * it takes the content as data or as a callback and decides which of it is
//     on screen and where — `table`, `tabs`, `virtualList`, `marquee`.
//
// A scrollbar is neither, and is here anyway, because it is `scrollArea`'s
// other half and one component's parts do not belong in two groups.
//
// Each also has a header of its own.
#pragma once

// ---- surfaces --------------------------------------------------------------
#include "gbui/widgets/box.hpp"
#include "gbui/widgets/listRow.hpp"
#include "gbui/widgets/panel.hpp"
#include "gbui/widgets/toolbar.hpp"

// ---- views over content ----------------------------------------------------
#include "gbui/widgets/carousel.hpp"
#include "gbui/widgets/compare.hpp"
#include "gbui/widgets/gallery.hpp"
#include "gbui/widgets/marquee.hpp"
#include "gbui/widgets/scroll.hpp"
#include "gbui/widgets/table.hpp"
#include "gbui/widgets/tabs.hpp"
#include "gbui/widgets/virtualList.hpp"
