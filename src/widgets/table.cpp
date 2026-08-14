#include "gbui/widgets/table.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

constexpr std::size_t kNoRow = static_cast<std::size_t>(-1);
/** The grab area on a column's trailing edge. Wider than the line it moves, or
 *  it would be a target nobody can hit. */
constexpr float kResizeGrip = 7.0f;

/**
 * The widths, resolved once for the whole table.
 *
 * Fixed and fitted columns take what they need; whatever is left is shared out
 * among the fractional ones by their weight, and anything that hit a bound
 * hands its surplus back to the rest. That last part is why this is a loop and
 * not a division: a fractional column pinned at its minimum would otherwise
 * swallow space it cannot use and leave the table short.
 */
/**
 * A width held inside a column's own bounds.
 *
 * Its own function because the obvious spelling is wrong in a way that
 * compiles: `std::clamp(w, min, isAuto(max) ? w : max)` passes `lo > hi`
 * whenever `w` is under the minimum, and `std::clamp` with the bounds the wrong
 * way round is **undefined behaviour**. In practice it returned the upper
 * bound, so a minimum width was silently ignored — a column asked never to go
 * under 150 came out at 95.
 */
float bounded(float width, const Column& column) {
    const float upper =
        isAuto(column.maxWidth) ? std::numeric_limits<float>::infinity() : column.maxWidth;
    return std::max(column.minWidth, std::min(width, upper));
}

std::vector<float> resolveWidths(const Ui& ui, const std::vector<Column>& columns,
                                 const std::vector<float>& overrides, float available,
                                 const Edges& padding) {
    const std::size_t count = columns.size();
    std::vector<float> widths(count, 0.0f);
    std::vector<bool> frozen(count, false);

    TextStyle heading;
    heading.weight = FontWeight::SemiBold;
    heading.size = 11.0f;
    TextStyle body;
    body.size = 12.0f;

    float taken = 0.0f;
    float weightTotal = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const Column& column = columns[i];
        const bool dragged = i < overrides.size() && !isAuto(overrides[i]);
        if (dragged) {
            widths[i] = overrides[i];
            frozen[i] = true;
        } else if (column.sizing == ColumnSize::Fixed) {
            widths[i] = column.width;
            frozen[i] = true;
        } else if (column.sizing == ColumnSize::FitContent) {
            // The header plus the sample, whichever is wider. Room for the sort
            // arrow is part of the header's cost, not an afterthought.
            const float title =
                ui.canMeasure() ? ui.measure(column.title, heading).width + 18.0f : 60.0f;
            // Measured in the style the *cell* draws in, not the table's:
            // see `Column::fitStyle`. Unset, it is the table's own, which is
            // what a column of prose wants.
            TextStyle sampleStyle = column.fitStyle;
            if (isAuto(sampleStyle.size)) sampleStyle.size = body.size;
            const float sample = column.fitSample.empty() || !ui.canMeasure()
                                     ? 0.0f
                                     : ui.measure(column.fitSample, sampleStyle).width;
            widths[i] = std::max(title, sample) + padding.horizontal();
            frozen[i] = true;
        } else {
            weightTotal += std::max(0.0f, column.width);
        }
        if (frozen[i]) {
            widths[i] = bounded(widths[i], column);
            taken += widths[i];
        }
    }

    // Share the remainder, freezing whatever hits a bound and repeating with
    // what those refused — the same loop the flex pass runs, for the same
    // reason.
    float remaining = std::max(0.0f, available - taken);
    for (std::size_t pass = 0; pass <= count; ++pass) {
        if (weightTotal <= 0.0f) break;
        float overflow = 0.0f;
        float stillFlexible = 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            if (frozen[i]) continue;
            const Column& column = columns[i];
            const float share = remaining * std::max(0.0f, column.width) / weightTotal;
            const float clamped = bounded(share, column);
            widths[i] = clamped;
            if (clamped != share) {
                overflow += clamped - share;
            } else {
                stillFlexible += std::max(0.0f, column.width);
            }
        }
        if (std::fabs(overflow) < 0.01f || stillFlexible <= 0.0f) break;
        for (std::size_t i = 0; i < count; ++i) {
            if (frozen[i]) continue;
            const Column& column = columns[i];
            const float share = remaining * std::max(0.0f, column.width) / weightTotal;
            if (widths[i] != share) {
                frozen[i] = true;
                remaining -= widths[i];
                weightTotal -= std::max(0.0f, column.width);
            }
        }
    }
    return widths;
}

}  // namespace

