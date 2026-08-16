// The components, as one include: the composed editors.
//
// What is left once the three structural questions in `elements.hpp` have been
// asked — a thing that is not a container, does not float, and is not a leaf.
// In practice that is the editors: a colour picker, the three date and time
// pickers, and the rich text editor. Each is a small application of its own,
// built out of elements, and each has already spent design decisions the caller
// would otherwise have to make — what a month looks like, where the hue rail
// goes, what a formatting toolbar holds.
//
// It is a short list, and that is the honest answer rather than a sign the
// group is wrong. Most of what a toolkit ships is either a leaf or a container;
// the genuinely composed widgets are few, and they are the expensive ones.
//
// Every one of them comes in two forms — the inline picker and the field that
// opens it in a popover — which is the pattern to follow when adding another.
//
// They are the same kind of thing to the compiler as an element: a plain
// function that writes nodes through a `Ui`, with an options struct. There is
// no widget base class and nothing to inherit, and writing your own is the same
// exercise — docs/guide/writing-a-component.md walks through one. Colours come
// from `Token`, never from a literal, so a theme swap moves the whole set.
#pragma once

// ---- editors ---------------------------------------------------------------
#include "gbui/widgets/colorPicker.hpp"
#include "gbui/widgets/datePicker.hpp"
#include "gbui/widgets/dateTimePicker.hpp"
#include "gbui/widgets/richEditor.hpp"
#include "gbui/widgets/timePicker.hpp"

// ---- charts ----------------------------------------------------------------
//
// A group of their own in the documentation and the metadata — nine unrelated
// charts under one header, which is a category by any reader's reckoning —
// but included here because a caller who wants "the components" wants these
// too.
#include "gbui/widgets/chart.hpp"
