// The interactive components.
//
// Each one is a function that draws the control and reports what the user did
// with it. They hold no state: the value belongs to the application and is
// passed in, and everything transient — what is hovered, pressed or focused —
// comes from the `Interaction` the frame was resolved against.
//
// The shape is always the same, and it is worth reading once:
//
//     bool toggled = checkbox(ui, interaction, "settings.tags", value, {.label = "Show tags"});
//     if (toggled) value = !value;
//
// The component never writes to your model. It tells you what happened and
// leaves the decision with you, which is what makes undo, validation and
// "are you sure?" possible without the toolkit knowing about any of them.
//
// This is the umbrella over the group; each control also has a header of its
// own, so a caller can include only what it uses.
#pragma once

#include "gbui/widgets/checkbox.hpp"
#include "gbui/widgets/colorPicker.hpp"
#include "gbui/widgets/datePicker.hpp"
#include "gbui/widgets/dateTimePicker.hpp"
#include "gbui/widgets/timePicker.hpp"
#include "gbui/widgets/label.hpp"
#include "gbui/widgets/hyperlink.hpp"
#include "gbui/widgets/numberField.hpp"
#include "gbui/widgets/progress.hpp"
#include "gbui/widgets/radio.hpp"
#include "gbui/widgets/richEditor.hpp"
#include "gbui/widgets/slider.hpp"
#include "gbui/widgets/toggle.hpp"
#include "gbui/widgets/textField.hpp"
