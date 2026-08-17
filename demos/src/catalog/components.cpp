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
