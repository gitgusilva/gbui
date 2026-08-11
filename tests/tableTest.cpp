// Column widths, and what happens when they no longer fit.
#include "gbui/widgets/table.hpp"

#include <string>

#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/text.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

struct Sheet {
    TableState state;
    TableResult result;
    Rect across{};
};

/** Builds a table three times, so the widths settle against last frame's
 *  geometry the way they do in a real loop. */
Sheet run(const std::vector<Column>& columns, const std::vector<float>& widths,
          float tableWidth = 400.0f) {
    Theme theme = Theme::dark();
    Interaction input;
    Sheet sheet;
    sheet.state.widths = widths;

    for (int pass = 0; pass < 3; ++pass) {
        Arena arena;
        Ui ui(arena);
        {
            auto root = ui.beginColumn({.width = tableWidth, .height = 300.0f});
            sheet.result = table(ui, input, "t", columns, 12, sheet.state,
                                 [](Ui& cellUi, std::size_t row, std::size_t column) {
                                     text(cellUi, std::to_string(row) + ":" +
                                                      std::to_string(column));
                                 },
                                 {.grow = 1.0f});
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, tableWidth, 300}, context);
        input.update(arena, ui.root(), InputFrame{});
        sheet.across = input.frameOf("t.across");
    }
    return sheet;
}

float total(const std::vector<float>& widths) {
    float sum = 0.0f;
    for (const float w : widths) sum += w;
    return sum;
}

}  // namespace

TEST("columns that fit share the width and do not scroll sideways") {
    const std::vector<Column> columns{
        {.title = "A", .sizing = ColumnSize::Fraction, .width = 1.0f},
        {.title = "B", .sizing = ColumnSize::Fraction, .width = 1.0f},
    };
    const Sheet sheet = run(columns, {});
    CHECK(total(sheet.result.columnWidths) <= 400.0f + 1.0f);
    CHECK(!sheet.state.columns.scrollable());
}

/**
 * The bug this pins: dragging a divider wider than the table put the far
 * columns somewhere with no way to reach them. The widths were honoured and
 * the overflow was simply clipped.
 */
TEST("columns dragged wider than the table can be scrolled to") {
    const std::vector<Column> columns{
        {.title = "A", .sizing = ColumnSize::Fraction, .width = 1.0f},
        {.title = "B", .sizing = ColumnSize::Fraction, .width = 1.0f},
        {.title = "C", .sizing = ColumnSize::Fraction, .width = 1.0f},
    };
    // Each pinned far wider than the 400px table.
    const Sheet sheet = run(columns, {300.0f, 300.0f, 300.0f});

    CHECK_NEAR(total(sheet.result.columnWidths), 900.0f);
    CHECK(sheet.state.columns.contentSize > sheet.state.columns.viewportSize);
    CHECK(sheet.state.columns.scrollable());
    // And there is real distance to travel, not a rounding error.
    CHECK(sheet.state.columns.maxOffset() > 400.0f);
}

TEST("a fixed column keeps its width whatever the table's is") {
    const std::vector<Column> columns{
        {.title = "A", .sizing = ColumnSize::Fixed, .width = 120.0f},
        {.title = "B", .sizing = ColumnSize::Fraction, .width = 1.0f},
    };
    const Sheet wide = run(columns, {}, 600.0f);
    const Sheet narrow = run(columns, {}, 300.0f);
    CHECK_NEAR(wide.result.columnWidths[0], 120.0f);
    CHECK_NEAR(narrow.result.columnWidths[0], 120.0f);
    // Only the fraction column absorbs the difference.
    CHECK(wide.result.columnWidths[1] > narrow.result.columnWidths[1]);
}

TEST("a column never goes below its minimum") {
    const std::vector<Column> columns{
        {.title = "A", .sizing = ColumnSize::Fraction, .width = 1.0f, .minWidth = 150.0f},
        {.title = "B", .sizing = ColumnSize::Fraction, .width = 1.0f, .minWidth = 150.0f},
    };
    // Two 150px minimums in a 200px table: they overflow rather than shrink,
    // which is what the sideways scroll is now there to make reachable.
    const Sheet sheet = run(columns, {}, 200.0f);
    CHECK(sheet.result.columnWidths[0] >= 150.0f);
    CHECK(sheet.result.columnWidths[1] >= 150.0f);
    CHECK(sheet.state.columns.scrollable());
}
