// A list that builds only what is on screen.
#pragma once

#include <cstddef>
#include <functional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {


/** Which rows a virtual list built this frame.
 *
 *  Returned rather than kept, so a caller can say "showing 41–78 of 50 000"
 *  without counting anything, and a test can assert the slice directly. */
struct VirtualSlice {
    std::size_t first = 0;   ///< index of the first row built
    std::size_t count = 0;   ///< how many were built, overscan included
    std::size_t total = 0;   ///< how many the list was told it has
    /** Row height plus the gap: the distance from one row's top to the next. */
    float pitch = 0.0f;

    std::size_t last() const { return first + count; }
    bool empty() const { return count == 0; }
};

struct VirtualListOptions {
    /** How many rows exist. Only the visible ones are ever built. */
    std::size_t count = 0;
    /** Every row is exactly this tall. The list enforces it rather than
     *  trusting the caller: a row that laid itself out taller would slide the
     *  ones after it out of step with the scrollbar. */
    float rowHeight = 28.0f;
    float gap = 0.0f;
    Edges padding{};
    /** Rows built above and below the viewport. Two is enough to keep a fast
     *  drag from showing an edge, and cheap enough not to think about. */
    std::size_t overscan = 2;

    // Passed through to the scroll container underneath.
    float step = 48.0f;
    bool scrollbar = true;
    float scrollbarWidth = 10.0f;
    bool autoHideScrollbar = true;
    bool focusable = true;
    float grow = 1.0f;
    float width = kAuto;
    float height = kAuto;

    /** The row shape, for `revealRow` and for the arithmetic below. */
    RowMetrics rows() const { return {rowHeight, gap, padding.top}; }
};

/**
 * A list that builds only what is on screen.
 *
 * The trick is not the clipping — the scroll view already clips. It is that the
 * rows that are *not* visible are replaced by two spacers, one standing in for
 * everything above the slice and one for everything below. The content is
 * therefore the full height it would have been, so the scrollbar, `maxOffset`
 * and Page Down all keep working on the real list, while the arena holds the
 * forty rows a person can actually see.
 *
 *     VirtualSlice shown = virtualList(ui, input, "history", state,
 *                                      {.count = commits.size(), .rowHeight = 28.0f},
 *                                      [&](Ui& ui, std::size_t index) {
 *                                          listRow(ui, input, rowId(index), …);
 *                                      });
 *
 * `row` is called once per visible index, in order, inside a box of exactly
 * `rowHeight`. It must not open a scope it leaves open.
 *
 * The viewport is last frame's, like everything else that needs geometry before
 * layout has run, so the first frame after a resize builds the previous size's
 * slice — and overscan is what keeps that from showing.
 */
VirtualSlice virtualList(Ui& ui, const Interaction& input, std::string_view id,
                         ScrollState& state, const VirtualListOptions& options,
                         const std::function<void(Ui&, std::size_t)>& row);

}  // namespace gbui
