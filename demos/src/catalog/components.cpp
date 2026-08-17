// The component group: text, the two surfaces, and the pieces that go on them.
//
// Every example is the smallest honest use of its component — small enough to
// read in one glance, and real enough that copying it into an application
// produces something that works. Nothing here is a showcase of options: the
// options are in the metadata, one line each, and repeating them as a wall of
// arguments would teach the wrong thing about a library whose defaults are
// most of its design.
#include "catalog.hpp"

namespace gbui::demos::catalog {

void addComponentExamples(std::vector<Example>& out) {
    out.push_back({"text", [](Ui& ui, const Interaction&, State&) {
                       text(ui, "themes/nord/theme.json");
                       text(ui, "Muted, and one size down",
                            {.color = Token::TextMuted, .size = 11.0f});
                   }, 90});

    out.push_back({"strong", [](Ui& ui, const Interaction&, State&) {
                       // Semantic, in the sense HTML gives it: importance,
                       // which happens to be drawn as a weight.
                       strong(ui, "3 files changed");
                   }, 70});

    out.push_back(
        {"emphasis", [](Ui& ui, const Interaction&, State&) { emphasis(ui, "detached HEAD"); }, 70});

    out.push_back({"sectionHeading",
                   [](Ui& ui, const Interaction&, State&) { sectionHeading(ui, "UNSTAGED (3)"); }, 70});

    out.push_back(
        {"richText", [](Ui& ui, const Interaction&, State&) {
             // One line, several colours. A node carries one text
             // style, so a sentence that changes colour is spans and
             // not markup — which is what stops this becoming a
             // parser.
             richText(ui, {{.text = "on branch "},
                           {.text = "main", .color = Token::Accent, .weight = FontWeight::SemiBold},
                           {.text = ", 3 files changed"}});
         }, 70});

    out.push_back({"image", [](Ui& ui, const Interaction& input, State& state) {
                       (void)input;
                       const Bitmap picture{state.picture.data(), state.pictureSide,
                                            state.pictureSide, 0};
                       Style row;
                       row.direction = Direction::Row;
                       row.align = Align::Center;
                       row.gap = 14.0f;
                       auto scope = ui.scope(row);
                       // The same pixels three ways: its own size, a wide box it
                       // is fitted inside, and the same box it is cropped to
                       // cover. `Contain` letterboxes and `Cover` crops, which
                       // is the whole of the difference and the only thing worth
                       // showing side by side.
                       image(ui, picture, {.width = 44.0f, .height = 44.0f, .radius = 10.0f});
                       image(ui, picture,
                             {.width = 96.0f,
                              .height = 44.0f,
                              .fit = ImageFit::Contain,
                              .radius = 6.0f,
                              .background = Fill{Token::BgOverlay}});
                       image(ui, picture,
                             {.width = 96.0f,
                              .height = 44.0f,
                              .fit = ImageFit::Cover,
                              .radius = 6.0f});
                       // And none at all, which is what `alt` is for.
                       image(ui, Bitmap{},
                             {.width = 44.0f,
                              .height = 44.0f,
                              .radius = 10.0f,
                              .background = Fill{Token::BgOverlay},
                              .alt = "N/A"});
                       (void)scope;
                   }, 100});

    out.push_back({"avatar",
                   [](Ui& ui, const Interaction&, State&) {
                       auto row = ui.row({.align = Align::Center, .gap = 10.0f});
                       // No picture: the initials and a colour derived from the
                       // name, which is the same on every machine and in every
                       // session because it is a hash rather than a choice.
                       avatar(ui, "Ada Lovelace");
                       avatar(ui, "Grace Hopper", {.size = 34.0f});
                       avatar(ui, "gitbox-bot", {.size = 24.0f, .square = true});
                       text(ui, "Ada Lovelace", {.color = Token::Text});
                       // Beside its own name, so it says nothing: a row that
                       // reads "Ada Lovelace, Ada Lovelace" has said it twice.
                       avatar(ui, "Ada Lovelace", {.size = 20.0f, .decorative = true});
                   },
                   70});

    out.push_back(
        {"chip",
         [](Ui& ui, const Interaction& input, State& state) {
             auto row = ui.row({.align = Align::Center, .gap = 8.0f});
             if (chip(ui, input, "catalog.chip.merged", "Merged",
                      {.selected = state.on, .leading = Icon::GitMerge})
                     .pressed) {
                 state.on = !state.on;
             }
             if (chip(ui, input, "catalog.chip.stale", "Stale", {.selected = state.off}).pressed) {
                 state.off = !state.off;
             }
             // Removable: the × is a second control with a name of
             // its own, and Delete takes it off from the keyboard.
             chip(ui, input, "catalog.chip.branch", "feat/nord-tuning",
                  {.removable = true, .leading = Icon::GitBranch});
         },
         60});

    out.push_back({"kbd",
                   [](Ui& ui, const Interaction&, State&) {
                       auto column = ui.column({.gap = 10.0f});
                       {
                           auto row = ui.row({.align = Align::Center, .gap = 8.0f});
                           text(ui, "Command palette", {.color = Token::Text, .grow = 1.0f});
                           kbd(ui, "Ctrl+Shift+P");
                       }
                       {
                           auto row = ui.row({.align = Align::Center, .gap = 8.0f});
                           text(ui, "Stage everything", {.color = Token::Text, .grow = 1.0f});
                           kbd(ui, "Ctrl + A");
                       }
                   },
                   80});

    out.push_back({"segmented",
                   [](Ui& ui, const Interaction& input, State& state) {
                       static const std::vector<Segment> layouts = {
                           {.label = "Unified"}, {.label = "Split"}, {.label = "Ribbon"}};
                       if (const auto picked = segmented(ui, input, "catalog.segmented", layouts,
                                                         state.segment, {.name = "Diff layout"})) {
                           state.segment = *picked;
                       }
                       // Icon-only, and named — "1m" is a word to a reader who
                       // can see the chart beside it and nothing to one who
                       // cannot.
                       static const std::vector<Segment> ranges = {
                           {.icon = Icon::TrendingUp, .name = "Rising"},
                           {.icon = Icon::TrendingDown, .name = "Falling"}};
                       (void)segmented(ui, input, "catalog.segmented.icons", ranges, 0,
                                       {.name = "Direction"});
                   },
                   120});

    out.push_back(
        {"breadcrumbs",
         [](Ui& ui, const Interaction& input, State& state) {
             static const std::vector<Crumb> trail = {
                 {.label = "gbui", .icon = Icon::Folder},
                 {.label = "src"},
                 {.label = "widgets"},
                 {.label = "treeView.cpp", .icon = Icon::File}};
             if (const auto hit = breadcrumbs(ui, input, "catalog.crumbs", trail).chosen) {
                 state.crumb = *hit;
             }
             // The same trail with no room for it: the *middle* goes,
             // because the ends are the two a reader needs — where
             // they are, and the way home.
             (void)breadcrumbs(ui, input, "catalog.crumbs.tight", trail, {.maxVisible = 3});
         },
         110});

    out.push_back({"pagination",
                   [](Ui& ui, const Interaction& input, State& state) {
                       if (const auto page =
                               pagination(ui, input, "catalog.pages", state.page, 20).chosen) {
                           state.page = *page;
                       }
                       text(ui, "Page " + std::to_string(state.page + 1) + " of 20",
                            {.color = Token::TextMuted, .size = 11.5f});
                   },
                   100});

    out.push_back({"badge", [](Ui& ui, const Interaction&, State&) {
                       // A row, because the stage stretches its children and a
                       // badge that spans the panel is not a badge.
                       auto row = ui.row({.align = Align::Center, .gap = 8.0f});
                       badge(ui, "M");
                       badge(ui, "Enterprise",
                             {.background = Token::Accent, .foreground = Token::AccentFg});
                   }, 70});

    out.push_back({"icon", [](Ui& ui, const Interaction&, State&) {
                       auto row = ui.row({.align = Align::Center, .gap = 10.0f});
                       icon(ui, Icon::GitBranch);
                       icon(ui, Icon::CircleAlert, {.color = Token::Removed, .size = 20.0f});
                   }, 70});

    out.push_back({"button", [](Ui& ui, const Interaction& input, State&) {
                       auto row = ui.row({.align = Align::Center, .gap = 8.0f});
                       button(ui, input, "PUSH",
                              {.variant = ButtonVariant::Primary,
                               .leading = Icon::Upload,
                               .id = "catalog.button.primary"});
                       button(ui, input, "FETCH",
                              {.variant = ButtonVariant::Ghost, .id = "catalog.button.ghost"});
                       button(ui, input, "DISCARD",
                              {.variant = ButtonVariant::Danger, .id = "catalog.button.danger"});
                   }, 80});

    out.push_back({"divider", [](Ui& ui, const Interaction&, State&) {
                       // Told which way the container it sits in runs, because
                       // a hairline in a column is horizontal and one in a row
                       // is not.
                       text(ui, "above");
                       divider(ui, Direction::Column);
                       text(ui, "below");
                   }, 110});

    out.push_back({"spacer", [](Ui& ui, const Interaction&, State&) {
                       auto row = ui.row({.align = Align::Center});
                       text(ui, "left");
                       spacer(ui);
                       text(ui, "right", {.color = Token::TextMuted});
                   }, 70});

    out.push_back({"panel", [](Ui& ui, const Interaction&, State&) {
                       auto panelScope = panel(ui);
                       sectionHeading(ui, "COMMIT MESSAGE");
                       text(ui, "A surface with a border and a radius.",
                            {.color = Token::TextMuted});
                   }, 120});

    out.push_back(
        {"listRow", [](Ui& ui, const Interaction& input, State&) {
             const char* files[] = {"themes/nord/theme.json", "scripts/preview.mjs", "README.md"};
             for (int i = 0; i < 3; ++i) {
                 const std::string tag = "catalog.row." + std::to_string(i);
                 auto row = listRow(
                     ui,
                     {.selected = i == 0, .hovered = input.isHovered(tag), .gap = 8.0f, .id = tag});
                 icon(ui, Icon::File, {.color = Token::TextMuted, .size = 14.0f});
                 text(ui, files[i], {.grow = 1.0f});
             }
         }, 160});

    out.push_back({"toolbar", [](Ui& ui, const Interaction& input, State&) {
                       auto bar = toolbar(ui);
                       text(ui, "gitbox-themes",
                            {.color = Token::TextStrong, .weight = FontWeight::Medium});
                       spacer(ui);
                       button(ui, input, "PULL",
                              {.variant = ButtonVariant::Ghost,
                               .leading = Icon::Download,
                               .id = "catalog.toolbar.pull"});
                   }, 90});
}

}  // namespace gbui::demos::catalog