TableResult table(Ui& ui, const Interaction& input, std::string_view id,
                  const std::vector<Column>& columns, std::size_t rowCount, TableState& state,
                  const std::function<void(Ui&, std::size_t row, std::size_t column)>& cell,
                  const TableOptions& options) {
    TableResult result;
    if (columns.empty()) return result;

    const std::string headerId = std::string(id) + ".header";
    // One width for both bars, so the corner they leave between them is square.
    constexpr float kScrollbarWidth = 10.0f;
    const std::string bodyId = std::string(id) + ".body";

    // The width the columns share out. Last frame's, like every other piece of
    // geometry a component needs before layout has run.
    const Rect frame = input.frameOf(id);
    const float bar = state.body.scrollable() ? 10.0f : 0.0f;
    // The frame is the border box, so the border has to come off as well as the
    // scrollbar. Two pixels sounds ignorable and is not: the columns would take
    // the full width, the content would end up wider than the viewport by
    // exactly the border, and the table would offer a sideways scroll of two
    // pixels on a table whose columns fit perfectly.
    constexpr float kBorder = 1.0f;
    const float available = std::max(0.0f, frame.width - bar - kBorder * 2.0f);
    result.columnWidths = resolveWidths(ui, columns, state.widths, available, options.cellPadding);

    // ---- dragging a divider ------------------------------------------------
    state.widths.resize(columns.size(), kAuto);
    for (std::size_t i = 0; i + 1 <= columns.size(); ++i) {
        if (!columns[i].resizable) continue;
        const std::string gripId = std::string(id) + ".grip." + std::to_string(i);
        if (input.dragging() != gripId) continue;
        const float wanted = result.columnWidths[i] + input.pointerDelta().x;
        state.widths[i] = bounded(wanted, columns[i]);
        // Re-resolved this frame, so the drag is not a frame behind the pointer.
        result.columnWidths =
            resolveWidths(ui, columns, state.widths, available, options.cellPadding);
    }

    // ---- the keyboard ------------------------------------------------------
    if (rowCount > 0 && input.isFocusedWithin(bodyId)) {
        const std::size_t before = state.selected;
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Down) {
                state.selected =
                    state.selected == kNoRow ? 0 : std::min(rowCount - 1, state.selected + 1);
            } else if (event.key == Key::Up) {
                state.selected =
                    state.selected == kNoRow || state.selected == 0 ? 0 : state.selected - 1;
            } else if (event.key == Key::Home) {
                state.selected = 0;
            } else if (event.key == Key::End) {
                state.selected = rowCount - 1;
            }
        }
        if (state.selected != before) {
            result.selectionChanged = true;
            revealRow(state.body, RowMetrics{options.rowHeight, 0.0f, 0.0f}, state.selected);
        }
    }

    Style outer;
    outer.direction = Direction::Column;
    outer.grow = options.grow;
    if (options.grow > 0.0f) outer.basis = 0.0f;
    outer.height = options.height;
    outer.border = Border{1.0f, Fill{Token::Border}};
    outer.radius = 6.0f;
    outer.overflow = Overflow::Hidden;
    auto scope = ui.scope(outer);
    ui.tag(id);

    // Everything below scrolls sideways together. The header has to be inside
    // this and outside the *vertical* one: it must follow the columns left and
    // right, and stay put up and down.
    const std::string acrossId = std::string(id) + ".across";
    ScrollOptions across;
    across.direction = Direction::Column;
    across.axis = ScrollAxis::Horizontal;
    across.grow = options.grow > 0.0f || !isAuto(options.height) ? 1.0f : 0.0f;
    auto acrossScope = scrollArea(ui, input, acrossId, state.columns, across);

    // ---- the header --------------------------------------------------------
    //
    // A sibling of the scroll view, not a row inside it: that is what makes it
    // stay put, and it costs nothing because the widths are already agreed.
    {
        Style header;
        header.direction = Direction::Row;
        header.align = Align::Center;
        header.height = options.headerHeight;
        header.shrink = 0.0f;
        header.background = Fill{Token::BgElevated};
        auto headerScope = ui.scope(header);
        ui.tag(headerId);

        for (std::size_t i = 0; i < columns.size(); ++i) {
            const Column& column = columns[i];
            const std::string cellId = std::string(id) + ".head." + std::to_string(i);
            const bool sorted = state.sortColumn == static_cast<int>(i);

            {
                // Its own block: a Scope closes when it leaves scope, so a grip
                // written after it here would land *inside* the cell, at an
                // offset measured from the cell's own box. That is what made the
                // resize handles sit somewhere off to the right of the header.
                Style head;
                head.direction = Direction::Row;
                head.align = Align::Center;
                head.gap = 4.0f;
                head.width = result.columnWidths[i];
                head.shrink = 0.0f;
                head.padding = options.cellPadding;
                head.justify = column.align == TextAlign::End      ? Justify::End
                               : column.align == TextAlign::Center ? Justify::Center
                                                                   : Justify::Start;
                // No hover surface on a header. A row lights up because the
                // reader is picking one *out* of many; a column title is not
                // picked out of anything, and the two affordances it does need
                // — that it can be clicked, and which way it would sort — are
                // already carried by the cursor and by the arrow every sortable
                // column wears permanently.
                head.cursorHint = column.sortable ? Cursor::Pointer : Cursor::Default;
                auto headScope = ui.scope(head);
                ui.tag(cellId).cursor(head.cursorHint);

                text(ui, column.title,
                     {.color = sorted ? Token::TextStrong : Token::TextMuted,
                      .weight = FontWeight::SemiBold,
                      .size = 11.0f});
                // Every sortable column carries its arrow, not just the sorted
                // one. Two reasons, and the second is the one that bites: a
                // column that only shows its arrow once clicked never tells the
                // reader it *can* be clicked, and an arrow that appears on sort
                // widens the header cell, so the titles shift sideways the
                // first time anyone sorts. Muted and pointing the way a click
                // would sort it; accented once it is the one in force.
                if (column.sortable) {
                    const bool up = sorted ? state.ascending : true;
                    // The inactive arrow recedes by weight rather than by
                    // colour: there is no token dimmer than `TextMuted`, and a
                    // thinner stroke at a smaller size reads as "available"
                    // next to the sorted column's solid accent.
                    icon(ui, up ? Icon::ChevronUp : Icon::ChevronDown,
                         {.color = sorted ? Token::Accent : Token::TextMuted,
                          .size = sorted ? 12.0f : 11.0f,
                          .stroke = sorted ? 2.0f : 1.5f});
                }
                (void)headScope;
            }

            if (column.sortable && input.clicked(cellId)) {
                // Clicking the column already sorted turns it round; a new one
                // starts ascending, which is what every table does.
                state.ascending = sorted ? !state.ascending : true;
                state.sortColumn = static_cast<int>(i);
                result.sortChanged = true;
            }
        }

        if (options.columnLines && options.gridLines > 0.0f) {
            float at = 0.0f;
            for (std::size_t i = 0; i + 1 < columns.size(); ++i) {
                at += result.columnWidths[i];
                Style rule;
                rule.position = Position::Absolute;
                rule.left = at;
                rule.top = 0.0f;
                rule.width = options.gridLines;
                rule.height = Length::percent(100);
                rule.background = Fill{Token::Border};
                ui.add(rule);
            }
        }

        // The grips, as children of the *header row* and placed at the running
        // boundary — half in each column, so the pointer finds one without
        // having to be exact.
        float boundary = 0.0f;
        for (std::size_t i = 0; i + 1 < columns.size(); ++i) {
            boundary += result.columnWidths[i];
            if (!columns[i].resizable) continue;
            const std::string gripId = std::string(id) + ".grip." + std::to_string(i);
            const bool active = input.isHovered(gripId) || input.dragging() == gripId;

            Style grip;
            grip.position = Position::Absolute;
            grip.left = boundary - kResizeGrip / 2.0f;
            grip.top = 0.0f;
            grip.width = kResizeGrip;
            grip.height = options.headerHeight;
            grip.justify = Justify::Center;
            // Over the header text, so a narrow column's title cannot take the
            // press meant for the divider.
            grip.zIndex = 1;
            grip.cursorHint = Cursor::ResizeHorizontal;
            auto gripScope = ui.scope(grip);
            ui.tag(gripId).cursor(Cursor::ResizeHorizontal);

            // A hairline that thickens and takes the accent while it is being
            // used: the divider is always there, the handle only when wanted.
            Style line;
            line.width = active ? 2.0f : 1.0f;
            line.height = Length::percent(100);
            line.background = Fill{active ? Token::Accent : Token::Border};
            ui.add(line);
            (void)gripScope;
        }
        (void)headerScope;
    }

    if (options.gridLines > 0.0f) {
        Style rule;
        rule.height = options.gridLines;
        rule.shrink = 0.0f;
        rule.background = Fill{Token::Border};
        ui.add(rule);
    }

    // ---- the rows ----------------------------------------------------------
    /** One row, whichever way it was reached. */
    const auto buildRow = [&](Ui& rowUi, std::size_t row) {
        const std::string rowId = std::string(id) + ".row." + std::to_string(row);
        const bool chosen = state.selected == row;

        Style line;
        line.direction = Direction::Row;
        line.align = Align::Center;
        line.height = options.rowHeight;
        line.shrink = 0.0f;
        if (chosen)
            line.background = Fill{Token::Accent, 0.18f};
        else if (input.isHovered(rowId))
            line.background = Fill{Token::SurfaceHover};
        else if (options.zebra && row % 2 == 1)
            line.background = Fill{Token::BgElevated, 0.45f};
        line.cursorHint = Cursor::Pointer;
        auto lineScope = rowUi.scope(line);
        rowUi.tag(rowId).cursor(Cursor::Pointer);

        for (std::size_t i = 0; i < columns.size(); ++i) {
            Style box;
            box.direction = Direction::Row;
            box.align = Align::Center;
            box.width = result.columnWidths[i];
            box.shrink = 0.0f;
            box.padding = options.cellPadding;
            box.overflow = Overflow::Hidden;
            box.justify = columns[i].align == TextAlign::End      ? Justify::End
                          : columns[i].align == TextAlign::Center ? Justify::Center
                                                                  : Justify::Start;
            auto boxScope = rowUi.scope(box);
            cell(rowUi, row, i);
            (void)boxScope;
        }

        // Both rules are absolute children of the row rather than elements in
        // it, so they take no space: a row is exactly `rowHeight` tall whether
        // it is ruled or not, and the virtual list's arithmetic — which counts
        // rows by that height — stays true.
        if (options.gridLines > 0.0f && (options.rowLines || options.columnLines)) {
            if (options.rowLines) {
                Style rule;
                rule.position = Position::Absolute;
                rule.left = 0.0f;
                rule.top = options.rowHeight - options.gridLines;
                rule.width = Length::percent(100);
                rule.height = options.gridLines;
                rule.background = Fill{Token::Border};
                rowUi.add(rule);
            }
            if (options.columnLines) {
                float at = 0.0f;
                for (std::size_t i = 0; i + 1 < columns.size(); ++i) {
                    at += result.columnWidths[i];
                    Style rule;
                    rule.position = Position::Absolute;
                    rule.left = at;
                    rule.top = 0.0f;
                    rule.width = options.gridLines;
                    rule.height = Length::percent(100);
                    rule.background = Fill{Token::Border};
                    rowUi.add(rule);
                }
            }
        }
        (void)lineScope;
    };

    // The rows scroll without a bar of their own: theirs would be drawn against
    // *their* right-hand edge, which is out at the end of the widest column and
    // therefore off screen the moment the columns are wider than the table. It
    // is drawn below instead, against the table itself — which is where a
    // browser puts it, and the only place a reader can reach it.
    if (options.virtualise) {
        VirtualListOptions rows;
        rows.count = rowCount;
        rows.rowHeight = options.rowHeight;
        rows.step = options.rowHeight * 3.0f;
        rows.scrollbar = false;
        result.shown = virtualList(ui, input, bodyId, state.body, rows,
                                   [&](Ui& rowUi, std::size_t row) { buildRow(rowUi, row); });
    } else {
        ScrollOptions view;
        view.axis = ScrollAxis::Vertical;
        view.scrollbar = false;
        auto body = scrollArea(ui, input, bodyId, state.body, view);
        for (std::size_t row = 0; row < rowCount; ++row) buildRow(ui, row);
        result.shown = VirtualSlice{0, rowCount, rowCount, options.rowHeight};
        (void)body;
    }

    acrossScope.close();

    // Against the table's own box, under the header and above the sideways bar
    // — the corner where the two would cross belongs to neither.
    const Rect box = input.frameOf(id);
    if (!box.empty()) {
        const float sideways = state.columns.scrollable() ? kScrollbarWidth : 0.0f;
        scrollbar(ui, input, bodyId, state.body,
                  Rect{0.0f, options.headerHeight, box.width,
                       std::max(0.0f, box.height - options.headerHeight - sideways)},
                  ScrollAxis::Vertical, kScrollbarWidth);
    }

    // Clicking a row selects it. Read after the rows are built, so the tags the
    // click is matched against are this frame's.
    for (std::size_t row = result.shown.first; row < result.shown.last(); ++row) {
        if (!input.clicked(std::string(id) + ".row." + std::to_string(row))) continue;
        if (state.selected != row) {
            state.selected = row;
            result.selectionChanged = true;
        }
    }
    (void)scope;

    return result;
}

}  // namespace gbui
