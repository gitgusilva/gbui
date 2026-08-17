// A row of two to five choices, all of them on screen at once.
//
// The control a `select` becomes when the list is short enough to show. Both
// answer "pick one of these"; the difference is whether the options are worth
// the room. Two or three — unified and split, day and week and month, dark and
// light — read faster as a row than as a box that has to be opened, and a reader
// can see what they did *not* pick, which is half of what makes a choice easy.
//
// **Past about five, use `select`.** A segmented control with nine segments is a
// row of tiny targets that wraps on a narrow window, and the thing it was good
// at — showing the alternatives — stops working once they do not fit.
//
// ---- why it is radios and not tabs ------------------------------------------
//
// A segmented control looks exactly like a tab strip and is not one. Tabs *show
// a panel*: pressing one changes what is below it, and the strip and the panel
// are one widget. This changes a **value** — how a diff is laid out, which range
// a chart covers — and the thing it changes may be nowhere near it. Announced as
// tabs, a reader is told to expect a panel that never comes.
#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct Segment {
    std::string_view label{};
    /** Drawn before the label, or on its own when the label is empty — which is
     *  how an icon-only strip is spelt. */
    std::optional<Icon> icon{};
    /** What it is called when the label is an icon or an abbreviation. "1m" is
     *  a word to a reader who can see the chart beside it and nothing to one
     *  who cannot. */
    std::string_view name{};
    bool disabled = false;
};

struct SegmentedOptions {
    /** What the whole row is for — "Diff layout", "Range". A group of choices
     *  with no name is a set of buttons a reader has to guess the subject of. */
    std::string_view name{};
    bool disabled = false;
    /** Each segment takes an equal share of the width. Off sizes each to its
     *  own label, which is what a strip inside a toolbar wants. */
    bool stretch = false;
    /** Zero takes the active design's control height, less two pixels for the
     *  track it sits in — so a strip lines up with the controls beside it. */
    float height = 0.0f;
    float size = 12.0f;
};

/**
 * A row of segments, one of them chosen.
 *
 * Returns the index pressed this frame, or nothing. The value is the caller's,
 * like every other control here.
 *
 * The keyboard is ARIA's radio group, which is one Tab stop and not one per
 * segment: Tab reaches the row, the arrows move *and choose* — because a radio
 * group with a highlight separate from its value is a control where the reader
 * has to press twice — and Home and End go to the ends.
 */
std::optional<std::size_t> segmented(Ui& ui, const Interaction& input, std::string_view id,
                                     const std::vector<Segment>& segments, std::size_t selected,
                                     const SegmentedOptions& options = {});

}  // namespace gbui
