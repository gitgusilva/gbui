// The component set, as one include.
//
// Every component here is a plain function that writes nodes through a Ui —
// there is no widget base class and nothing to inherit. That is what makes a
// component in this library: a function with an options struct, composed from
// the primitives in ui.hpp. Writing your own is the same exercise, and
// docs/guide/writing-a-component.md walks through one.
//
// Components take their colours from Token, never from a literal, so a theme
// swap moves the whole set.
//
// One component, one header, the way one component is one file — this is the
// umbrella for the group, for a caller who wants all of it.
#pragma once

#include "gbui/widgets/badge.hpp"
#include "gbui/widgets/button.hpp"
#include "gbui/widgets/chart.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/image.hpp"
#include "gbui/widgets/listRow.hpp"
#include "gbui/widgets/panel.hpp"
#include "gbui/widgets/spacing.hpp"
#include "gbui/widgets/text.hpp"
#include "gbui/widgets/toolbar.hpp"
