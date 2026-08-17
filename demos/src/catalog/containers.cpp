// The containers: the box everything else sits in, the two that clip, and the
// two that only build what is on screen.
#include "catalog.hpp"

namespace gbui::demos::catalog {

void addContainerExamples(std::vector<Example>& out) {
    out.push_back({"accordion",
                   [](Ui& ui, const Interaction& input, State& state) {
                       static const std::vector<AccordionSection> sections = {
                           {.id = "general",
                            .title = "General",
                            .detail = "Startup, updates, language",
                            .icon = Icon::Settings,
                            .body =
                                [](Ui& inner) {
                                    text(inner, "Open on the last repository.",
                                         {.color = Token::TextMuted, .size = 12.0f});
                                }},
                           {.id = "git",
                            .title = "Git",
                            .icon = Icon::GitBranch,
                            .badge = "2",
                            .body =
                                [](Ui& inner) {
                                    text(inner, "Sign every commit; prune on fetch.",
                                         {.color = Token::TextMuted, .size = 12.0f});
                                }},
                           {.id = "advanced", .title = "Advanced", .disabled = true, .body = {}}};
                       const AccordionResult result =
                           accordion(ui, input, "catalog.accordion", sections, state.sections,
                                     {.name = "Settings"});
                       // The caller's half of the focus contract, which the
                       // arrows between headers need.
                       if (result.focus) state.focusRequest = std::string(*result.focus);
                   },
                   240});

    out.push_back({"box", [](Ui& ui, const Interaction&, State&) {
                       // `ui.scope(Style{…})` builds any box the layout engine
                       // can express; `box` is the ergonomics, and the
                       // presets are the boxes an application actually repeats.
                       auto card = box(ui, BoxStyle::card({.gap = 6.0f}));
                       sectionHeading(ui, "A CARD");
                       text(ui, "Padding, a border and a radius.", {.color = Token::TextMuted});
                   }, 120});

    out.push_back({"scrollArea", [](Ui& ui, const Interaction& input, State& state) {
                       {
                           auto view = scrollArea(ui, input, "catalog.scroll", state.scroll,
                                                  {.gap = 2.0f, .height = 120.0f});
                           for (int i = 1; i <= 30; ++i) {
                               text(ui, "row " + std::to_string(i), {.color = Token::TextMuted});
                           }
                       }
                       // The same view with a bar that gets out of the way —
                       // faded to a quarter until the pointer is over it. Take
                       // the pointer off this one and watch the bar go.
                       text(ui, "and one whose bar fades",
                            {.color = Token::TextMuted, .size = 11.0f});
                       auto quiet = scrollArea(
                           ui, input, "catalog.scroll.quiet", state.scrollQuiet,
                           {.scrollbarVisibility = ScrollbarVisibility::WhileUsed,
                            .scrollbarRestOpacity = 0.25f,
                            .gap = 2.0f,
                            .height = 120.0f});
                       for (int i = 1; i <= 30; ++i) {
                           text(ui, "row " + std::to_string(i), {.color = Token::TextMuted});
                       }
                   }, 320});

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
                       auto box = ui.scope(outer);
                       ui.tag("catalog.bar.box");
                       {
                           auto view = scrollArea(ui, input, "catalog.bar", state.scroll2,
                                                  {.scrollbar = false, .gap = 2.0f});
                           for (int i = 1; i <= 30; ++i) {
                               text(ui, "row " + std::to_string(i), {.color = Token::TextMuted});
                           }
                       }
                       const Rect here = input.frameOf("catalog.bar.box");
                       scrollbar(ui, input, "catalog.bar", state.scroll2,
                                 Rect{0.0f, 0.0f, here.width, here.height});
                   }, 190});

    out.push_back({"virtualList", [](Ui& ui, const Interaction& input, State& state) {
                       // Fifty thousand rows, and only the visible ones are
                       // ever built — which is the difference between this and
                       // the scroll view above.
                       virtualList(ui, input, "catalog.virtual", state.listScroll,
                                   {.count = 50000,
                                    .rowHeight = 26.0f,
                                    .height = 140.0f,
                                    .name = "Commits"},
                                   [](Ui& rowUi, std::size_t index) {
                                       auto row = listRow(rowUi, {.height = 26.0f});
                                       text(rowUi, "commit " + std::to_string(index),
                                            {.role = FontRole::Mono, .size = 11.5f});
                                   });
                   }, 190});

    out.push_back({"splitPane", [](Ui& ui, const Interaction& input, State& state) {
                       // The shape every IDE is: a sidebar beside a pane, with
                       // a divider the reader owns the position of.
                       const auto side = [](std::string_view title, Token wash) {
                           return [title, wash](Ui& inner) {
                               Style fill;
                               fill.direction = Direction::Column;
                               fill.width = Length::percent(100);
                               fill.height = Length::percent(100);
                               fill.padding = Edges::all(10.0f);
                               fill.gap = 6.0f;
                               fill.background = Fill{wash};
                               auto scope = inner.scope(fill);
                               sectionHeading(inner, title);
                               text(inner, "Drag the divider, or Tab to it and use the arrows.",
                                    {.color = Token::TextMuted, .size = 11.5f,
                                     .overflow = TextOverflow::Wrap});
                               (void)scope;
                           };
                       };
                       const SplitPaneResult result =
                           splitPane(ui, input, "catalog.split", state.split,
                                     side("SIDEBAR", Token::Bg),
                                     side("EDITOR", Token::BgElevated),
                                     {.minLeading = 90.0f,
                                      .minTrailing = 120.0f,
                                      .name = "Sidebar width",
                                      .leadingLabel = "Sidebar",
                                      .trailingLabel = "Editor",
                                      .height = 150.0f});
                       if (result.changed) state.split = result.position;
                   }, 200});

    out.push_back({"treeView", [](Ui& ui, const Interaction& input, State& state) {
                       // Flat, in pre-order, with a depth on each row — which is
                       // what a `git ls-tree` walk already hands you.
                       static const std::vector<TreeItem> files = {
                           {.id = "src", .label = "src", .hasChildren = true,
                            .icon = Icon::Folder},
                           {.id = "src/widgets", .label = "widgets", .depth = 1,
                            .hasChildren = true, .icon = Icon::Folder},
                           {.id = "src/widgets/treeView.cpp", .label = "treeView.cpp",
                            .depth = 2, .icon = Icon::File, .detail = "9.1k"},
                           {.id = "src/widgets/toast.cpp", .label = "toast.cpp", .depth = 2,
                            .icon = Icon::File, .detail = "11k"},
                           {.id = "src/scene", .label = "scene", .depth = 1,
                            .hasChildren = true, .icon = Icon::Folder},
                           {.id = "src/scene/ui.cpp", .label = "ui.cpp", .depth = 2,
                            .icon = Icon::File, .detail = "4.8k"},
                           {.id = "README.md", .label = "README.md", .icon = Icon::File,
                            .detail = "6.2k"},
                       };
                       // The first frame opens the two folders, so the preview
                       // shows a hierarchy rather than two closed rows.
                       if (state.tree.expanded.empty()) {
                           state.tree.expanded.emplace("src");
                           state.tree.expanded.emplace("src/widgets");
                           state.tree.selected = "src/widgets/toast.cpp";
                       }
                       treeView(ui, input, "catalog.tree", files, state.tree,
                                {.name = "Files", .height = 160.0f});
                   }, 210});

    out.push_back({"gallery", [](Ui& ui, const Interaction& input, State& state) {
                       static const char* captions[] = {
                           "Ridge line, first light", "The long approach",
                           "Above the inversion", "Down the east face"};
                       std::vector<GalleryItem> items;
                       items.reserve(state.plates.size());
                       for (std::size_t i = 0; i < state.plates.size(); ++i) {
                           items.push_back({.image = Bitmap{state.plates[i].data(),
                                                            state.plateSide, state.plateSide, 0},
                                            .caption = captions[i],
                                            .alt = captions[i]});
                       }
                       gallery(ui, input, "catalog.gallery", items, state.gallery,
                               {.thumbnailSize = 44.0f,
                                .loop = true,
                                .name = "Site survey",
                                .height = 150.0f,
                                .grow = 1.0f});
                   }, 290});

    out.push_back({"carousel", [](Ui& ui, const Interaction& input, State& state) {
                       // Two and a half across, so the strip says there is more
                       // without spending a control on saying it — and on a
                       // clock, which is what the pause button is for.
                       static const Token washes[] = {Token::Graph1, Token::Graph2,
                                                      Token::Graph3, Token::Graph4,
                                                      Token::Graph5, Token::Graph6};
                       carousel(ui, input, "catalog.carousel", 6, state.carousel, state.delta,
                                [](Ui& inner, std::size_t index) {
                                    Style card;
                                    card.width = Length::percent(100);
                                    card.height = Length::percent(100);
                                    card.justify = Justify::Center;
                                    card.align = Align::Center;
                                    card.radius = 8.0f;
                                    card.background = Fill{washes[index]};
                                    auto scope = inner.scope(card);
                                    text(inner, "SLIDE " + std::to_string(index + 1),
                                         {.color = Token::AccentFg,
                                          .weight = FontWeight::SemiBold});
                                    (void)scope;
                                },
                                {.slidesPerPage = 2.5f,
                                 .loop = true,
                                 .autoplay = 2.5,
                                 .name = "Screenshots",
                                 .height = 110.0f,
                                 .grow = 1.0f});
                   }, 190});

    out.push_back({"compare", [](Ui& ui, const Interaction& input, State& state) {
                       // Two flat washes rather than two photographs: the
                       // catalogue decodes no images, and the point of the
                       // example is the seam and the handle.
                       const auto wash = [](Token colour, std::string_view caption) {
                           return [colour, caption](Ui& inner) {
                               Style fill;
                               fill.width = Length::percent(100);
                               fill.height = Length::percent(100);
                               fill.justify = Justify::Center;
                               fill.align = Align::Center;
                               fill.background = Fill{colour};
                               auto scope = inner.scope(fill);
                               text(inner, caption,
                                    {.color = Token::AccentFg, .weight = FontWeight::SemiBold});
                               (void)scope;
                           };
                       };
                       const CompareResult result =
                           compare(ui, input, "catalog.compare", state.seam,
                                   wash(Token::Graph3, "BEFORE"), wash(Token::Graph1, "AFTER"),
                                   {.name = "Before and after",
                                    .beforeLabel = "Before",
                                    .afterLabel = "After",
                                    .height = 150.0f,
                                    .grow = 1.0f});
                       if (result.changed) state.seam = result.position;
                   }, 200});

    out.push_back({"marquee", [](Ui& ui, const Interaction& input, State& state) {
                       // Zero delta stops it: held while the pointer is over
                       // it, the reader can read a name instead of chasing it.
                       const bool held = input.isHovered("catalog.marquee");
                       Style bar;
                       bar.direction = Direction::Row;
                       bar.align = Align::Center;
                       bar.height = 34.0f;
                       bar.background = Fill{Token::BgOverlay, 0.6f};
                       bar.radius = 6.0f;
                       auto scope = ui.scope(bar);
                       (void)marquee(ui, input, "catalog.marquee", state.marquee,
                               held ? 0.0f : state.delta, [](Ui& lane) {
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
                               auto chipScope = lane.scope(chip);
                               text(lane, word,
                                    {.color = Token::Text, .role = FontRole::Mono, .size = 11.0f});
                               (void)chipScope;
                           }
                       },
                       {.name = "Market"});
                       (void)scope;
                   }, 90});

    out.push_back({"tabs", [](Ui& ui, const Interaction& input, State& state) {
                       const std::vector<TabItem> pages = {
                           {.label = "History"}, {.label = "Changes"}, {.label = "Stashes"}};
                       if (const auto chosen = tabs(ui, input, "catalog.tabs", pages, state.tab)) {
                           state.tab = *chosen;
                       }
                   }, 90});

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
                   }, 130});

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
                   {.height = 160.0f, .name = "Recent commits"});
         }, 210});
}

}  // namespace gbui::demos::catalog
