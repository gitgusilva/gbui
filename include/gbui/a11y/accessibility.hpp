// What a node carries for a reader who cannot see it: a role, a name, a state,
// a value and its relations to other nodes.
//
// Stages 1 to 3 of the accessibility plan, in one record. They are together
// because they are one answer to one question — *what should be announced when
// the keyboard lands here* — and splitting them across three headers would put
// a slider's `role`, its `value` and the `label` that names it in three places
// a component has to visit separately.
//
// ---- it is not stored on the node, and that is on purpose -----------------
//
// A `Node` is a trivial aggregate so that clearing the arena is a size reset
// rather than a walk, and most nodes have nothing to say here: a row that
// spaces two things out is not a thing a reader is told about. So the record
// lives in a side table the arena owns and a node names by index, exactly as
// vector art already does — four bytes on every node, and the full record only
// on the ones that have one.
//
// ---- relations are tags, not indices --------------------------------------
//
// The tree is rebuilt every frame and only a tag survives that, which is the
// same reason focus, hit testing and the animation clock are all keyed by tag.
// A `NodeId` here would name a different node next frame, or none.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "gbui/a11y/role.hpp"

namespace gbui {

/**
 * A state that may not apply at all, which `bool` cannot express.
 *
 * ARIA omits an attribute that does not apply and reports one that does, and
 * the difference is audible: a checkbox that is not checked is announced as
 * "not checked", while a button — which has no checked state — is announced as
 * a button. A `bool` defaulting to false would give every button in the tree a
 * state it does not have.
 *
 * `Mixed` is the third value a tri-state checkbox has, and the reason this is
 * not `std::optional<bool>`.
 */
enum class Flag : std::uint8_t { Unset, False, True, Mixed };

/** `bool` to `Flag`, for the common case where the state always applies. */
constexpr Flag flag(bool value) { return value ? Flag::True : Flag::False; }

/**
 * Which way a column is sorted — ARIA's `aria-sort`, and its own type because
 * it has four answers and none of them is a boolean.
 *
 * `None` is not `Unset`: a sortable column that is not the one in force says
 * "sortable, not sorted", and a column that cannot be sorted at all says
 * nothing. A reader uses the difference to know which headers are worth
 * pressing.
 */
enum class Sort : std::uint8_t { Unset, None, Ascending, Descending };

/** What is true of a control right now. Everything unset is left unsaid. */
struct AccessibilityState {
    /** Checkboxes, switches, and menu items that toggle. `Mixed` is the
     *  indeterminate form. */
    Flag checked = Flag::Unset;
    /** Anything that opens: a select, a tree item, an accordion. */
    Flag expanded = Flag::Unset;
    /** A row, an option, a tab. Distinct from focus: a list can have a
     *  selection while the keyboard is elsewhere. */
    Flag selected = Flag::Unset;
    /** Held down right now — a toggle button, or a button mid-press. */
    Flag pressed = Flag::Unset;
    /** Present and unusable, which is not the same as absent. */
    Flag disabled = Flag::Unset;
    /** Present, usable, and not editable. */
    Flag readOnly = Flag::Unset;
    /** Failed validation. The control's half of what `field` owns the message
     *  for. */
    Flag invalid = Flag::Unset;
    /** Working: a value is being fetched, a list is loading. */
    Flag busy = Flag::Unset;
    Flag required = Flag::Unset;
    /**
     * The one in the set that the reader is *at* — ARIA's `aria-current`.
     *
     * Not `selected`, and the difference is the whole reason this exists: a
     * breadcrumb trail's last crumb and a paginator's page 3 are not *chosen*
     * from a set of options, they are where you already are. A reader running
     * through a trail of five links needs to know which of them is the page
     * they are on, and `selected` would say something else — that they picked
     * it, and could pick another.
     */
    Flag current = Flag::Unset;
    /**
     * More than one of the children may be selected at once — ARIA's
     * `aria-multiselectable`, on the container rather than on a child.
     *
     * A reader meeting a list of options is told how many of them they may
     * take. Without it a multi-select list is announced exactly like a
     * single-select one, and the first choice appears to have thrown away the
     * previous one.
     */
    Flag multiSelectable = Flag::Unset;
    /** Column headers only. `None` says "sortable, not sorted by". */
    Sort sorted = Sort::Unset;
};

/**
 * What a control currently holds.
 *
 * `text` is the whole point of this being more than a number. A slider that
 * announces "70" is a slider nobody can use; "70 percent" is one they can, and
 * only the component knows which of the two it is — the number carries no unit
 * and the toolkit has no locale to invent one from.
 *
 * **A range is optional.** `minimum == maximum` says there is no range, which
 * is the honest answer for a text box: it has a value and no bounds. Anything
 * with bounds sets all three numbers, and a reader is told "seventy, of a
 * hundred" instead of "seventy".
 */
struct AccessibilityValue {
    /** False leaves the whole value unsaid, which is what a button wants. */
    bool present = false;
    double now = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    /** The value in words. What a reader hears instead of the number, and the
     *  only form a text box has. Empty falls back to the number. */
    std::string_view text{};
};

/**
 * What this node is to other nodes, by tag.
 *
 * These are what make a `label` attach to its control, a `field`'s error attach
 * to its input, and an open `select` report which row is highlighted while
 * focus stays on the box. Without them the three are unrelated nodes that
 * happen to be near each other on screen, which is exactly as much as a screen
 * reader can infer from a rectangle.
 */
struct AccessibilityRelations {
    /** The node whose text is this node's name. Takes precedence over `name`,
     *  as `aria-labelledby` does over `aria-label`. */
    std::string_view labelledBy{};
    /** The node whose text describes this one — help text, an error message. */
    std::string_view describedBy{};
    /** What this node operates: a tab and its panel, a button and its menu. */
    std::string_view controls{};
    /** A child in the accessibility tree that is not a child in this one. An
     *  open popover belongs to the control that opened it, wherever it is
     *  drawn. */
    std::string_view owns{};
    /** Which descendant is "current" while focus stays here — the highlighted
     *  row of an open select, the focused cell of a grid. */
    std::string_view activeDescendant{};

