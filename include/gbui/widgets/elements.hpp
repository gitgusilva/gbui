// The elements, as one include: the leaves every interface is built from.
//
// An element is a **leaf that has a counterpart in HTML** — a run of text, a
// button, a checkbox, an image, a badge. It draws itself and nothing else, and
// it decides nothing beyond the theme it takes its colours from. That last part
// is the line against `components.hpp`, where a date picker has already decided
// what a month looks like.
//
// ---- how the four groups divide, in the order the questions are asked -------
//
//   1. Is its job the content inside it?  ->  containers.hpp
//   2. Does it leave the flow and float?  ->  overlays.hpp
//   3. Is it a leaf with an HTML counterpart?  ->  here
//   4. Otherwise it is composed, and has opinions.  ->  components.hpp
//
// The order matters and is why `panel` is a container rather than a component
// even though it is plainly composed, and why `select` is an element even
// though its list is a popover: a control is not defined by where it happens to
// draw a menu. Asking the questions in any other order produces a taxonomy with
// two axes and no answer for the things that sit on both.
//
// ---- the contract the interactive ones share -------------------------------
//
// They hold no state. The value belongs to the application and is passed in;
// everything transient — hovered, pressed, focused — comes from the
// `Interaction` the frame was resolved against. The shape is always the same:
//
//     bool toggled = checkbox(ui, interaction, "settings.tags", value, {.label = "Show tags"});
//     if (toggled) value = !value;
//
// The element never writes to your model. It reports what happened and leaves
// the decision with you, which is what makes undo, validation and "are you
// sure?" possible without the toolkit knowing about any of them.
//
// Each element also has a header of its own, so a caller can include only what
// it uses.
#pragma once

// ---- content ---------------------------------------------------------------
#include "gbui/widgets/badge.hpp"
#include "gbui/widgets/hyperlink.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/image.hpp"
#include "gbui/widgets/label.hpp"
#include "gbui/widgets/progress.hpp"
#include "gbui/widgets/spacing.hpp"
#include "gbui/widgets/text.hpp"

// ---- input -----------------------------------------------------------------
#include "gbui/widgets/button.hpp"
#include "gbui/widgets/checkbox.hpp"
#include "gbui/widgets/field.hpp"
#include "gbui/widgets/numberField.hpp"
#include "gbui/widgets/radio.hpp"
#include "gbui/widgets/select.hpp"
#include "gbui/widgets/slider.hpp"
#include "gbui/widgets/textarea.hpp"
#include "gbui/widgets/textField.hpp"
#include "gbui/widgets/toggle.hpp"
