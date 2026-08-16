// Short-lived messages, stacked in a corner and gone on their own.
//
// The thing every application ends up writing badly: a queue, a timer, a place
// to put them, and — the part that is usually missed entirely — a live region,
// because a message a reader never hears is a message that was not delivered.
//
// ---- what the application owns ---------------------------------------------
//
// The queue, as `ToastState`. That is the same contract every other component
// here has, and it matters more than usual: toasts are *raised from anywhere* —
// a network reply, a file watcher, a keyboard shortcut three screens away — and
// a component that owned them would be a component with a global. Pushing one
// is `state.push({.kind = …, .message = …})` from wherever the thing happened,
// and drawing them is one call at the end of the frame.
//
// `elapsed` is the exception that proves the rule: the outlet advances it,
// because a timer needs a clock and the clock is the frame. `MarqueeState` does
// the same with its offset, and for the same reason.
//
// ---- where they go ----------------------------------------------------------
//
// Six corners and edges, or **anywhere at all**: `Placement::Anchored` puts the
// stack against a tagged node using the same engine a popover uses, and
// `bounds` says which rectangle the corners are measured from, so a stack can
// live inside a panel rather than in the window. Which way the stack *grows* is
// never a decision the caller has to make — it grows away from the edge it is
// anchored to, so a top-left stack reads downwards and a bottom-right one
// upwards, which is what everybody expects and nobody wants to configure.
//
// ---- the timer, and why hovering stops it ----------------------------------
//
// A message that vanishes while it is being read is a message that was not
// delivered either. The stack pauses while the pointer is over it or the
// keyboard is inside it, which is Toastify's behaviour and also what WCAG's
// "enough time" rule asks for: a reader who needs longer has a way to take it.
// A `duration` of zero never expires at all, which is the right answer for
// anything the reader has to act on.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/core/geometry.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

/**
 * What kind of news it is, which decides the colour, the glyph, and — the part
 * that is not decoration — whether a screen reader is interrupted.
 *
 * `Info` and `Success` are polite: they wait for a pause. `Warning` and `Error`
 * interrupt, because something has gone wrong and the reader is probably about
 * to do the next thing in a sequence that will not work.
 */
enum class ToastKind { Info, Success, Warning, Error };

/** Where a stack sits. */
enum class ToastPlacement {
    TopLeft, TopCenter, TopRight,
    BottomLeft, BottomCenter, BottomRight,
    /** Against a tagged node instead of an edge, placed by the same engine a
     *  popover uses — including the flip that keeps it on screen. Needs
     *  `ToastOptions::anchorId`. */
    Anchored,
};

/** One message. */
struct ToastEntry {
    /**
     * Its identity, and **the whole of the grouping mechanism**.
     *
     * Everything that survives a frame here is keyed by a string, and a toast
     * has more than most riding on it: the timer, the reader's place in the
     * stack, and whether this is news at all. `push` treats two entries with
     * the same id as one, bumping a count instead of stacking a second copy —
     * which is how a retry loop reports "still offline" once rather than forty
     * times, and it is the failure every application's first toast queue has.
     *
     * **Empty derives one from the kind, the title and the message**, so
     * identical messages collapse without the caller thinking about it. Set it
     * where they should not: forty upload failures with forty filenames in them
     * already differ, and forty that say only "Upload failed" should not.
     */
    std::string id{};
    ToastKind kind = ToastKind::Info;
    /** The headline. Empty draws none and leaves the message alone. */
    std::string title{};
    std::string message{};
    /**
     * Seconds before it goes on its own. **Zero never expires.**
     *
     * Zero for anything the reader has to act on, and for anything they cannot
     * get back — an error with a Retry on it is not a thing to take away after
     * four seconds.
     */
    double duration = 5.0;
    /**
     * Which outlet shows it, when there is more than one.
     *
     * Empty is the default outlet. A second `toast()` call with a `group` set
     * shows only the entries carrying that group — which is how a dialog puts
     * its own messages inside itself while the application's go to the corner.
     */
    std::string group{};
    /** The × in the corner. Off for something that only reports, on for
     *  anything the reader may want out of the way. */
    bool closable = true;
    /** Instead of the one the kind would choose. */
    std::optional<Icon> icon{};
    /** A single action — "Undo", "Retry". Empty draws none. */
    std::string action{};

