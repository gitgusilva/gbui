// The containers: the box everything else sits in, the two that clip, and the
// two that only build what is on screen.
#include "catalog.hpp"

namespace gbui::demos::catalog {

void addContainerExamples(std::vector<Example>& out) {
    out.push_back({"beginBox", [](Ui& ui, const Interaction&, State&) {
                       // `ui.begin(Style{…})` builds any box the layout engine
                       // can express; `beginBox` is the ergonomics, and the
                       // presets are the boxes an application actually repeats.
                       auto card = beginBox(ui, BoxStyle::card({.gap = 6.0f}));
                       sectionHeading(ui, "A CARD");
                       text(ui, "Padding, a border and a radius.", {.color = Token::TextMuted});
                   }});

    out.push_back({"beginScroll", [](Ui& ui, const Interaction& input, State& state) {
                       auto view = beginScroll(ui, input, "catalog.scroll", state.scroll,
                                               {.gap = 2.0f, .height = 140.0f});
                       for (int i = 1; i <= 30; ++i) {
                           text(ui, "row " + std::to_string(i), {.color = Token::TextMuted});
                       }
                   }});

    out.push_back({"scrollbar", [](Ui& ui, const Interaction& input, State& state) {
                       // A bar drawn somewhere other than the view it drives.
                       // The view is narrow and the bar is against the *outer*
                       // box, which is the shape a table needs: rows that
                       // scroll up and down inside a box that scrolls sideways
                       // would otherwise put their bar out at the end of the
                       // widest column, where nobody can reach it.
                       Style outer;
                       outer.height = 140.0f;
                       outer.padding = Edges::symmetric(0.0f, 10.0f);
                       outer.background = Fill{Token::BgOverlay, 0.4f};
                       outer.radius = 6.0f;
                       auto box = ui.begin(outer);
                       ui.tag("catalog.bar.box");
                       {
                           auto view = beginScroll(ui, input, "catalog.bar", state.scroll2,
                                                   {.scrollbar = false, .gap = 2.0f});
                           for (int i = 1; i <= 30; ++i) {
                               text(ui, "row " + std::to_string(i), {.color = Token::TextMuted});
                           }
                       }
                       const Rect here = input.frameOf("catalog.bar.box");
                       scrollbar(ui, input, "catalog.bar", state.scroll2,
                                 Rect{0.0f, 0.0f, here.width, here.height});
                   }});

    out.push_back({"virtualList", [](Ui& ui, const Interaction& input, State& state) {
                       // Fifty thousand rows, and only the visible ones are
                       // ever built — which is the difference between this and
                       // the scroll view above.
                       virtualList(ui, input, "catalog.virtual", state.listScroll,
                                   {.count = 50000, .rowHeight = 26.0f, .height = 140.0f},
                                   [](Ui& rowUi, std::size_t index) {
                                       auto row = beginListRow(rowUi, {.height = 26.0f});
                                       text(rowUi, "commit " + std::to_string(index),
                                            {.role = FontRole::Mono, .size = 11.5f});
                                   });
                   }});

    out.push_back({"marquee", [](Ui& ui, const Interaction& input, State& state) {
                       // The clock is the caller's, which is what lets the
                       // strip stop: held while the pointer is over it, the
                       // reader can read a name instead of chasing it.
                       if (!input.isHovered("catalog.marquee")) state.marquee = state.clock;
                       Style bar;
                       bar.direction = Direction::Row;
                       bar.align = Align::Center;
                       bar.height = 34.0f;
                       bar.background = Fill{Token::BgOverlay, 0.6f};
                       bar.radius = 6.0f;
                       auto scope = ui.begin(bar);
                       marquee(ui, input, "catalog.marquee", state.marquee, [](Ui& lane) {
                           for (const std::string_view word :
                                {"ALPHA 12.40", "BRAVO 8.15", "CHARLIE 61.02", "DELTA 3.77",
                                 "ECHO 45.60"}) {
                               Style chip;
                               chip.direction = Direction::Row;
                               chip.align = Align::Center;
                               chip.height = 22.0f;
                               chip.shrink = 0.0f;
                               chip.padding = Edges::symmetric(0.0f, 10.0f);
                               chip.margin = Edges::symmetric(0.0f, 5.0f);
                               chip.radius = 11.0f;
                               chip.background = Fill{Token::BgElevated};
                               auto chipScope = lane.begin(chip);
                               text(lane, word,
                                    {.color = Token::Text, .role = FontRole::Mono, .size = 11.0f});
                               (void)chipScope;
                           }
                       });
                       (void)scope;
                   }});

    out.push_back({"tabs", [](Ui& ui, const Interaction& input, State& state) {
                       const std::vector<TabItem> pages = {
                           {.label = "History"}, {.label = "Changes"}, {.label = "Stashes"}};
                       if (const auto chosen = tabs(ui, input, "catalog.tabs", pages, state.tab)) {
                           state.tab = *chosen;
                       }
                   }});

    out.push_back({"tabPanels", [](Ui& ui, const Interaction& input, State& state) {
                       const std::vector<TabItem> pages = {{.label = "One"}, {.label = "Two"}};
                       if (const auto chosen =
                               tabs(ui, input, "catalog.panels.tabs", pages, state.tab)) {
                           state.tab = *chosen;
                       }
                       // Separate from `tabs` because the two are rarely
                       // siblings: a vertical strip sits beside its panels and
                       // a horizontal one above them.
                       tabPanels(ui, state.tab,
                                 {[](Ui& panel) { text(panel, "The first panel"); },
                                  [](Ui& panel) { text(panel, "The second panel"); }});
                   }});

    out.push_back(
        {"table", [](Ui& ui, const Interaction& input, State& state) {
             struct Row {
                 const char* name;
                 const char* hash;
                 int files;
             };
             static constexpr Row rows[] = {
                 {"warm up the hover surface", "a41f9c2", 3},
                 {"add Arrakis Dark", "7be0114", 12},
                 {"keep headless Chrome alive", "0c39da8", 1},
                 {"shrink the README", "d5517ae", 2},
             };
             const std::vector<Column> columns = {
                 {.title = "Subject", .width = 2.0f, .sortable = true},
                 {.title = "Commit",
                  .sizing = ColumnSize::FitContent,
                  .fitSample = "0000000",
                  .fitStyle = {.role = FontRole::Mono}},
                 {.title = "Files",
                  .sizing = ColumnSize::FitContent,
                  .fitSample = "999",
                  .fitStyle = {.role = FontRole::Mono},
                  .align = TextAlign::End},
             };
             table(ui, input, "catalog.table", columns, 4, state.table,
                   [&](Ui& cellUi, std::size_t row, std::size_t column) {
                       const Row& r = rows[row];
                       if (column == 0) {
                           text(cellUi, r.name, {.grow = 1.0f});
                       } else if (column == 1) {
                           text(cellUi, r.hash,
                                {.color = Token::TextMuted, .role = FontRole::Mono});
                       } else {
                           text(cellUi, std::to_string(r.files),
                                {.role = FontRole::Mono, .align = TextAlign::End, .grow = 1.0f});
                       }
                   },
                   {.height = 160.0f});
         }});
}

}  // namespace gbui::demos::catalog
