// A strip of tabs.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/icons.hpp"

namespace gbui {

struct TabItem {
    std::string_view label;
    /** Drawn before the label, in the label's colour. */
    std::optional<Icon> icon{};
    bool disabled = false;
    /**
     * A heading drawn above this tab, starting a section.
     *
     * Set it on the first tab of each run; repeating the same text on the rest
     * of the run is allowed and draws one heading, which means a caller can
     * label every entry and not have to track where the runs begin.
     *
     * Vertical strips only — a horizontal one has nowhere to put it. Headings
     * are drawn *between* tabs rather than being tabs, so they take no index
     * and the keyboard walks straight past them.
     */
    std::string_view group{};
};

/** Which way the strip runs. Vertical is a sidebar: the same component, the
 *  same keys, the indicator down the side instead of along the bottom. */
enum class TabsOrientation { Horizontal, Vertical };

struct TabsOptions {
    /** A strip along the top or a rail down the side. Both arrow pairs step it
     *  either way, so the keys do not change when the layout does. */
    TabsOrientation orientation = TabsOrientation::Horizontal;
    /** The strip's thickness: its height when horizontal, its width when
     *  vertical. */
    float thickness = 36.0f;
    /** Vertical only — how tall each row is. Horizontal takes its tab height
     *  from `thickness`. */
    float itemHeight = 34.0f;
    float gap = 2.0f;
    /** The moving bar marking the active tab: under it when horizontal, down
     *  its leading edge when vertical. It slides between tabs when an animator
     *  is present and simply appears on the new one when there is not. */
    bool indicator = true;
    float indicatorWidth = 2.0f;
    /**
     * How far the indicator sits from the strip's trailing edge — under a
     * horizontal strip, beside a vertical one.
     *
     * Zero puts it hard against the edge, which is what a strip with a rule
     * under it wants: the bar and the rule are then the same line, one of them
     * lit. A strip without a rule usually wants a pixel or two of air, or the
     * bar reads as an underline attached to the word rather than as a marker
     * under the tab.
     */
    float indicatorInset = 0.0f;
    /**
     * The space inside each tab.
     *
     * Unset takes a default that fits the orientation: horizontal tabs get room
     * under the label so the indicator clears its descenders, and vertical rows
     * keep a lane free on the leading edge so the label does not shift sideways
     * when a tab becomes the active one.
     *
     * Here for the same reason `Style::padding` is on every node — a caller who
     * disagrees with the spacing should be able to say so rather than work
     * around it — and it is the one piece of a tab strip's spacing the caller
     * could not reach.
     */
    std::optional<Edges> itemPadding{};
    /** A rule along the whole strip — under it, or beside it — so the strip
     *  reads as an edge rather than as floating text. */
    bool rule = true;
};

/**
 * Draws the strip and reports which tab the user chose, or nothing.
 *
 * Stateless like everything else: the caller owns the selected index and hands
 * it in. The strip is one focusable stop — the ARIA "roving tabindex" pattern —
 * so Tab moves *past* the whole strip rather than through every tab in it, and
 * the arrow keys move between them once it has the keyboard.
 *
 * Not built yet, and named rather than half-done: an overflow menu for when the
 * tabs do not fit (they currently shrink and elide), a close affordance, and
 * reordering by drag.
 */
std::optional<std::size_t> tabs(Ui& ui, const Interaction& input, std::string_view id,
                                const std::vector<TabItem>& items, std::size_t selected,
                                const TabsOptions& options = {});

struct TabPanelsOptions {
    /**
     * Build only the selected panel, and skip the rest entirely.
     *
     * Off by default, which is what a hidden `<div>` does on the web: it is
     * still in the document, so anything that measures it or looks it up finds
     * it. Turning it on is cheaper — an unbuilt panel costs nothing to lay out
     * or paint — and the price is that everything about the panels the reader
     * is not looking at stops existing: `input.frameOf` on a control inside one
     * returns nothing, and any component that places itself from last frame's
     * geometry starts from scratch when its tab comes back.
     *
     * For a handful of pages the difference is not worth thinking about. For a
     * window whose tabs each hold a table of fifty thousand rows it is the
     * difference between usable and not.
     */
    bool lazy = false;
};

/**
 * Builds the panel bodies for a tab strip.
 *
 * Separate from `tabs` because the two are rarely siblings: a vertical strip
 * sits beside its panels and a horizontal one above them, and a caller often
 * puts a toolbar or a splitter between them.
 *
 * An unselected panel is built inside a box of no height that clips, so it
 * takes part in nothing the reader can see while still being *there* — which is
 * the whole difference `lazy` turns off.
 */
void tabPanels(Ui& ui, std::size_t selected,
               const std::vector<std::function<void(Ui&)>>& panels,
               const TabPanelsOptions& options = {});

}  // namespace gbui