    // ---- the same two relations, pointed the other way ---------------------
    //
    // `labelledBy` and `describedBy` belong on the *control*, and the control
    // is not what knows about them: a caption is built before the input it
    // names, a `field`'s error after it, and a component never reaches into
    // another component's node. So the end that knows says so, and the
    // accessibility tree turns it round when it is built.
    //
    // This is `<label for>` exactly — the attribute is on the label, and the
    // browser resolves it onto the control.

    /** The control this node names. Becomes that control's `labelledBy`. */
    std::string_view labels{};
    /** The control this node describes — help text, an error message. Becomes
     *  that control's `describedBy`. */
    std::string_view describes{};
};

/** Everything a node says about itself, or nothing at all. */
struct Accessibility {
    Role role = Role::None;
    /**
     * This node and everything under it is not in the accessibility tree —
     * ARIA's `aria-hidden`.
     *
     * Not the same as `Role::None`, which means "I am not a thing, but my
     * children might be". This means the subtree is not there at all, and it
     * exists for content that is drawn twice on purpose: a marquee draws its
     * text a second time to hide the seam, and a reader given both would be
     * read the same sentence twice with nothing to say why.
     */
    bool hidden = false;
    /**
     * What this is called. Interned when it is set, so a temporary is safe.
     *
     * A name is not a description of what will happen — "Commit", not "Click to
     * commit the staged changes". The verb is already in the role.
     */
    std::string_view name{};
    /** The longer sentence, announced after the name and only if the reader
     *  asks for it. */
    std::string_view description{};
    AccessibilityState state{};
    AccessibilityValue value{};
    AccessibilityRelations relations{};

    /**
     * Which item of how many, one-based. Zero on both means "not one of a set".
     *
     * This exists for one reason and it is a good one: **a virtualised list
     * builds only the rows on screen.** Without these a reader walking fifty
     * thousand commits is told "row 3 of 14" and then "row 4 of 14" for the
     * rest of their life, because fourteen is all that was ever in the tree.
     * The count belongs to the data and only the caller has it.
     */
    std::size_t positionInSet = 0;
    std::size_t setSize = 0;
    /**
     * How deep in a hierarchy, one-based. Zero is "not in one".
     *
     * ARIA's `aria-level`, and a tree is the first thing here to need it: "item
     * 2 of 5" is half an answer in a hierarchy, because the other half is
     * *whose* five. Without it a reader walking a branch list hears the same
     * sentence at every depth.
     */
    std::size_t level = 0;
};

}  // namespace gbui
