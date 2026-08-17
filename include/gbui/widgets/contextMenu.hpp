// A menu, and the same menu at a point rather than under a control.
//
// `menuItem` has always been the row; this is the box the rows go in, and the
// keyboard that walks them. Without it every caller wired a `popover` to a list
// of items by hand — which meant every caller got the geometry right and the
// keyboard wrong, because the geometry is visible and the keyboard is not.
//
// ---- one component, two anchors ---------------------------------------------
//
// A dropdown hangs off a control and a context menu appears where the pointer
// was. That is the *only* difference: the rows, the highlight, the keys, the
// dismissal and the roles are identical, so it is one implementation with two
// ways to say where it goes. Anchoring to a zero-sized rectangle at a point is
// what the placement engine already does for a tooltip on a caret.
//
// ---- the keyboard is the reason this exists ---------------------------------
//
// A menu is **one Tab stop, not one per row** — a menu of nine commands that
// took nine Tab presses to cross would be a menu nobody uses from the keyboard.
// The highlight moves with the arrows, Home and End reach the ends, Enter and
// Space activate, Escape closes, and the highlight skips separators and disabled
// rows rather than landing on them and appearing stuck.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/floating.hpp"
#include "gbui/widgets/menu.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

/** One row. A separator is an entry with no label, so a caller builds the whole
 *  menu as one list rather than interleaving two kinds of call. */
struct MenuEntry {
    /** Identifies the row in the result. Empty makes it a separator. */
    std::string_view id{};
    std::string_view label{};
    std::optional<Icon> icon{};
    /** Advertised, not bound: a menu is where a shortcut is *shown*, and the
     *  application is where it is handled. */
    std::string_view shortcut{};
    /** Drawn with a tick — a menu item that toggles something. */
    bool checked = false;
    bool disabled = false;
    /** The destructive one, in the error colour. */
    bool danger = false;

    bool separator() const { return id.empty() && label.empty(); }
};

/** Where the highlight is. The application's, like every other piece of state
 *  the toolkit reads — and cleared by the caller when the menu closes. */
struct MenuState {
    /** The highlighted row's `MenuEntry::id`, or empty. An id rather than an
     *  index so a menu whose entries change under it does not silently
     *  highlight something else. */
    std::string highlighted{};
    /** Where a menu too long for the window is scrolled to. Written by the
     *  component; a caller only has to hold it. */
    ScrollState list{};
};

struct MenuOptions : FloatingOptions {
    /** What the menu is, announced before its first row. */
    std::string_view name{};
    /** How wide it is. Zero sizes it to its widest row, which is what a menu
     *  should do — a fixed width either truncates a command or leaves a gap. */
    float width = 0.0f;
    /** How many rows before it scrolls. */
    std::size_t maxVisible = 14;
};

struct MenuResult {
    /** The row activated this frame, by pointer or by keyboard. */
    std::optional<std::string_view> chosen{};
    /** The reader asked to put it away — Escape, or a press outside it. Act on
     *  it by clearing whatever `open` flag the caller owns; the component holds
     *  no such flag, for the reason every floating thing here does not. */
    bool dismissed = false;
    /**
     * Focus this, if anything.
     *
     * The menu itself when it has just opened, so the arrows work without the
     * reader having to click a row first. Handed back rather than acted on, the
     * contract every component here shares.
     */
    std::optional<std::string_view> focus{};
};

/**
 * A menu hanging off `anchorId`.
 *
 * Draw it while the caller's own flag says it is open, exactly as `popover` is
 * drawn — this holds no open state of its own.
 */
MenuResult menu(Ui& ui, const Interaction& input, std::string_view id, std::string_view anchorId,
                const std::vector<MenuEntry>& entries, MenuState& state,
                const MenuOptions& options = {});

/**
 * The same menu, at a point.
 *
 * `at` is where the pointer was when the reader asked for it — keep it, because
 * a menu that re-reads the live pointer position walks away from itself as the
 * reader moves towards it.
 *
 *     if (input.rightClicked("row")) { menuAt = input.pointer(); menuOpen = true; }
 *     if (menuOpen) {
 *         const MenuResult hit = contextMenu(ui, input, "row.menu", menuAt, items, state);
 *         if (hit.dismissed || hit.chosen) menuOpen = false;
 *     }
 */
MenuResult contextMenu(Ui& ui, const Interaction& input, std::string_view id, Vec2 at,
                       const std::vector<MenuEntry>& entries, MenuState& state,
                       const MenuOptions& options = {});

}  // namespace gbui