    // ---- owned by the outlet, not by the caller --------------------------
    /** Seconds this has been on screen, advanced by `toast()`. Paused while the
     *  stack is hovered or holds the keyboard. */
    double elapsed = 0.0;
    /** How many identical messages this one stands for — see `id`. One means
     *  one, and draws no badge. */
    std::size_t count = 1;
};

/**
 * The queue, owned by the application.
 *
 * A plain vector, deliberately: raising a toast from a network callback should
 * not need a handle to a component, and inspecting the queue in a test should
 * not need one either.
 */
struct ToastState {
    std::vector<ToastEntry> items;

    /**
     * Adds one, or bumps the count of the one already saying it.
     *
     * Returns the id by value rather than as a view: the vector may have
     * reallocated, and a view into it would be a view into whatever the
     * allocator did next.
     */
    std::string push(ToastEntry entry);
    /** Takes one out by id. Does nothing when it is already gone, which is the
     *  normal case for a caller racing the timer. */
    void dismiss(std::string_view id);
    void clear() { items.clear(); }
    bool empty() const { return items.empty(); }
};

struct ToastOptions {
    /** Which corner, edge or anchor the stack sits against. Which way it
     *  *grows* follows from it and is not a second decision. */
    ToastPlacement placement = ToastPlacement::TopRight;
    /** Which entries this outlet shows. Empty shows the ungrouped ones. */
    std::string_view group{};
    /** The tag `ToastPlacement::Anchored` measures against. */
    std::string_view anchorId{};
    /**
     * The rectangle the corners are measured from. Empty is the window.
     *
     * What lets a stack live inside a panel rather than over the whole screen —
     * a docked tool reporting into its own pane rather than across the
     * application it is embedded in.
     */
    Rect bounds{};
    /** A bar draining across the foot of each toast, showing the time left.
     *  Drawn only where there *is* a time left: a toast with no duration has
     *  nothing to count down and gets none. */
    bool progress = true;
    /** How many are on screen at once. The rest wait their turn, which is
     *  better than a stack taller than the window. */
    std::size_t maxVisible = 4;
    float width = 340.0f;
    /** Between two toasts in the stack. */
    float gap = 8.0f;
    /** Between the stack and the edge it is anchored to. */
    float margin = 16.0f;
    /** What the region is called. A reader arriving at it out of context is
     *  otherwise told only that something changed. */
    std::string_view name = "Notifications";
};

struct ToastResult {
    /** Dismissed this frame — by the ×, by Escape while the keyboard is inside
     *  the stack, or by its timer running out. Already removed from the queue;
     *  here so a caller can react. */
    std::optional<std::string_view> dismissed{};
    /** Its action was pressed. The toast is still there: only the caller knows
     *  whether "Undo" should close it. */
    std::optional<std::string_view> activated{};
};

/**
 * Draws the stack and advances its timers. `delta` is the frame's own seconds.
 *
 * Called once per outlet, near the end of the frame — it draws on the overlay
 * layer, so where it sits in the tree decides nothing about where it appears,
 * but building it last keeps it above anything that shares that layer.
 *
 * **It never takes the keyboard.** A message that stole focus would interrupt
 * whatever the reader was typing, and the live region is what delivers it
 * instead; only the × and the action are Tab stops, and only while they exist.
 */
ToastResult toast(Ui& ui, const Interaction& input, ToastState& state, float delta,
                  const ToastOptions& options = {});

}  // namespace gbui
