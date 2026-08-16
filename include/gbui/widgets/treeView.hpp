// An expandable hierarchy: a branch sidebar, a file tree, an outline.
//
// The component inventory calls this the single biggest gap for a git client,
// and the reason is that the three things it needs are each easy and are never
// all three: an expansion model, keyboard walking that matches what a reader
// expects from every file browser they have used, and virtualisation, because a
// repository's file tree is not a hundred rows.
//
// ---- the shape of the data, which is the whole design ----------------------
//
// **A flat vector in pre-order, with a depth on each row.** Not a recursive
// structure, and not a "give me the children of X" callback.
//
// Flat is what makes virtualisation possible at all: the rows on screen are a
// slice, and a slice of a tree is only a slice if the tree is already a
// sequence. It is also what the caller usually has — a `git ls-tree` walk, a
// directory listing, an outline — and the depth is the one thing they always
// know.
//
// What the component owns from there is everything that is genuinely its
// business: which rows are *visible* given what is collapsed, the twisties, the
// indentation, the keys, the scrolling, and what a screen reader is told. What
// the caller keeps is the data and the answer.
//
// ---- one keyboard stop, and the keys a file browser has ---------------------
//
// The tree is one Tab stop and the arrows move inside it — the roving pattern
// `tabs` and the calendar already use. Right opens a closed node and steps into
// an open one; Left closes an open node and steps *out* of a closed one. That
// pair is the whole of why a tree feels like a tree, and getting it wrong —
// making Right always step, say — is what makes one feel like an indented list.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"
#include "gbui/widgets/scroll.hpp"
#include "gbui/widgets/virtualList.hpp"

namespace gbui {

/** One row of the flattened hierarchy. */
struct TreeItem {
    /**
     * Its identity, and the caller's to keep stable.
     *
     * A string, like everything here that has to survive the tree being rebuilt
     * — and a tree has more riding on it than most: what is open, what is
     * selected and where the keyboard is are all remembered by it. A path is
     * the obvious choice and usually the right one.
     */
    std::string_view id{};
    std::string_view label{};
    /** How deep. Zero is a root, and the rows after one are its subtree until
     *  the next row at its depth or shallower. */
    std::size_t depth = 0;
    /**
     * Whether it can be opened.
     *
     * Deliberately not "has rows after it at a greater depth". A lazy tree
     * knows a directory has contents before it has read them, and a twisty that
     * only appears once the contents arrive is a twisty nobody presses. Say so
     * here and fill the rows in when they land.
     */
    bool hasChildren = false;
    /** Drawn before the label, after the twisty. */
    std::optional<Icon> icon{};
    /** Trailing, muted — a count, a short hash, a size. */
    std::string_view detail{};
    bool disabled = false;
};

/** What the application remembers. */
struct TreeState {
    /** The ids of every open node. A set rather than a vector: a file tree with
     *  a thousand rows asks this question a thousand times a frame. */
    std::set<std::string, std::less<>> expanded;
    /** The chosen row, or empty. One at a time — a tree that multi-selects is a
     *  different control and a bigger one. */
    std::string selected;
    /** Where the keyboard is *inside* the tree, which is not the same as which
     *  row is chosen: walking a tree is not choosing from it, the same
     *  separation `select` makes between its highlight and its value. */
    std::string focused;
    ScrollState view;

    bool isExpanded(std::string_view id) const { return expanded.find(id) != expanded.end(); }
    void toggle(std::string_view id) {
        if (const auto found = expanded.find(id); found != expanded.end()) expanded.erase(found);
        else expanded.emplace(id);
    }
};

struct TreeViewOptions {
    float rowHeight = 26.0f;
    /** How far one level steps in. */
    float indent = 15.0f;
    /**
     * A hairline down each open level, joining a parent to its children.
     *
     * On, because indentation alone stops being readable at about four levels —
     * the eye cannot hold which column belongs to which parent — and a file
     * tree is routinely deeper than that.
     */
    bool guides = true;
    /** Builds only the rows on screen. Off draws them all, which is right for
     *  an outline of twenty and wrong for a repository. */
    bool virtualise = true;
    /** What the hierarchy is of — "Branches", "Files". */
    std::string_view name{};
    float height = kAuto;
    float width = kAuto;
    float grow = 1.0f;
};

struct TreeResult {
    /** Activated this frame — clicked, or Return or Space on it. Selection has
     *  already moved; this is the "and they meant it" half. */
    std::optional<std::string_view> activated{};
    /** Opened or closed this frame. `state.expanded` already holds the answer. */
    std::optional<std::string_view> toggled{};
    /** The selection changed. */
    bool selectionChanged = false;
    /** Which rows were built, for a caller that wants to know. */
    VirtualSlice shown{};
};

/**
 * Draws the visible rows of `items` and walks them.
 *
 * `items` is the **whole** hierarchy, flattened in pre-order. Which of it is on
 * screen is worked out here from `state.expanded`, so a caller that collapses a
 * node passes exactly the same vector as before.
 */
TreeResult treeView(Ui& ui, const Interaction& input, std::string_view id,
                    const std::vector<TreeItem>& items, TreeState& state,
                    const TreeViewOptions& options = {});

}  // namespace gbui
