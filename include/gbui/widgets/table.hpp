// A table: columns that agree, a header that stays, and rows that virtualise.
#pragma once

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/scroll.hpp"
#include "gbui/widgets/virtualList.hpp"

namespace gbui {

/** How a column's width is decided. */
enum class ColumnSize {
    /** Exactly `width`, whatever the table's own width is. */
    Fixed,
    /** A share of what is left after the fixed and fitted columns. */
    Fraction,
    /**
     * As wide as its content needs.
     *
     * Measured from the header's title and `fitSample` — a string the caller
     * says is representative, usually the widest value it expects. It is *not*
     * measured from the cells, and that is a real limit rather than an
     * oversight: a cell is a callback that builds arbitrary UI, so asking it
     * how wide it would like to be means building the whole table twice. For a
     * commit hash, a date or a count — the columns that actually want fitting —
     * a sample is exact.
     */
    FitContent,
};

struct Column {
    std::string_view title{};
    ColumnSize sizing = ColumnSize::Fraction;
    /** Pixels for `Fixed`, the share for `Fraction`, ignored for `FitContent`. */
    float width = 1.0f;
    /** The widest value this column expects, for `FitContent`. */
    std::string_view fitSample{};
    float minWidth = 48.0f;
    float maxWidth = kAuto;
    TextAlign align = TextAlign::Start;
    /**
     * Whether clicking the header sorts by this column.
     *
     * Off by default, and deliberately: sorting is not free for the caller.
     * This widget owns the geometry, not the data — it reports that the reader
     * asked for a different order and the application has to actually reorder
     * its rows. A column that advertises sorting it does not implement is worse
     * than one that never offered, so it is opted into per column.
     */
    bool sortable = false;
    bool resizable = true;
};

/** What a table remembers between frames. Owned by the application. */
struct TableState {
    ScrollState body{};
    /**
     * How far the columns are scrolled sideways.
     *
     * Its own scroll, shared by the header and the rows so they cannot drift
     * apart. Only ever has anywhere to go once the columns are wider than the
     * table — which is what dragging a divider wider is *for*, and until now
     * simply put the far columns somewhere unreachable.
     */
    ScrollState columns{};
    /**
     * Widths a reader has dragged, one per column; `kAuto` leaves the column's
     * own rule in charge. Resized here rather than in `Column` so the layout
     * rules a developer wrote and the widths a reader chose stay separate —
     * shipping a new column order should not throw away either.
     */
    std::vector<float> widths{};
    /** The column being sorted by, or -1. The *ordering* is the application's
     *  job: it owns the data and only it knows how to compare two rows. */
    int sortColumn = -1;
    bool ascending = true;
    /** The selected row, or `npos`. */
    std::size_t selected = static_cast<std::size_t>(-1);
};

struct TableOptions {
    float rowHeight = 30.0f;
    float headerHeight = 32.0f;
    /** The header stays put while the body scrolls under it. */
    bool stickyHeader = true;
    /** Alternating row shading. Off by default: with a hover and a selection
     *  already in play, a third band is usually noise. */
    bool zebra = false;
    /** Builds only the visible rows. Off draws them all, which is right for a
     *  table of twenty and wrong for one of fifty thousand. */
    bool virtualise = true;
    Edges cellPadding = Edges::symmetric(0.0f, 10.0f);
    /** The hairline under the header, and the width of every other rule here. */
    float gridLines = 1.0f;
    /** A rule under every row. Off by default: on a table read top to bottom,
     *  the rows are already separated by their own alignment, and a line under
     *  each one is a lot of ink for no information. Worth turning on when the
     *  cells wrap to different heights, or when the reader is comparing across
     *  a row rather than down a column. */
    bool rowLines = false;
    /** A rule between every column, in the header and in the body — which,
     *  together with `rowLines`, is a full grid. */
    bool columnLines = false;
    float grow = 1.0f;
    float height = kAuto;
};

struct TableResult {
    /** The sort column or direction changed; re-sort the data. */
    bool sortChanged = false;
    bool selectionChanged = false;
    /** Which rows were built, for a caller that wants to know. */
    VirtualSlice shown{};
    /** The resolved width of each column, in order — a caller drawing its own
     *  cells sometimes needs to know. */
    std::vector<float> columnWidths{};
};

/**
 * Draws a table and reports what the reader did to it.
 *
 * `cell` is called for each visible cell and builds whatever belongs there. It
 * is given the row and the column; the box it draws into is already the right
 * width, because **the widths are resolved once for the whole table** rather
 * than negotiated per row. That is the difference between a table and a list of
 * rows that happen to contain the same number of things.
 *
 * The application owns the data, the order and the selection. This owns the
 * geometry.
 */
TableResult table(Ui& ui, const Interaction& input, std::string_view id,
                  const std::vector<Column>& columns, std::size_t rowCount, TableState& state,
                  const std::function<void(Ui&, std::size_t row, std::size_t column)>& cell,
                  const TableOptions& options = {});

}  // namespace gbui
