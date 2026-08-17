// A closed box that opens a list, and — with `filter` on — a combobox.
//
// ---- why the combobox is an option and not a component ----------------------
//
// The component inventory calls type-to-filter "the gap that bites first": a
// branch picker past about thirty branches is unusable without it. It is a flag
// here rather than a `combobox` beside this, for the reason `textInput`
// absorbed `textField` and `numberField` — the two would be one control
// described twice. Everything that makes a select a select is unchanged by
// typing into it: the value is still an index the caller owns, the highlight is
// still separate from the value, the list is still a popover on the overlay
// layer. What filtering adds is a box at the top and a smaller set of rows.
//
// The one thing it *does* change is where the keyboard is, and that is why
// `SelectResult` grew a `focus`. A filter box has to hold the keyboard to be
// typed into, so the control can no longer keep it on the closed box — and a
// component here never moves focus behind the caller's back. Same contract
// `label` and `field` already have.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/input/textEdit.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/floating.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

struct SelectOptions : FloatingOptions {
    /**
     * What this is called, for a reader who cannot see the caption beside it.
     *
     * Unnecessary when a `label` or a `field` names it — those attach the
     * relation, and a name given twice is a name read out twice. Necessary the
     * rest of the time, and the placeholder is not a substitute: a box named by
     * its placeholder loses its name the moment somebody types in it.
     */
    std::string_view name{};
    std::string_view placeholder = "Select…";
    bool disabled = false;
    float width = kAuto;
    float grow = 0.0f;
    /** Zero takes the active design's control height, so a select lines up
     *  with every other control on its row without anyone matching numbers. */
    float height = 0.0f;
    /** How many rows fit before the list scrolls. The rest are reachable by
     *  scrolling or by walking to them with the arrow keys. */
    std::size_t maxVisible = 12;
    /** A hard ceiling in pixels, which wins over `maxVisible` when both would
     *  apply — a row count cannot know how tall the window is. `kAuto` leaves
     *  the row count in charge. */
    float maxListHeight = kAuto;
    /** Whether the open list scrolls at all. `None` clips it instead, for a
     *  caller that would rather constrain the list than let it move. */
    ScrollAxis listScroll = ScrollAxis::Vertical;

    // ---- the combobox ------------------------------------------------------
    /**
     * A box at the top of the open list that narrows it as the reader types.
     *
     * **Hand `SelectResult::focus` to `Interaction::focus` when this is on.**
     * Without it the filter box still works — a click focuses it — but it will
     * not have the keyboard the moment the list opens, which is the whole
     * gesture.
     */
    bool filter = false;
    /** The filter box's own hint. */
    std::string_view filterPlaceholder = "Type to filter…";
    /** Drawn where the rows would be when nothing matches. An empty list with
     *  no explanation reads as a list that failed to load. */
    std::string_view emptyMessage = "No matches";

    // ---- more than one at a time --------------------------------------------
    /**
     * The reader may take several rows rather than one.
     *
     * An option and not a `multiSelect` component beside this, for the third
     * time the same argument has been settled here: everything that makes a
     * select a select is unchanged by how many rows it keeps. The value is
     * still the caller's, the highlight is still separate from it, the list is
     * still a popover. What changes is that a row **toggles** instead of
     * replacing, that the list stays open so a second row can be taken, and
     * that the closed box has more than one thing to say.
     *
     * `multiple` is the *interaction*; the caller's container is the value. Use
     * the overload that takes a vector of indices to pass more than one — the
     * single-value form still works and simply holds at most one.
     */
    bool multiple = false;
    /**
     * How the closed box reads once several rows are taken.
     *
     * Below the threshold the labels are listed; at or above it the box says
     * "N selected", because a box listing nine branch names is a box whose own
     * label has gone. Zero always lists them.
     */
    std::size_t summariseFrom = 3;
};

/**
 * What a select remembers between frames, owned by the application like every
 * other piece of state the toolkit reads.
 *
 * `highlighted` is not the value. Walking a list is not choosing from it: the
 * highlight moves with Up and Down, Return commits it, and Escape throws it
 * away and leaves the value alone. That separation is the whole reason an open
 * list is usable from the keyboard.
 */
struct SelectState {
    bool open = false;
    /**
     * Which row the keys are on — an index into the **whole** list, never into
     * the filtered view of it.
     *
     * That is the invariant filtering is easiest to get wrong: a highlight
     * stored as "the third visible row" means a different option every time a
     * character is typed, and the reader commits something they never saw.
     */
    std::optional<std::size_t> highlighted{};
    /** Where the open list is scrolled to. Written by the component. */
    ScrollState list;
    /** What has been typed into the filter box. Cleared when the list opens,
     *  because a filter left over from last time is a list with rows missing
     *  and nothing on screen saying why. */
    TextEditState query{};
};

struct SelectResult {
    /** Index chosen this frame, into the caller's own list — never into the
     *  filtered view of it. */
    std::optional<std::size_t> chosen{};
    /**
     * Focus this, if anything.
     *
     * The filter box when the list has just opened, and the closed box when it
     * has just shut — otherwise the keyboard is left on a node that no longer
     * exists. Handed back rather than acted on, the same contract `label` and
     * `field` have: the toolkit does not move focus behind a component's back.
     *
     *     if (const auto target = result.focus) interaction.focus(*target);
     */
    std::optional<std::string_view> focus{};
};

/**
 * A closed box that opens a list.
 *
 * Closed and focused, Up and Down step the value and Return or Space opens the
 * list. Open, they walk the highlight, Home and End jump to the ends, Return or
 * Space commits, and Escape closes without changing anything. The list keeps
 * the highlighted row in view as it moves, and the box — not the rows — keeps
 * the keyboard, so Tab leaves the control rather than walking into the popup.
 */
[[nodiscard]] SelectResult select(Ui& ui, const Interaction& input, std::string_view id,
                    const std::vector<std::string>& items, std::optional<std::size_t> selected,
                    SelectState& state, const SelectOptions& options = {});

/**
 * The same control, holding more than one value.
 *
 * `selected` is the caller's own set, as indices into `items` — a vector rather
 * than a `std::set` because the order a reader chose things in is worth keeping
 * and because most callers already have one.
 *
 * `SelectResult::chosen` is the row that was **toggled** this frame, not the new
 * value: the component says what happened and the caller decides what its set
 * becomes, exactly as `checkbox` reports a press rather than writing a bool.
 *
 *     if (const auto hit = result.chosen) {
 *         const auto at = std::find(picked.begin(), picked.end(), *hit);
 *         if (at != picked.end()) picked.erase(at);
 *         else picked.push_back(*hit);
 *     }
 *
 * Set `SelectOptions::multiple` with it. Without that the list closes on the
 * first choice and the rows draw as values rather than as things to tick, which
 * is the single-value control with a set awkwardly attached.
 */
[[nodiscard]] SelectResult select(Ui& ui, const Interaction& input, std::string_view id,
                    const std::vector<std::string>& items,
                    const std::vector<std::size_t>& selected, SelectState& state,
                    const SelectOptions& options = {});

}  // namespace gbui
