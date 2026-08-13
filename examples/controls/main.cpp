// gbui_controls — every interactive component, in a window you can use.
//
//     gbui_controls                  open it
//     gbui_controls --shot out.ppm   render one frame and exit
//     gbui_controls --shot out.ppm --tab 4   …after four presses of Tab
//
// Tab and Shift+Tab move the keyboard between controls; Space and Return
// activate the focused one; the arrow keys drive sliders and number fields.
//
// It doubles as the proof that components hold no state: every value below
// lives in `Model`, and each component is handed the value and hands back what
// the user did with it.

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/paint/canvas.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/platform/font.hpp"
#include "gbui/platform/window.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/controls.hpp"
#include "gbui/widgets/containers.hpp"
#include "gbui/widgets/overlays.hpp"

using namespace gbui;

namespace {

/** A contributor, which is what the table shows. */
struct Contributor {
    std::string name;
    int commits;
    int added;
    int removed;
    std::string lastSeen;
};

/** Everything the screen shows. The toolkit owns none of it. */
struct Model {
    bool showTags = true;
    bool showRemotes = false;
    bool wrapLines = false;
    int mergeStyle = 0;          // 0 merge, 1 rebase, 2 fast-forward
    bool autoFetch = true;
    TextEditState name{"gitbox-themes", 13, 13};
    TextEditState token{"hunter2-secret", 14, 14};
    bool tokenRevealed = false;
    double fetchMinutes = 5.0;
    double fontSize = 20.0;
    double contrast = 0.65;
    double stepped = 0.5;
    std::size_t tab = 0;
    std::size_t design = 0;   // which design system the screen is wearing
    std::vector<std::string> designNames{"GitBox", "Material", "Cupertino", "Fluent"};
    SelectState designList;
    /** Index 0 keeps whatever the design asked for; the rest override it, which
     *  is what `Typography::uiFont` is for. */
    std::vector<std::string> fontNames{"Design default", "Adwaita Sans", "Noto Sans",
                                       "DejaVu Sans", "Cantarell", "Liberation Sans",
                                       "Carlito", "Arial"};
    std::size_t fontFamily = 0;
    SelectState fontList;
    bool lightMode = false;
    float clock = 0.0f;          // drives the indeterminate bar

    std::vector<std::string> branches{"main", "feat/nord-tuning", "feat/catppuccin",
                                      "fix/preview-generation", "release/1.3.2"};
    std::optional<std::size_t> branch = 0;
    SelectState branchList;
    bool confirmOpen = false;
    Vec2 confirmAt{};
    ScrollState log;

    // The virtualised history: a scroll position, a selection, and the slice
    // the list reported building last frame. No rows — there is no list to
    // hold, only a count.
    /** A rolling window of how long each frame took, in milliseconds. */
    /** How long the loop took end to end — with vsync on this is the display's
     *  refresh interval, and it says more about the monitor than about us. */
    std::vector<double> frameTimes = std::vector<double>(90, 0.0);
    /** How long *building, laying out and rasterising* took. This is the
     *  toolkit's own cost and the only one it can do anything about. */
    std::vector<double> drawTimes = std::vector<double>(90, 0.0);

    ColorPickerState colour{Hsv{212.0f, 0.84f, 0.92f, 1.0f}};
    ColorPickerState tint{Hsv{150.0f, 0.55f, 0.80f, 1.0f}};
    ColorPickerState projectColour{Hsv{262.0f, 0.62f, 0.90f, 1.0f}};
    Date date = Date::today();
    DatePickerState dateView;
    Date due{};              // deliberately unset: the field shows its placeholder
    DatePickerState dueView;
    DateTime when{Date::today(), Time{14, 30, 0}};
    DateTimePickerState whenView;

    DonutState share;
    TableState table;
    /** The frame-time plot is frozen by default: a chart that redraws every
     *  frame to show how long redrawing takes is measuring itself measuring
     *  itself, and it animates the page for a reader who came to read it. */
    bool fpsPaused = true;
    DateTime scheduled{{2026, 8, 11}, {14, 30, 0}};
    DateTimeFieldState scheduledField{};
    ChartView churnView{};
    float motionDuration = 0.32f;
    std::size_t motionCurve = 2;   // ease-out
    bool motionPlaying = false;
    bool stackBars = false;
    bool lollipops = false;
    bool tableRowLines = true;
    bool tableColumnLines = true;
    RichEditorState docState;
    RichDocument doc{{
        {BlockType::Heading1, "Release notes", {}},
        {BlockType::Paragraph,
         "This release rewrites the path rasteriser and adds a table.",
         {{13, 21, Mark::Bold, ""}, {53, 58, Mark::Code, ""}}},
        {BlockType::Heading3, "What changed", {}},
        {BlockType::Bullet, "Filled paths are drawn by scanline", {{23, 34, Mark::Italic, ""}}},
        {BlockType::Bullet, "Strokes accumulate coverage per row", {}},
        {BlockType::Numbered, "Measure before optimising", {}},
        {BlockType::Numbered, "Prove the drawing did not change", {{0, 5, Mark::Underline, ""}}},
        {BlockType::Quote, "A donut wedge is two arcs and two lines.", {}},
        {BlockType::Code, "canvas.drawPath(path, paint, 1.5f, clip);", {}},
        {BlockType::Paragraph, "See the guide for what is still missing.",
         {{4, 13, Mark::Hyperlink, "https://example.com"}}},
    }};
    std::vector<Contributor> contributors{
        {"hermes.santos", 1284, 48210, 19004, "2 hours ago"},
        {"gustavo.will", 976, 39118, 12877, "yesterday"},
        {"ana.ribeiro", 612, 21440, 8032, "3 days ago"},
        {"caio.mendes", 388, 14002, 5511, "last week"},
        {"marina.lopes", 274, 9880, 3120, "last week"},
        {"pedro.alves", 191, 7233, 2410, "2 weeks ago"},
        {"lu.ferreira", 143, 5010, 1880, "3 weeks ago"},
        {"rita.gomes", 97, 3402, 1204, "last month"},
        {"dev-ci-bot", 4820, 102300, 98110, "10 minutes ago"},
        {"release-bot", 512, 18400, 17990, "1 hour ago"},
        {"tiago.nunes", 88, 2910, 990, "last month"},
        {"bea.castro", 61, 1980, 720, "2 months ago"},
    };
    ScrollState page;
    ScrollState history;
    std::size_t commit = 0;
    VirtualSlice shown{};
};

/** The design systems the switcher offers. Demo data: the library ships the
 *  `Design` presets and the palettes, and this only decides which to show. */
struct DesignChoice {
    const char* label;
    Design (*design)();
    Theme (*theme)(bool darkMode);
};

constexpr DesignChoice kDesigns[] = {
    {"GITBOX", &Design::gitbox, nullptr},
    {"MATERIAL", &Design::material, &Theme::material},
    {"CUPERTINO", &Design::cupertino, &Theme::cupertino},
    {"FLUENT", &Design::fluent, &Theme::fluent},
};
constexpr std::size_t kDesignCount = sizeof(kDesigns) / sizeof(kDesigns[0]);

/** A history nobody stores: fifty thousand commits generated from an index. */
constexpr std::size_t kCommits = 50000;
constexpr float kCommitRowHeight = 22.0f;

VirtualListOptions historyOptions() {
    return {.count = kCommits,
            .rowHeight = kCommitRowHeight,
            .padding = Edges::symmetric(4.0f, 0.0f),
            .step = kCommitRowHeight * 3.0f};
}

std::string commitTag(std::size_t index) { return "controls.history." + std::to_string(index); }

std::string commitSubject(std::size_t index) {
    static const char* kVerbs[] = {"feat(themes): warm up", "fix(layout): stop clipping",
                                   "refactor(paint): fold", "docs(guide): explain",
                                   "test(input): cover"};
    static const char* kNouns[] = {"the hover surface", "the diff pane", "the glyph cache",
                                   "focus traversal", "the scroll offset"};
    return std::string(kVerbs[index % 5]) + " " + kNouns[(index / 5) % 5] + " (#" +
           std::to_string(kCommits - index) + ")";
}

std::string commitHash(std::size_t index) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%07zx",
                  (index * 2654435761u + 0x9e3779b9u) & 0xfffffff);
    return buffer;
}

/**
 * A panel's heading, and where in the library it comes from.
 *
 * The header path rather than a namespace, because that is what is true:
 * everything a caller uses here is in `gbui`, and the module a thing belongs to
 * is recorded by which header declares it. Printing `gbui::chart` would name
 * something that does not exist.
 */
void sectionTitle(Ui& ui, std::string_view title, std::string_view module = {}) {
    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 10.0f;
    auto scope = ui.begin(row);
    text(ui, title, {.color = Token::TextStrong, .weight = FontWeight::SemiBold});
    if (!module.empty()) {
        Style chip;
        chip.padding = Edges::symmetric(1.0f, 6.0f);
        chip.radius = 4.0f;
        chip.shrink = 0.0f;
        chip.background = Fill{Token::BgElevated};
        chip.border = Border{1.0f, Fill{Token::Border}};
        auto chipScope = ui.begin(chip);
        text(ui, module,
             {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.0f});
        (void)chipScope;
    }
    (void)scope;
}

/** A labelled row: the name on the left, the control on the right. */
Ui::Scope beginField(Ui& ui, std::string_view label) {
    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 12.0f;
    // A floor, so the row grows when the type does.
    row.minHeight = 34.0f;
    auto scope = ui.begin(row);
    {
        Style name;
        name.width = 130.0f;
        name.shrink = 0.0f;
        auto nameScope = ui.begin(name);
        text(ui, label, {.color = Token::TextMuted, .size = 12.0f});
        (void)nameScope;
    }
    return scope;
}

// Each tab is a function, so the strip below reads as a table of contents and
// a panel can be found without counting braces.

void togglesTab(Ui& ui, const Interaction& input, Model& model) {
    auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 14.0f});
    sectionTitle(ui, "Checkbox, radio, switch", "widgets/checkbox · radio · switchToggle");

    if (checkbox(ui, input, "controls.tags", model.showTags, {.label = "Show tags in graph"})) {
        model.showTags = !model.showTags;
    }
    if (checkbox(ui, input, "controls.remotes", model.showRemotes,
                 {.label = "Show remote branches"})) {
        model.showRemotes = !model.showRemotes;
    }
    if (checkbox(ui, input, "controls.wrap", model.wrapLines,
                 {.disabled = true, .label = "Wrap long lines (disabled)"})) {
        model.wrapLines = !model.wrapLines;
    }

    divider(ui, Direction::Column);

    const char* styles[] = {"Merge commit", "Rebase", "Fast-forward only"};
    for (int i = 0; i < 3; ++i) {
        const std::string id = "controls.merge." + std::to_string(i);
        if (radio(ui, input, id, model.mergeStyle == i, {.label = styles[i]})) {
            model.mergeStyle = i;
        }
    }

    divider(ui, Direction::Column);

    if (switchToggle(ui, input, "controls.autofetch", model.autoFetch,
                     {.label = "Fetch automatically"})) {
        model.autoFetch = !model.autoFetch;
    }
    // A tooltip sits beside the control it describes and draws nothing until
    // that control is hovered, so the call is unconditional.
    tooltip(ui, input, "controls.autofetch",
            "Fetches every few minutes while the window is open.");
    if (switchToggle(ui, input, "controls.autofetch.off", false,
                     {.disabled = true, .label = "Disabled switch"})) {
    }
    (void)panel;
}

void textTab(Ui& ui, const Interaction& input, Model& model) {
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Fields", "widgets/textField");
        {
            auto field = beginField(ui, "Repository");
            textField(ui, input, "controls.name", model.name,
                      {.placeholder = "name", .leading = Icon::Folder, .grow = 1.0f});
            (void)field;
        }
        {
            auto field = beginField(ui, "Access token");
            // The eye is on by default; the model owns whether it is showing.
            const TextFieldResult result =
                textField(ui, input, "controls.token", model.token,
                          {.placeholder = "paste a token", .password = true,
                           .revealed = model.tokenRevealed, .grow = 1.0f});
            if (result.toggledReveal) model.tokenRevealed = !model.tokenRevealed;
            (void)field;
        }
        {
            auto field = beginField(ui, "Branch");
            const SelectResult result =
                select(ui, input, "controls.branch", model.branches, model.branch,
                       model.branchList, {.grow = 1.0f});
            if (result.chosen) model.branch = result.chosen;
            (void)field;
        }
        {
            auto field = beginField(ui, "Read-only");
            TextEditState readOnly{"origin/main", 0, 0};
            textField(ui, input, "controls.readonly", readOnly, {.readOnly = true, .grow = 1.0f});
            (void)field;
        }
        {
            auto field = beginField(ui, "Disabled");
            TextEditState off{"", 0, 0};
            textField(ui, input, "controls.disabledfield", off,
                      {.placeholder = "not editable", .disabled = true, .grow = 1.0f});
            (void)field;
        }
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 12.0f});
        sectionTitle(ui, "Weights and wrapping", "widgets/text · layout/textWrap");
        {
            auto row = ui.beginRow({.align = Align::Center, .gap = 12.0f});
            text(ui, "Regular");
            text(ui, "Medium", {.weight = FontWeight::Medium});
            gbui::strong(ui, "Strong");
            text(ui, "Bold", {.weight = FontWeight::Bold});
            gbui::emphasis(ui, "Emphasis");
            text(ui, "Bold italic", {.weight = FontWeight::Bold, .slant = FontSlant::Italic});
            (void)row;
        }
        // One line, several colours: a row of runs rather than a string with
        // markup in it.
        richText(ui,
                 {
                         {.text = "on branch "},
                         {.text = "main", .color = Token::Accent,
                          .weight = FontWeight::SemiBold},
                         {.text = " — "},
                         {.text = "3 added", .color = Token::Added},
                         {.text = ", "},
                         {.text = "1 removed", .color = Token::Removed,
                          .strikeThrough = true},
                         {.text = ", see "},
                         {.text = "the log", .color = Token::Accent, .underline = true},
                 },
                 // Wrapping between spans, now that the engine can: narrow the
                 // window and the phrase reflows instead of running off.
                 {.wrap = true});
        // A gradient across a whole run, which is the other half of the answer:
        // per-span colour above, and a sweep within one span here.
        text(ui, "a heading that fades across itself",
             {.weight = FontWeight::SemiBold, .size = 15.0f,
              .gradient = Gradient::linear(Fill{Token::Accent}, Fill{Token::TextMuted}, 90.0f)});

        // A paragraph that wraps to whatever width the window leaves it, and a
        // second clamped to two lines so the ellipsis is visible beside it.
        text(ui,
             "A run set to wrap fills the width it is given and takes as many lines as it "
             "needs; narrow the window and this paragraph reflows rather than eliding.",
             {.color = Token::TextMuted, .size = 12.0f, .overflow = TextOverflow::Wrap,
              .lineHeight = 1.5f});
        text(ui,
             "The same run clamped to two lines: maxLines cuts it and ends the last line "
             "with an ellipsis, which is what CSS calls line-clamp and what a commit body "
             "in a narrow pane wants.",
             {.color = Token::TextMuted, .size = 12.0f, .overflow = TextOverflow::Wrap,
              .maxLines = 2, .lineHeight = 1.5f});
        (void)panel;
    }
}

void numbersTab(Ui& ui, const Interaction& input, Model& model) {
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Numbers and ranges", "widgets/numberField · slider");
        {
            auto field = beginField(ui, "Auto-fetch");
            const auto result = numberField(ui, input, "controls.minutes", model.fetchMinutes,
                                            {.minimum = 0, .maximum = 60, .step = 1,
                                             .suffix = " min"});
            if (result.changed) model.fetchMinutes = result.value;
            (void)field;
        }
        {
            auto field = beginField(ui, "Contrast");
            const auto result = slider(ui, input, "controls.contrast", model.contrast,
                                       {.showValue = true});
            if (result.changed) model.contrast = result.value;
            (void)field;
        }
        {
            auto field = beginField(ui, "Steps of 0.25");
            const auto result = slider(ui, input, "controls.stepped", model.stepped,
                                       {.step = 0.25, .showValue = true});
            if (result.changed) model.stepped = result.value;
            (void)field;
        }
        {
            auto field = beginField(ui, "Disabled");
            // Discarded on purpose: a disabled control has nothing to report.
            (void)slider(ui, input, "controls.disabledslider", 0.4, {.disabled = true});
            (void)field;
        }
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 12.0f});
        sectionTitle(ui, "Progress", "widgets/progress");
        {
            auto field = beginField(ui, "Determinate");
            progressBar(ui, {.value = model.contrast});
            (void)field;
        }
        {
            auto field = beginField(ui, "Indeterminate");
            progressBar(ui, {.value = -1.0, .phase = model.clock});
            (void)field;
        }
        (void)panel;
    }
}

void listsTab(Ui& ui, const Interaction& input, Model& model) {
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f, .grow = 1.0f});
        sectionTitle(ui, "Virtualised history", "widgets/virtualList · scroll · listRow");

        // Arrow keys walk the selection and keep it on screen. The list is the
        // focusable node, so this belongs to the application rather than to a
        // row — and it reads `isFocusedWithin`, so clicking a row does not take
        // the keyboard away from the list.
        if (input.isFocusedWithin("controls.history")) {
            const std::size_t before = model.commit;
            for (const KeyEvent& event : input.keys()) {
                if (event.key == Key::Down && model.commit + 1 < kCommits) ++model.commit;
                if (event.key == Key::Up && model.commit > 0) --model.commit;
            }
            if (model.commit != before) {
                revealRow(model.history, historyOptions().rows(), model.commit);
            }
        }
        {
            // Fifty thousand commits, of which only the ones on screen are
            // ever built.
            //
            // An explicit height, not `grow`: this page scrolls, and inside a
            // scroll there is no leftover to grow into — the content is as tall
            // as it says it is and the view moves it. Left growing, the frame
            // collapsed to its minimum and showed four rows of fifty thousand.
            Style frame;
            frame.height = 320.0f;
            frame.shrink = 0.0f;
            frame.border = Border{1.0f, Fill{Token::Border}};
            auto frameScope = ui.begin(frame);
            model.shown = virtualList(
                ui, input, "controls.history", model.history, historyOptions(),
                [&](Ui& ui, std::size_t index) {
                    const std::string tag = commitTag(index);
                    auto row = beginListRow(ui, {.selected = model.commit == index,
                                                 .hovered = input.isHovered(tag),
                                                 .height = kCommitRowHeight,
                                                 .padding = Edges::symmetric(0.0f, 8.0f),
                                                 .gap = 6.0f,
                                                 .id = tag});
                    text(ui, commitSubject(index), {.size = 11.0f, .grow = 1.0f});
                    text(ui, commitHash(index),
                         {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.0f});
                    if (input.clicked(tag)) model.commit = index;
                    (void)row;
                });
            (void)frameScope;
        }
        text(ui, std::to_string(kCommits) + " commits, " + std::to_string(model.shown.count) +
                     " rows built — click it, then use the arrows",
             {.color = Token::TextMuted, .size = 11.0f});
        (void)panel;
    }
    {
        // A card built from the box preset, with a gradient behind its heading —
        // both of them are one option each.
        auto panel = beginBox(ui, BoxStyle::card({.gap = 10.0f}));
        {
            BoxOptions banner_options;
            banner_options.align = Align::Center;
            banner_options.padding = Edges::symmetric(0.0f, 10.0f);
            banner_options.height = 34.0f;
            banner_options.backgroundGradient =
                Gradient::linear(Fill{Token::Accent}, Fill{Token::Accent, 0.0f}, 90.0f);
            banner_options.radius = 6.0f;
            auto banner = beginBox(ui, banner_options);
            text(ui, "Scroll and gradients",
                 {.color = Token::TextStrong, .weight = FontWeight::SemiBold});
            (void)banner;
        }
        {
            // A scroll view: the content is laid out at its natural height and
            // clipped, and the wheel moves it.
            Style frame;
            frame.height = 96.0f;
            frame.border = Border{1.0f, Fill{Token::Border}};
            auto frameScope = ui.begin(frame);
            auto scroll = beginScroll(ui, input, "controls.log", model.log,
                                      {.padding = Edges::all(8.0f), .gap = 2.0f});
            for (int i = 1; i <= 24; ++i) {
                text(ui, "git fetch origin --prune  (" + std::to_string(i) + ")",
                     {.color = i % 4 == 0 ? Token::TextMuted : Token::Text,
                      .role = FontRole::Mono, .size = 11.0f});
            }
            (void)scroll;
            (void)frameScope;
        }
        text(ui, "wheel over the log; hover its bar, then drag it",
             {.color = Token::TextMuted, .size = 11.0f});
        (void)panel;
    }
}

void editorTab(Ui& ui, const Interaction& input, Model& model) {
    auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
    sectionTitle(ui, "Rich text", "widgets/richEditor");

    // The toolbar is the caller's to compose: the default set, plus a button of
    // this application's own.
    RichEditorOptions options;
    options.toolbar = defaultToolbar();
    options.toolbar.push_back({.action = EditorAction::Separator});
    options.toolbar.push_back(
        {.action = EditorAction::Custom, .icon = Icon::GitBranch, .tooltip = "Insert a branch name",
         .onClick = [](RichDocument& document, RichEditorState& state) {
             Block& block = document.blocks[state.block];
             const std::size_t at = state.edit.caret;
             const std::string_view name = "feat/nord-tuning";
             block.text.insert(at, name);
             block.marks.push_back({at, at + name.size(), Mark::Code});
             state.edit.text = block.text;
             state.edit.caret = at + name.size();
             state.edit.anchor = state.edit.caret;
         }});
    options.minHeight = 300.0f;

    richEditor(ui, input, "controls.editor", model.doc, model.docState, options);
    text(ui, "click a block to edit it, type, and Return splits it; the last button is this "
             "application's own, not the toolkit's",
         {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
    (void)panel;
}

void tableTab(Ui& ui, const Interaction& input, Model& model) {
    // An explicit height, not `grow`: this page scrolls, and inside a scroll
    // there is no leftover to grow into — the content is as tall as it says it
    // is and the view moves it.
    auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
    sectionTitle(ui, "Per-developer statistics", "widgets/table");

    {
        auto row = ui.beginRow({.gap = 16.0f});
        if (checkbox(ui, input, "controls.table.rowlines", model.tableRowLines, {.label = "Row lines"})) {
            model.tableRowLines = !model.tableRowLines;
        }
        if (checkbox(ui, input, "controls.table.collines", model.tableColumnLines, {.label = "Column lines"})) {
            model.tableColumnLines = !model.tableColumnLines;
        }
        (void)row;
    }

    // Sorting is opted into per column, and only where `sortNow` below can
    // actually reorder by it — LAST SEEN is a formatted string with no ordering
    // this demo implements, so it does not offer one.
    const std::vector<Column> columns{
        {.title = "DEVELOPER", .sizing = ColumnSize::Fraction, .width = 2.0f, .minWidth = 120.0f,
         .sortable = true},
        {.title = "COMMITS", .sizing = ColumnSize::FitContent, .fitSample = "999 999",
         .align = TextAlign::End, .sortable = true},
        {.title = "ADDED", .sizing = ColumnSize::FitContent, .fitSample = "+99 999",
         .align = TextAlign::End, .sortable = true},
        {.title = "REMOVED", .sizing = ColumnSize::FitContent, .fitSample = "-99 999",
         .align = TextAlign::End, .sortable = true},
        {.title = "LAST SEEN", .sizing = ColumnSize::Fixed, .width = 110.0f},
    };

    // Sorted by the *application*, because only it knows how to compare two of
    // its own rows — the table reports which column and which way.
    std::vector<Contributor>& people = model.contributors;
    const auto sortNow = [&] {
        const int column = model.table.sortColumn;
        if (column < 0) return;
        const bool up = model.table.ascending;
        std::stable_sort(people.begin(), people.end(),
                         [&](const Contributor& a, const Contributor& b) {
                             switch (column) {
                                 case 0: return up ? a.name < b.name : b.name < a.name;
                                 case 1: return up ? a.commits < b.commits : b.commits < a.commits;
                                 case 2: return up ? a.added < b.added : b.added < a.added;
                                 case 3: return up ? a.removed < b.removed : b.removed < a.removed;
                                 default: return false;
                             }
                         });
    };

    const TableResult sheet = table(
        ui, input, "controls.table", columns, people.size(), model.table,
        [&](Ui& cellUi, std::size_t row, std::size_t column) {
            const Contributor& person = people[row];
            switch (column) {
                case 0:
                    text(cellUi, person.name, {.color = Token::Text, .size = 12.0f, .grow = 1.0f});
                    break;
                case 1:
                    text(cellUi, std::to_string(person.commits),
                         {.color = Token::Text, .role = FontRole::Mono, .size = 12.0f,
                          .align = TextAlign::End});
                    break;
                case 2:
                    text(cellUi, "+" + std::to_string(person.added),
                         {.color = Token::Added, .role = FontRole::Mono, .size = 12.0f,
                          .align = TextAlign::End});
                    break;
                case 3:
                    text(cellUi, "-" + std::to_string(person.removed),
                         {.color = Token::Removed, .role = FontRole::Mono, .size = 12.0f,
                          .align = TextAlign::End});
                    break;
                default:
                    text(cellUi, person.lastSeen,
                         {.color = Token::TextMuted, .size = 11.0f});
                    break;
            }
        },
        {.rowHeight = 30.0f, .zebra = true, .rowLines = model.tableRowLines,
         .columnLines = model.tableColumnLines, .grow = 0.0f, .height = 380.0f});
    if (sheet.sortChanged) sortNow();

    richText(ui, {{.text = std::to_string(people.size()) + " rows, ",
                   .color = Token::TextMuted, .size = 11.0f},
                  {.text = std::to_string(sheet.shown.count) + " built",
                   .color = Token::Accent, .weight = FontWeight::SemiBold, .size = 11.0f},
                  {.text = " — click a header to sort, drag a divider to resize, click a row or "
                           "use the arrows",
                   .color = Token::TextMuted, .size = 11.0f}});
    (void)panel;
}

// ---------------------------------------------------------------------------
// Motion
// ---------------------------------------------------------------------------

/** The curves worth putting side by side, with the names CSS knows them by
 *  where it has one. */
struct Curve {
    Easing easing;
    const char* name;
    const char* note;
};

const std::vector<Curve>& curveGallery() {
    static const std::vector<Curve> curves{
        {Easing::Linear, "linear", "no easing at all — reads as mechanical"},
        {Easing::Ease, "ease", "CSS's default, and not symmetric"},
        {Easing::EaseOut, "ease-out", "the everyday choice: fast, then settles"},
        {Easing::EaseInOut, "ease-in-out", "for something moving between two places"},
        {Easing::SineInOut, "sine-in-out", "the softest useful curve"},
        {Easing::QuadOut, "quad-out", "a firmer ease-out"},
        {Easing::CubicOut, "cubic-out", "firmer still"},
        {Easing::QuintOut, "quint-out", "almost stationary at the end"},
        {Easing::ExpoOut, "expo-out", "for something arriving from off screen"},
        {Easing::CircOut, "circ-out", "flat, then vertical"},
        {Easing::BackOut, "back-out", "overshoots once — wrong for an opacity"},
        {Easing::Spring, "spring", "overshoots and settles; for an arrival"},
        {Easing::ElasticOut, "elastic-out", "oscillates several times"},
        {Easing::BounceOut, "bounce-out", "lands, and bounces"},
    };
    return curves;
}

/** Plots one easing curve, with the eased position marked on it. */
void curvePlot(Ui& ui, const Curve& curve, float phase) {
    std::vector<Shape> shapes;
    constexpr float kW = 96.0f;
    constexpr float kH = 52.0f;

    // The three curves that leave 0..1 are drawn to a taller range, so an
    // overshoot is visible rather than clipped at the frame.
    const float low = -0.35f;
    const float high = 1.35f;
    const auto yAt = [&](float v) { return kH * (1.0f - (v - low) / (high - low)); };

    Path baseline;
    baseline.moveTo({0.0f, yAt(0.0f)});
    baseline.lineTo({kW, yAt(0.0f)});
    shapes.push_back({std::move(baseline), Fill{Token::Border}, 1.0f});
    Path unit;
    unit.moveTo({0.0f, yAt(1.0f)});
    unit.lineTo({kW, yAt(1.0f)});
    shapes.push_back({std::move(unit), Fill{Token::Border}, 1.0f});

    Path line;
    for (int i = 0; i <= 48; ++i) {
        const float t = static_cast<float>(i) / 48.0f;
        const Vec2 point{t * kW, yAt(ease(curve.easing, t))};
        if (i == 0) line.moveTo(point);
        else line.lineTo(point);
    }
    shapes.push_back({std::move(line), Fill{Token::Accent}, 1.5f});

    // Where a value on this curve is right now, so the plot and the swatch
    // beside it are visibly the same number.
    Path head;
    const Vec2 at{phase * kW, yAt(ease(curve.easing, phase))};
    head.moveTo({at.x - 0.1f, at.y});
    head.lineTo({at.x + 0.1f, at.y});
    shapes.push_back({std::move(head), Fill{Token::Graph3}, 7.0f});

    Style box;
    box.width = kW;
    box.height = kH;
    box.shrink = 0.0f;
    box.overflow = Overflow::Hidden;
    ui.draw(box, std::move(shapes));
}

void motionTab(Ui& ui, const Interaction& input, Model& model) {
    // Raw progress, eased by nothing: a *linear* animation from 0 to 1 over the
    // chosen duration. Every curve is then sampled at this same instant, which
    // is the only way a row of curves is honestly comparable — an eased clock
    // would bake one curve's shape into all the others.
    const float target = model.motionPlaying ? 1.0f : 0.0f;
    const float phase =
        ui.animator()
            ? ui.animator()->animate("motion.clock", "t", target,
                                     {.duration = std::max(0.02f, model.motionDuration),
                                      .easing = Easing::Linear})
            : 0.55f;

    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "What a transition is here", "anim/animator");
        text(ui,
             "The model is CSS's `transition`, not a keyframe timeline: a component names a "
             "value and a duration, and the animator carries it from wherever it was to "
             "wherever it now is. Nothing declares a start — there isn't one, because the "
             "value was already somewhere.",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});

        {
            auto row = ui.beginRow({.align = Align::Center, .gap = 14.0f});
            text(ui, "duration", {.color = Token::TextMuted, .size = 11.0f});
            const auto picked = slider(ui, input, "motion.duration", model.motionDuration,
                                       {.minimum = 0.0, .maximum = 1.2, .width = 190.0f});
            if (picked.changed) model.motionDuration = static_cast<float>(picked.value);
            char label[32];
            std::snprintf(label, sizeof(label), "%.2f s", static_cast<double>(model.motionDuration));
            text(ui, label, {.color = Token::Accent, .role = FontRole::Mono, .size = 11.0f});
            (void)row;
        }
        text(ui, "zero is not a special case — it just makes every change immediate, which is "
                 "how a component opts out without the call site changing shape",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }


    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        {
            auto head = ui.beginRow({.align = Align::Center, .gap = 12.0f});
            sectionTitle(ui, "The curves", "anim/easing");
            ui.add({.grow = 1.0f});
            button(ui, input, model.motionPlaying ? "Play back" : "Play",
                   {.variant = ButtonVariant::Primary, .id = "motion.play"});
            if (input.clicked("motion.play")) model.motionPlaying = !model.motionPlaying;
            (void)head;
        }
        text(ui, "press play: every curve is sampled at the same instant, so the dots race "
                 "each other and the differences between the curves are the gaps between them",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});

        // The chosen curve, applied to something that is not a plot.
        {
            const float t = ease(curveGallery()[model.motionCurve].easing, phase);
            Style track;
            track.height = 42.0f;
            track.radius = 6.0f;
            track.background = Fill{Token::BgElevated};
            auto trackScope = ui.begin(track);
            Style box;
            box.position = Position::Absolute;
            box.left = 6.0f + t * 240.0f;
            box.top = 5.0f;
            box.width = 32.0f;
            box.height = 32.0f;
            box.radius = 6.0f + t * 10.0f;
            box.background = Fill{Token::Accent};
            ui.add(box);
            (void)trackScope;
        }

        Style grid;
        grid.direction = Direction::Row;
        grid.wrap = true;
        grid.gap = 14.0f;
        auto gridScope = ui.begin(grid);
        for (std::size_t i = 0; i < curveGallery().size(); ++i) {
            const Curve& curve = curveGallery()[i];
            const std::string id = "motion.curve." + std::to_string(i);
            const bool chosen = model.motionCurve == i;

            Style cell;
            cell.direction = Direction::Column;
            cell.gap = 4.0f;
            cell.padding = Edges::all(8.0f);
            cell.radius = 6.0f;
            cell.shrink = 0.0f;
            cell.cursorHint = Cursor::Pointer;
            if (chosen) cell.background = Fill{Token::Accent, 0.16f};
            else if (input.isHovered(id)) cell.background = Fill{Token::SurfaceHover};
            {
                auto cellScope = ui.begin(cell);
                ui.tag(id).cursor(Cursor::Pointer);
                curvePlot(ui, curve, phase);
                text(ui, curve.name,
                     {.color = chosen ? Token::Accent : Token::Text,
                      .weight = FontWeight::SemiBold, .role = FontRole::Mono, .size = 10.0f});
                (void)cellScope;
            }
            if (input.clicked(id)) model.motionCurve = i;
        }
        (void)gridScope;

        text(ui, std::string(curveGallery()[model.motionCurve].name) + " — " +
                     curveGallery()[model.motionCurve].note,
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        text(ui, "the last families leave 0..1 on purpose, which is why the plots are drawn "
                 "taller than the unit square. Fine for a position; wrong for an opacity, which "
                 "would clip.",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }

}

void chartsTab(Ui& ui, const Interaction& input, Model& model) {
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        {
            auto head = ui.beginRow({.align = Align::Center, .gap = 10.0f});
            sectionTitle(ui, "Frame time, live", "widgets/chart");
            ui.add({.grow = 1.0f});
            if (switchToggle(ui, input, "controls.fps.pause", model.fpsPaused,
                             {.label = "Paused"})) {
                model.fpsPaused = !model.fpsPaused;
            }
            (void)head;
        }

        // The real thing this measures: how long the loop took, sample by
        // sample. A chart of the toolkit drawing itself.
        Series frames;
        frames.name = "draw";
        frames.values = model.drawTimes;
        frames.color = Token::Accent;
        frames.fillAlpha = 0.18f;
        frames.thickness = 1.5f;

        const ChartResult hovered =
            lineChart(ui, input, "controls.fps", {frames},
                      {.tickCount = 4, .height = 130.0f, .valueFormat = "%.1f"});

        const std::size_t at = hovered.hoveredIndex >= 0
                                   ? static_cast<std::size_t>(hovered.hoveredIndex)
                                   : model.drawTimes.size() - 1;
        const double shown =
            model.drawTimes.empty() ? 0.0
                                    : model.drawTimes[std::min(at, model.drawTimes.size() - 1)];

        // A single sample jumps around; the median of the window is what the
        // loop actually costs. Both are shown because they answer different
        // questions — "is there a spike?" and "how fast is this?".
        std::vector<double> sorted = model.drawTimes;
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted.empty() ? 0.0 : sorted[sorted.size() / 2];

        char caption[96];
        std::snprintf(caption, sizeof(caption), "%.2f ms", shown);
        char headroom[128];
        std::snprintf(headroom, sizeof(headroom), "median %.2f ms — %.0f fps if uncapped", median,
                      median > 0.0 ? 1000.0 / median : 0.0);
        richText(ui, {{.text = hovered.hoveredIndex >= 0 ? "sample " : "draw "},
                      {.text = caption, .color = Token::Accent, .weight = FontWeight::SemiBold,
                       .role = FontRole::Mono},
                      {.text = "  ·  "},
                      {.text = headroom, .color = Token::TextMuted, .role = FontRole::Mono}});
        text(ui,
             model.fpsPaused
                 ? "paused — turn the switch off to sample again. Frozen by default because a "
                   "plot that redraws every frame to show how long a redraw takes is measuring "
                   "itself, and it animates a page somebody came to read."
                 : "build, layout and rasterise — the part the toolkit controls. The loop is "
                   "paced by the display, so the frame time is the refresh interval and says "
                   "more about the monitor than about us. Hover the plot to read a sample.",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        {
            // The control sits above the plot and hard right, where a chart's
            // own controls live — not below it in the prose, where it reads as
            // part of the caption.
            auto head = ui.beginRow({.align = Align::Center, .gap = 10.0f});
            sectionTitle(ui, "Two series, one scale", "widgets/chart — lineChart");
            ui.add({.grow = 1.0f});
            // Always there, disabled when there is nothing to undo. A control
            // that appears once it becomes useful moves everything beside it
            // and never advertises that it exists.
            button(ui, input, "",
                   {.variant = ButtonVariant::Ghost, .leading = Icon::RotateCcw,
                    .disabled = model.churnView.whole(), .height = 26.0f,
                    .id = "controls.churn.reset"});
            if (input.clicked("controls.churn.reset")) model.churnView.reset();
            (void)head;
        }
        // Outside the row: a tooltip anchors to its control rather than sitting
        // inside it, and built in the row it would stretch the row.
        tooltip(ui, input, "controls.churn.reset",
                model.churnView.whole() ? "Showing everything" : "Reset zoom");

        Series added;
        added.name = "added";
        added.color = Token::Added;
        added.fillAlpha = 0.14f;
        Series removed;
        removed.name = "removed";
        removed.color = Token::Removed;
        removed.fillAlpha = 0.0f;
        for (int i = 0; i < 28; ++i) {
            const double t = static_cast<double>(i);
            added.values.push_back(60.0 + 45.0 * std::sin(t / 3.4) + 12.0 * std::sin(t / 1.3));
            removed.values.push_back(28.0 + 22.0 * std::sin(t / 4.9 + 1.2));
        }
        // A readout whose *content* is the caller's: the toolkit lays the panel
        // out, this decides what belongs in it. Here that is the net figure,
        // which is not a reformatting of either series and so could not have
        // been a format string.
        ChartTooltip readout;
        readout.title = [](const TooltipContext& at) {
            return "day " + std::to_string(at.index + 1);
        };
        readout.rows = [](const TooltipContext& at) -> std::vector<TooltipRow> {
            std::vector<TooltipRow> rows;
            if (!at.series) return rows;
            double net = 0.0;
            for (std::size_t s = 0; s < at.series->size(); ++s) {
                const Series& one = (*at.series)[s];
                if (at.index >= one.values.size()) continue;
                char value[32];
                std::snprintf(value, sizeof(value), "%.0f", one.values[at.index]);
                rows.push_back({one.color, std::string(one.name), value});
                net += s == 0 ? one.values[at.index] : -one.values[at.index];
            }
            char total[32];
            std::snprintf(total, sizeof(total), "%+.0f", net);
            rows.push_back({std::nullopt, "net", total});
            return rows;
        };

        const ChartResult churn =
            lineChart(ui, input, "controls.churn", {added, removed}, model.churnView,
                      {.height = 130.0f, .tooltip = readout},
                      {.wheel = true, .wheelModifier = true, .drag = ChartDrag::Select});
        chartBrush(ui, input, "controls.churn.brush", {added, removed}, model.churnView,
                   {.axisWidth = 34.0f});
        text(ui,
             model.churnView.whole()
                 ? "Ctrl+wheel over the plot to zoom, then drag it to pan — a plain wheel "
                   "still scrolls the page"
                 : "drag the shaded strip's edges to change the window, or its middle to "
                   "draw a new one",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)churn;
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Commits per weekday", "widgets/chart — barChart");

        Series thisWeek;
        thisWeek.name = "this week";
        thisWeek.values = {18.0, 27.0, 31.0, 24.0, 39.0, 8.0, 4.0};
        Series lastWeek;
        lastWeek.name = "last week";
        lastWeek.values = {12.0, 22.0, 26.0, 30.0, 21.0, 6.0, 2.0};
        const std::vector<std::string> days{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

        const ChartResult bars = barChart(
            ui, input, "controls.weekday", {thisWeek, lastWeek},
            {.height = 150.0f,
             .grouping = model.stackBars ? BarGrouping::Stacked : BarGrouping::Grouped,
             .shape = model.lollipops ? BarShape::Lollipop : BarShape::Bar,
             .categories = days});
        {
            auto row = ui.beginRow({.align = Align::Center, .gap = 16.0f});
            if (checkbox(ui, input, "controls.bars.stack", model.stackBars, {.label = "Stacked"})) {
                model.stackBars = !model.stackBars;
            }
            if (checkbox(ui, input, "controls.bars.pop", model.lollipops, {.label = "Lollipop"})) {
                model.lollipops = !model.lollipops;
            }
            (void)row;
        }
        text(ui,
             bars.hoveredIndex >= 0
                 ? "hovering " + days[static_cast<std::size_t>(bars.hoveredIndex)] +
                       " — the whole slot is the target, not the bar"
                 : std::string("grouped compares the series, stacked compares the totals; the "
                               "same call draws both"),
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Lines changed per release", "widgets/chart — barChart");

        Series sizes;
        sizes.values = {1240.0, 860.0, 2100.0, 640.0, 1580.0};
        barChart(ui, input, "controls.releases", {sizes},
                 {.height = 132.0f,
                  .horizontal = true,
                  .categories = {"v1.0", "v1.1", "v1.2", "v1.3", "v2.0"},
                  .categoryAxis = 46.0f});
        text(ui, "horizontal, because the category names are words rather than numbers",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Open pull requests, daily range", "widgets/chart — candlestickChart");

        // Open/high/low/close over ten days: how many were open at the start of
        // the day, the most and fewest at any point, and how many at the end.
        const std::vector<Candle> days{
            {18, 24, 17, 22}, {22, 27, 21, 26}, {26, 28, 22, 23}, {23, 25, 19, 20},
            {20, 23, 18, 22}, {22, 30, 22, 29}, {29, 31, 26, 27}, {27, 27, 21, 22},
            {22, 26, 20, 25}, {25, 33, 24, 32}};
        const ChartResult tick = candlestickChart(
            ui, input, "controls.prs", days,
            {.height = 150.0f,
             .categories = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"}});
        std::string caption = "the only chart here whose scale does not reach zero — a range of "
                              "17 to 33 drawn from zero would be a stripe at the top";
        if (tick.hoveredIndex >= 0) {
            const Candle& day = days[static_cast<std::size_t>(tick.hoveredIndex)];
            char line[128];
            std::snprintf(line, sizeof(line), "day %d — open %.0f, high %.0f, low %.0f, close %.0f",
                          tick.hoveredIndex + 1, day.open, day.high, day.low, day.close);
            caption = line;
        }
        text(ui, caption, {.color = Token::TextMuted, .size = 11.0f,
                           .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Commits by weekday and hour", "widgets/chart — heatmap");

        // Seven rows of twenty-four: the shape a contribution grid takes, and
        // the case the widget exists for.
        std::vector<std::vector<double>> activity(7, std::vector<double>(24, 0.0));
        for (std::size_t day = 0; day < 7; ++day) {
            for (std::size_t hour = 0; hour < 24; ++hour) {
                const double d = static_cast<double>(day);
                const double h = static_cast<double>(hour);
                // Busy on weekday afternoons, quiet at night and at weekends.
                const double working = std::exp(-std::pow((h - 15.0) / 4.2, 2.0));
                const double weekday = day < 5 ? 1.0 : 0.22;
                activity[day][hour] =
                    std::round(working * weekday * (18.0 + 6.0 * std::sin(d * 1.7 + h * 0.4)));
            }
        }

        const HeatmapResult cell =
            heatmap(ui, input, "controls.activity", activity,
                    {.rows = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"},
                     .columns = {"00", "", "", "03", "", "", "06", "", "", "09", "", "",
                                 "12", "", "", "15", "", "", "18", "", "", "21", "", ""}});
        std::string caption =
            "one colour at five strengths, not a rainbow — hue carries no order, so a "
            "multi-hue scale sends the reader back to the legend for every cell";
        if (cell.hoveredRow >= 0) {
            static const char* kDays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
            char line[128];
            std::snprintf(line, sizeof(line), "%s at %02d:00 — %.0f commits",
                          kDays[cell.hoveredRow], cell.hoveredColumn, cell.hoveredValue);
            caption = line;
        }
        text(ui, caption,
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Review time against change size", "widgets/chart — scatterChart");

        // Two continuous quantities against each other: how big a pull request
        // was, and how long it sat. Neither axis is an index.
        PointSeries merged;
        merged.name = "merged";
        merged.points = {{42, 3.5}, {88, 6.0},  {130, 5.2}, {210, 11.0}, {64, 2.0},
                         {310, 14.5}, {150, 8.0}, {96, 4.4}, {420, 19.0}, {58, 3.0},
                         {180, 9.5}, {245, 12.0}, {75, 5.5}, {360, 16.0}, {120, 6.6}};
        PointSeries abandoned;
        abandoned.name = "closed unmerged";
        abandoned.points = {{190, 22.0}, {280, 26.0}, {95, 18.0}, {340, 31.0}, {150, 24.0}};

        const ScatterResult dot =
            scatterChart(ui, input, "controls.review", {merged, abandoned},
                         {.height = 210.0f, .valueFormat = "%.0fh", .xFormat = "%.0f"});
        std::string caption = "lines changed across the bottom, hours to review up the side — "
                              "the hit test is the nearest dot, because a scatter has no columns";
        if (dot.hoveredSeries >= 0) {
            const PointSeries& which =
                dot.hoveredSeries == 0 ? merged : abandoned;
            const Point& at = which.points[static_cast<std::size_t>(dot.hoveredIndex)];
            char line[128];
            std::snprintf(line, sizeof(line), "%s — %.0f lines, %.1f hours",
                          std::string(which.name).c_str(), at.x, at.y);
            caption = line;
        }
        text(ui, caption,
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Files, churn and how many touched them",
                     "widgets/chart — scatterChart with weights");

        // The same call with a third value per point, which becomes the area.
        PointSeries files;
        files.name = "file";
        files.points = {{120, 4, 3}, {480, 9, 12}, {260, 6, 7},  {910, 14, 26},
                        {75, 2, 2},  {640, 11, 18}, {330, 7, 5}, {1200, 17, 34},
                        {200, 5, 9}};
        scatterChart(ui, input, "controls.files", {files},
                     {.height = 200.0f, .valueFormat = "%.0f", .xFormat = "%.0f"});
        text(ui, "the third value is the dot's area, not its radius — mapping it to the radius "
                 "would make a doubled value look four times bigger",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 10.0f});
        sectionTitle(ui, "Share of the work", "widgets/chart — donutChart");

        // No colours named: they come from the design's chart palette, so the
        // donut re-themes with everything else.
        // Enough contributors that the legend has to scroll, which is the
        // point of bounding it.
        const std::vector<Slice> people{
            {"hermes", 42.0},  {"gustavo", 31.0}, {"ana", 18.0},   {"bot", 9.0},
            {"marina", 7.5},   {"caio", 6.0},     {"lu", 4.5},     {"pedro", 3.0},
            {"rita", 2.5},     {"dev-ci", 1.5}};
        const DonutResult wedge =
            donutChart(ui, input, "controls.share", people, model.share);
        const std::string caption =
            model.share.focused >= 0
                ? std::string("focused on ") +
                      std::string(people[static_cast<std::size_t>(model.share.focused)].name) +
                      " — click it again, or the hole, to put the rest back"
            : wedge.hoveredIndex >= 0
                ? std::string("hovering ") +
                      std::string(people[static_cast<std::size_t>(wedge.hoveredIndex)].name) +
                      " — click to single it out"
                : std::string("click a wedge or a name to single it out; the hit test is by "
                              "angle and radius, not by bounding box");
        text(ui, caption,
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
}

void pickersTab(Ui& ui, const Interaction& input, Model& model) {
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 12.0f});
        sectionTitle(ui, "Colour", "widgets/colorPicker · core/color");
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 24.0f;
            row.align = Align::Start;
            auto rowScope = ui.begin(row);

            // The full picker: hue, alpha and a row of the theme's own colours.
            const ColorPickerResult picked =
                colorPicker(ui, input, "controls.colour", model.colour,
                            {.swatches = {Color{37, 99, 235}, Color{34, 197, 94},
                                          Color{239, 68, 68}, Color{255, 171, 0},
                                          Color{213, 0, 249}, Color{255, 255, 255}}});

            {
                Style side;
                side.direction = Direction::Column;
                side.gap = 10.0f;
                side.grow = 1.0f;
                side.basis = 0.0f;
                auto sideScope = ui.begin(side);

                // A big swatch, so the choice is visible away from the square.
                Style preview;
                preview.height = 60.0f;
                preview.radius = 8.0f;
                preview.background = Fill{picked.color};
                preview.border = Border{1.0f, Fill{Token::Border}};
                ui.add(preview);

                text(ui, "drag the square, the hue rail or the alpha rail",
                     {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});

                divider(ui, Direction::Column);
                text(ui, "In a form", {.color = Token::TextMuted,
                                       .weight = FontWeight::SemiBold, .size = 11.0f});
                {
                    // The same picker behind a swatch, which is what a form
                    // wants: it borrows the space instead of owning it.
                    auto field = beginField(ui, "Project colour");
                    colorField(ui, input, "controls.projectcolour", model.projectColour,
                               {{.alpha = false, .width = 220.0f}});
                    (void)field;
                }

                divider(ui, Direction::Column);
                text(ui, "Without alpha or hue", {.color = Token::TextMuted,
                                                  .weight = FontWeight::SemiBold, .size = 11.0f});
                // The same component with its rails turned off: a control that
                // only shades one hue, which is what a "tint of the accent"
                // picker wants.
                colorPicker(ui, input, "controls.tint", model.tint,
                            {.alpha = false, .hue = false, .showHex = true, .width = 200.0f,
                             .squareHeight = 90.0f});
                (void)sideScope;
            }
            (void)rowScope;
        }
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 12.0f});
        sectionTitle(ui, "Date and time together", "widgets/dateTimePicker");
        {
            // The pair behind one input, which is how a due date is asked for
            // far more often than as two controls side by side.
            DateTimeFieldOptions dueOptions;
            dueOptions.pattern = "EEE, d MMM yyyy 'at' HH:mm";
            dueOptions.width = 280.0f;
            const DateTimeFieldResult scheduled =
                dateTimeField(ui, input, "controls.scheduled", model.scheduled,
                              model.scheduledField, dueOptions);
            if (scheduled.changed) model.scheduled = scheduled.when;
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 20.0f;
            row.align = Align::Start;
            auto rowScope = ui.begin(row);

            DateTimePickerOptions options;
            options.time.use24Hour = false;   // twelve-hour, with the AM/PM column
            options.time.minuteStep = 5;
            options.date.cellSize = 26.0f;
            const DateTimePickerResult picked = dateTimePicker(
                ui, input, "controls.when", model.when, model.whenView, options);
            if (picked.changed) model.when = picked.when;

            {
                Style side;
                side.direction = Direction::Column;
                side.gap = 6.0f;
                side.grow = 1.0f;
                side.basis = 0.0f;
                auto sideScope = ui.begin(side);
                // One pattern spanning both halves — the formatter hands each
                // run of letters to whichever of the two owns it.
                const std::string patterns[] = {"dd/MM/yyyy HH:mm", "EEE, d MMM 'at' h:mm a",
                                                "yyyy-MM-dd'T'HH:mm:ss"};
                for (const std::string& pattern : patterns) {
                    richText(ui, {{.text = pattern, .color = Token::TextMuted,
                                   .role = FontRole::Mono, .size = 11.0f},
                                  {.text = "  ", .size = 11.0f},
                                  {.text = formatDateTime(model.when, pattern),
                                   .color = Token::Accent, .weight = FontWeight::SemiBold}});
                }
                text(ui, "twelve-hour here is a display: the value stays 0–23, so switching to "
                         "AM/PM keeps the same instant",
                     {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
                (void)sideScope;
            }
            (void)rowScope;
        }
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 12.0f});
        sectionTitle(ui, "Date", "widgets/datePicker · timePicker");
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 24.0f;
            row.align = Align::Start;
            auto rowScope = ui.begin(row);

            // Bounded to a window around today, so the disabled days are
            // visible rather than theoretical.
            DatePickerOptions dates;
            dates.minimum = Date::fromSerial(Date::today().serial() - 20);
            dates.maximum = Date::fromSerial(Date::today().serial() + 45);
            const DatePickerResult day =
                datePicker(ui, input, "controls.date", model.date, model.dateView, dates);
            if (day.chosen) model.date = day.date;

            {
                Style side;
                side.direction = Direction::Column;
                side.gap = 8.0f;
                side.grow = 1.0f;
                side.basis = 0.0f;
                auto sideScope = ui.begin(side);
                // The same date through four patterns, which is what makes the
                // display format the caller's decision rather than the widget's.
                CalendarLocale ptBr;
                ptBr.months = {"janeiro", "fevereiro", "março",    "abril",   "maio",     "junho",
                               "julho",   "agosto",    "setembro", "outubro", "novembro",
                               "dezembro"};
                ptBr.weekdayNames = {"domingo",      "segunda-feira", "terça-feira",
                                     "quarta-feira", "quinta-feira",  "sexta-feira", "sábado"};
                const std::string patterns[][2] = {
                    {"dd/MM/yyyy", ""},
                    {"MM/dd/yy", ""},
                    {"EEE, d MMM yyyy", ""},
                    {"d 'de' MMMM 'de' yyyy", "pt-BR"},
                };
                for (const auto& row : patterns) {
                    const bool localised = !row[1].empty();
                    richText(ui,
                             {{.text = row[0], .color = Token::TextMuted,
                               .role = FontRole::Mono, .size = 11.0f},
                              {.text = "  ", .size = 11.0f},
                              {.text = localised ? formatDate(model.date, row[0], ptBr)
                                                 : formatDate(model.date, row[0]),
                               .color = Token::Accent, .weight = FontWeight::SemiBold}});
                }
                text(ui, "click a day, or focus the grid and use the arrows; Page Up and Page "
                         "Down change month",
                     {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});

                divider(ui, Direction::Column);
                text(ui, "The same calendar behind a field",
                     {.color = Token::TextMuted, .weight = FontWeight::SemiBold, .size = 11.0f});
                {
                    // Nothing chosen shows the placeholder rather than quietly
                    // pretending it is today: "no date" and "today" are
                    // different answers.
                    DateFieldOptions dueOptions;
                    dueOptions.pattern = "EEE, d MMM yyyy";
                    dueOptions.width = 220.0f;
                    const DateFieldResult due =
                        dateField(ui, input, "controls.due", model.due, model.dueView, dueOptions);
                    if (due.changed) model.due = due.date;
                }
                text(ui, "days outside the allowed range are drawn and dimmed, not hidden",
                     {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
                (void)sideScope;
            }
            (void)rowScope;
        }
        (void)panel;
    }
}

/** An element: it draws one thing and reports one press. */
void buttonsTab(Ui& ui, const Interaction& input, Model& model) {
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 12.0f});
        sectionTitle(ui, "Variants", "widgets/button");
        {
            auto row = ui.beginRow({.align = Align::Center, .gap = 10.0f});
            // No `.ripple` here: what a press looks like is the design's call,
            // so switching design above turns the ink on for all of them at
            // once.
            button(ui, input, "PRIMARY", {.variant = ButtonVariant::Primary, .id = "controls.b1"});
            button(ui, input, "SECONDARY",
                   {.variant = ButtonVariant::Secondary, .id = "controls.b2"});
            button(ui, "GHOST", {.variant = ButtonVariant::Ghost, .id = "controls.b3"});
            button(ui, "DANGER", {.variant = ButtonVariant::Danger, .id = "controls.b5"});
            button(ui, "DISABLED", {.disabled = true, .id = "controls.b4"});
            (void)row;
        }
        text(ui, "press one — Material throws ink from the point you pressed, the others "
                 "change the surface",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    {
        auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 12.0f});
        sectionTitle(ui, "With an icon, and as a hyperlink", "widgets/button · icon · hyperlink");
        {
            auto row = ui.beginRow({.align = Align::Center, .gap = 10.0f});
            button(ui, input, "COMMIT", {.leading = Icon::GitCommitHorizontal,
                                         .id = "controls.b6"});
            button(ui, input, "PUSH", {.variant = ButtonVariant::Secondary,
                                       .leading = Icon::Upload, .id = "controls.b7"});
            // A real URL: following it hands the address to whatever the
            // desktop opens links with.
            (void)hyperlink(ui, input, "controls.docs", "Read the documentation",
                            {.href = "https://github.com/gitgusilva/gbui",
                             .trailing = Icon::ChevronRight});
            (void)row;
        }
        text(ui, "a hyperlink is an element too: it is a label that reports a follow, and it "
                 "takes the pointer and the focus ring of one",
             {.color = Token::TextMuted, .size = 11.0f, .overflow = TextOverflow::Wrap});
        (void)panel;
    }
    (void)model;
}

/** Components: each owns a layer of its own and state the application holds. */
void overlaysTab(Ui& ui, const Interaction& input, Model& model) {
    auto panel = beginPanel(ui, {.padding = Edges::all(16.0f), .gap = 12.0f});
    sectionTitle(ui, "Dialogs, menus and tooltips", "widgets/modal · menu · popover · tooltip");
    {
        auto row = ui.beginRow({.align = Align::Center, .gap = 10.0f});
        if (button(ui, input, "OPEN DIALOG",
                   {.leading = Icon::CircleAlert, .id = "controls.opendialog"});
            input.clicked("controls.opendialog")) {
            model.confirmOpen = true;
        }
        (void)row;
    }
    tooltip(ui, input, "controls.opendialog", "Opens a draggable modal on the Modal layer.");
    (void)panel;
}

NodeId buildScreen(Ui& ui, const Interaction& input, Model& model) {
    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.padding = Edges::all(20.0f);
    window.gap = 14.0f;
    auto root = ui.begin(window);

    text(ui, "gbui controls",
         {.color = Token::TextStrong, .weight = FontWeight::Bold, .size = 18.0f});
    text(ui, "Tab moves the keyboard. Space or Return activates. Arrows drive ranges — and "
             "the tab strip.",
         {.color = Token::TextMuted, .size = 12.0f});

    // The design switcher. Everything below is drawn by the same components;
    // only the palette and the `Design` handed to the builder change, which is
    // the whole point of showing them side by side.
    {
        auto row = ui.beginRow({.align = Align::Center, .gap = 10.0f});
        text(ui, "DESIGN", {.color = Token::TextMuted, .weight = FontWeight::SemiBold,
                            .size = 11.0f});
        {
            Style box;
            box.width = 190.0f;
            box.shrink = 0.0f;
            auto boxScope = ui.begin(box);
            const SelectResult chosen =
                select(ui, input, "controls.design", model.designNames, model.design,
                       model.designList, {.grow = 1.0f});
            if (chosen.chosen) model.design = *chosen.chosen;
            (void)boxScope;
        }
        text(ui, "FONT", {.color = Token::TextMuted, .weight = FontWeight::SemiBold,
                          .size = 11.0f});
        {
            Style box;
            box.width = 170.0f;
            box.shrink = 0.0f;
            auto boxScope = ui.begin(box);
            const SelectResult chosen =
                select(ui, input, "controls.font", model.fontNames, model.fontFamily,
                       model.fontList, {.grow = 1.0f});
            if (chosen.chosen) model.fontFamily = *chosen.chosen;
            (void)boxScope;
        }
        {
            Style stepper;
            stepper.width = 118.0f;
            stepper.shrink = 0.0f;
            auto stepperScope = ui.begin(stepper);
            const auto result = numberField(ui, input, "controls.uisize", model.fontSize,
                                            {.minimum = 9, .maximum = 22, .step = 1,
                                             .suffix = " px"});
            if (result.changed) model.fontSize = result.value;
            (void)stepperScope;
        }
        {
            Style spacer;
            spacer.grow = 1.0f;
            ui.add(spacer);
        }
        if (checkbox(ui, input, "controls.lightmode", model.lightMode, {.label = "Light"})) {
            model.lightMode = !model.lightMode;
        }
        (void)row;
    }

    // The strip owns nothing: it is handed the index and reports the new one,
    // like every other component here.
    // Ordered by *what a thing is*, not by what it is for.
    //
    // An element draws one thing and reports one interaction; it owns no state
    // and composes nothing. A component is built out of elements, owns state
    // the application has to hold for it, and decides its own layout. The line
    // matters to a reader deciding what to reach for: an element is a leaf you
    // can put anywhere, a component is a commitment.
    //
    // The chip beside each panel's heading names the header it comes from, so
    // the demo also answers "where does this live".
    const std::vector<TabItem> pages{
        {.label = "TEXT", .icon = Icon::Heading, .group = "ELEMENTS"},
        {.label = "TOGGLES", .icon = Icon::Check, .group = "ELEMENTS"},
        {.label = "VALUES", .icon = Icon::RotateCcw, .group = "ELEMENTS"},
        {.label = "BUTTONS", .icon = Icon::Package, .group = "ELEMENTS"},

        {.label = "LISTS", .icon = Icon::List, .group = "COMPONENTS"},
        {.label = "TABLE", .icon = Icon::Archive, .group = "COMPONENTS"},
        {.label = "PICKERS", .icon = Icon::ClockFading, .group = "COMPONENTS"},
        {.label = "CHARTS", .icon = Icon::ChartPie, .group = "COMPONENTS"},
        {.label = "EDITOR", .icon = Icon::File, .group = "COMPONENTS"},
        {.label = "OVERLAYS", .icon = Icon::CircleAlert, .group = "COMPONENTS"},

        {.label = "MOTION", .icon = Icon::ClockFading, .group = "SYSTEM"},
    };
    {
        // The strip down the side and the page beside it: the same component as
        // a horizontal strip, told which way to run.
        Style body;
        body.direction = Direction::Row;
        body.gap = 20.0f;
        body.grow = 1.0f;
        body.basis = 0.0f;
        auto bodyScope = ui.begin(body);

        if (const auto chosen =
                tabs(ui, input, "controls.tabs", pages, model.tab,
                     {.orientation = TabsOrientation::Vertical, .thickness = 168.0f})) {
            model.tab = *chosen;
        }

        // The page scrolls rather than squashing.
        //
        // A column that only grows hands its children whatever is left, and
        // when that is less than they need they shrink into each other — which
        // is what a window too short for the Pickers page was doing. A scroll
        // view keeps them at their natural height and moves them instead.
        Style page;
        page.direction = Direction::Column;
        page.grow = 1.0f;
        page.basis = 0.0f;
        auto pageScope = ui.begin(page);
        auto pageScroll = beginScroll(ui, input, "controls.page", model.page,
                                      {.padding = Edges{0.0f, 6.0f, 0.0f, 0.0f}, .gap = 16.0f});
        // `lazy`, which is not the default: a gallery has eleven pages and
        // only one is ever read, so building the other ten every frame buys
        // nothing. An application whose tabs need their off-screen geometry —
        // to measure, or to keep a tooltip anchored — leaves it off.
        tabPanels(ui, model.tab,
                  {[&](Ui& ui) { textTab(ui, input, model); },
                   [&](Ui& ui) { togglesTab(ui, input, model); },
                   [&](Ui& ui) { numbersTab(ui, input, model); },
                   [&](Ui& ui) { buttonsTab(ui, input, model); },
                   [&](Ui& ui) { listsTab(ui, input, model); },
                   [&](Ui& ui) { tableTab(ui, input, model); },
                   [&](Ui& ui) { pickersTab(ui, input, model); },
                   [&](Ui& ui) { chartsTab(ui, input, model); },
                   [&](Ui& ui) { editorTab(ui, input, model); },
                   [&](Ui& ui) { overlaysTab(ui, input, model); },
                   [&](Ui& ui) { motionTab(ui, input, model); }},
                  {.lazy = true});
        (void)pageScroll;
        (void)pageScope;
        (void)bodyScope;
    }

    // ---- footer --------------------------------------------------------
    {
        Style footer;
        footer.direction = Direction::Row;
        footer.align = Align::Center;
        footer.gap = 10.0f;
        auto scope = ui.begin(footer);
        const std::string summary = "merge=" + std::to_string(model.mergeStyle) +
                                    "  tags=" + (model.showTags ? "on" : "off") +
                                    "  name=" + model.name.text;
        // A live readout of the model, so the state the controls above are
        // editing is visible without opening anything.
        text(ui, summary,
             {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.0f, .grow = 1.0f});
        (void)scope;
    }

    // The modal is built last so it is written after everything it covers —
    // though the layer, not the order, is what actually puts it on top.
    if (model.confirmOpen) {
        auto dialog = beginModal(ui, input, "controls.confirm", "Discard all changes?",
                                 model.confirmAt,
                                 {.width = 380.0f, .icon = Icon::CircleAlert, .danger = true});
        model.confirmAt = dialog.result.position;
        if (dialog.result.dismissed) model.confirmOpen = false;

        {
            Style body;
            body.direction = Direction::Column;
            body.padding = Edges::all(16.0f);
            body.gap = 8.0f;
            auto bodyScope = ui.begin(body);
            // One sentence, wrapped to the dialog. It used to be two `text`
            // calls split by hand at the width the dialog happened to be.
            text(ui, "This cannot be undone. Every modified file in the working tree goes "
                     "back to what it was at the last commit.",
                 {.color = Token::Text, .overflow = TextOverflow::Wrap, .lineHeight = 1.5f});
            text(ui, "Drag this dialog by its header.",
                 {.color = Token::TextMuted, .size = 11.0f});
            (void)bodyScope;
        }
        {
            auto actions = beginModalActions(ui);
            if (button(ui, "CANCEL", {.variant = ButtonVariant::Secondary,
                                      .id = "controls.confirm.cancel"});
                input.clicked("controls.confirm.cancel")) {
                model.confirmOpen = false;
            }
            button(ui, "DISCARD", {.variant = ButtonVariant::Danger,
                                   .id = "controls.confirm.ok"});
            if (input.clicked("controls.confirm.ok")) model.confirmOpen = false;
            (void)actions;
        }
        (void)dialog.body;
    }

    return root.id();
}

bool writePpm(const Canvas& canvas, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";
    const std::uint8_t* pixels = canvas.pixels();
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * canvas.pitch() + static_cast<std::size_t>(x) * 4;
            out.put(static_cast<char>(pixels[index]));
            out.put(static_cast<char>(pixels[index + 1]));
            out.put(static_cast<char>(pixels[index + 2]));
        }
    }
    return true;
}

/** The palette and the shape rules the model currently asks for. */
Theme themeFor(const Model& model) {
    const DesignChoice& choice = kDesigns[std::min(model.design, kDesignCount - 1)];
    Theme theme = choice.theme ? choice.theme(!model.lightMode)
                               : (model.lightMode ? Theme::light() : Theme::dark());

    // The type controls are live: the family and the size come from the model
    // and go into the theme, which is the whole of "configure the font" — every
    // run in the tree resolves through here.
    Typography& type = theme.typography();
    if (model.fontFamily > 0 && model.fontFamily < model.fontNames.size()) {
        type.uiFont = model.fontNames[model.fontFamily];
    }
    type.uiFontSize = static_cast<float>(model.fontSize);
    type.editorFontSize = static_cast<float>(model.fontSize) - 1.0f;
    return theme;
}

Design designFor(const Model& model) {
    return kDesigns[std::min(model.design, kDesignCount - 1)].design();
}

NodeId drawFrame(float scale, Canvas& canvas, FontDatabase& fonts, const Theme& theme,
                 Arena& arena,
                 const Interaction& input, Model& model, Animator* animator = nullptr) {
    arena.reset();
    Ui ui(arena);
    ui.setDesign(designFor(model));
    // The builder measures too: a caret sits at a byte offset inside a run, and
    // only the same function layout uses can say where that is on screen.
    ui.setMeasure(measureWith(fonts, scale), theme.typography());
    // The animator outlives the arena, so it is handed over rather than owned.
    ui.setAnimator(animator);
    const NodeId root = buildScreen(ui, input, model);

    LayoutContext context;
    context.theme = &theme;
    context.measure = measureWith(fonts, scale);
    layout(arena, root,
           Rect{0, 0, static_cast<float>(canvas.width()) / scale,
                static_cast<float>(canvas.height()) / scale},
           context);

    DisplayList list;
    // The one place logical units become device pixels.
    list.setScale(scale);
    list.reserve(arena.size() * 2);
    record(arena, root, theme, list, context.measure);

    canvas.clear(theme.color(Token::Bg));
    SoftwarePainter painter(canvas, fonts, theme.typography());
    painter.paint(list);
    return root;
}

}  // namespace

int main(int argc, char** argv) {
    std::string shotPath;
    bool openList = false;
    bool openModal = false;
    int page = 0;
    int design = 0;
    bool lightMode = false;
    int tabs = 0;
    Vec2 pointerAt{-1.0f, -1.0f};
    int shotWidth = 900;
    int shotHeight = 780;
    double fontSize = 0.0;
    double shotScale = 1.0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shotPath = argv[++i];
        // Opening the overlays for an offscreen render, so a screenshot covers
        // the parts that normally need a pointer.
        else if (std::strcmp(argv[i], "--open-list") == 0) openList = true;
        else if (std::strcmp(argv[i], "--open-modal") == 0) openModal = true;
        else if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc) page = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--design") == 0 && i + 1 < argc) design = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--light") == 0) lightMode = true;
        else if (std::strcmp(argv[i], "--font-size") == 0 && i + 1 < argc) fontSize = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc) shotScale = std::atof(argv[++i]);
        // Focus rings only appear for the keyboard, so an offscreen render has
        // to press Tab to see one.
        else if (std::strcmp(argv[i], "--tab") == 0 && i + 1 < argc) tabs = std::atoi(argv[++i]);
        // A tall shot is how a long page gets verified without a scrollbar in
        // the way — the page is as long as it is, and the window is what clips.
        // Parks the pointer somewhere for the shot, which is the only way to
        // photograph a hover state.
        else if (std::strcmp(argv[i], "--pointer") == 0 && i + 2 < argc) {
            pointerAt = {static_cast<float>(std::atof(argv[i + 1])),
                         static_cast<float>(std::atof(argv[i + 2]))};
            i += 2;
        }
        else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            shotWidth = std::atoi(argv[++i]);
            shotHeight = std::atoi(argv[++i]);
        }
    }

    Theme theme = Theme::dark();
    FontDatabase fonts;
    Arena arena;
    arena.reserve(512);
    Model model;
    Interaction interaction;

    if (!shotPath.empty()) {
        model.branchList.open = openList;
        model.confirmOpen = openModal;
        model.tab = static_cast<std::size_t>(std::max(0, page));
        model.design = static_cast<std::size_t>(std::clamp(design, 0, static_cast<int>(kDesignCount) - 1));
        model.lightMode = lightMode;
        if (fontSize > 0.0) model.fontSize = fontSize;
        theme = themeFor(model);
        const auto shot = static_cast<float>(shotScale);
        // The canvas is in device pixels; the tree is laid out in logical ones.
        Canvas canvas(static_cast<int>(static_cast<double>(shotWidth) * shot),
                      static_cast<int>(static_cast<double>(shotHeight) * shot));
        // Two frames, with the interaction resolved in between: the first lays
        // the controls out, the update records where everything landed, and the
        // second draws the parts that need to know — a slider's knob sits at a
        // point on a track whose width only exists after layout.
        InputFrame resting;
        if (pointerAt.x >= 0.0f) resting.pointer = pointerAt;
        NodeId root = drawFrame(shot, canvas, fonts, theme, arena, interaction, model);
        interaction.update(arena, root, resting);
        // Each Tab is a frame of its own: focus moves against the tree that was
        // on screen when the key was pressed, exactly as it does in the loop.
        for (int i = 0; i < tabs; ++i) {
            InputFrame frame;
            frame.keys.push_back({Key::Tab});
            if (pointerAt.x >= 0.0f) frame.pointer = pointerAt;
            root = drawFrame(shot, canvas, fonts, theme, arena, interaction, model);
            interaction.update(arena, root, frame);
        }
        // Settle before photographing.
        //
        // Every widget here places itself from *last* frame's geometry, so a
        // freshly opened popup needs several frames before its size stops
        // changing — and a screenshot taken too early shows a layout that never
        // appears on screen. Six is well past what anything here takes; the
        // running loop reaches the same state in a few milliseconds, which is
        // why this only ever misleads in a shot.
        for (int settle = 0; settle < 6; ++settle) {
            root = drawFrame(shot, canvas, fonts, theme, arena, interaction, model);
            interaction.update(arena, root, resting);
        }
        drawFrame(shot, canvas, fonts, theme, arena, interaction, model);
        if (!writePpm(canvas, shotPath)) return 1;
        std::printf("wrote %s\n", shotPath.c_str());
        return 0;
    }

    auto window = Window::create({.title = "gbui — controls", .width = 900, .height = 780});
    if (!window) {
        std::fprintf(stderr, "gbui_controls: no window backend\n");
        return 1;
    }

    NodeId root;
    Animator animator;
    auto previous = std::chrono::steady_clock::now();
    const auto started = previous;
    while (window->pumpEvents()) {
        // The clock advances once, before anything is built, so every component
        // this frame reads the same instant.
        const auto frameAt = std::chrono::steady_clock::now();
        const float delta = std::chrono::duration<float>(frameAt - previous).count();
        animator.tick(delta);
        previous = frameAt;

        // A rolling window of real frame times, which is what the charts page
        // plots — the toolkit measuring itself.
        if (!model.fpsPaused) {
            model.frameTimes.erase(model.frameTimes.begin());
            model.frameTimes.push_back(static_cast<double>(delta) * 1000.0);
        }

        const InputFrame input = window->takeInput();
        interaction.update(arena, root, input);
        // The node under the pointer decides the shape; the loop only forwards
        // what it decided.
        window->setCursor(interaction.cursor());

        bool quit = false;
        for (const KeyEvent& event : interaction.keys()) {
            if (event.key == Key::Escape && interaction.focused().empty()) quit = true;
        }
        if (quit) break;

        // An indeterminate bar needs a clock; everything else redraws on
        // change. Until the toolkit has an animation clock of its own, the
        // application supplies one.
        const auto now = std::chrono::steady_clock::now();
        model.clock = std::chrono::duration<float>(now - started).count() * 0.6f;

        theme = themeFor(model);
        const auto drawBegan = std::chrono::steady_clock::now();
        root = drawFrame(window->scale(), window->canvas(), fonts, theme, arena, interaction, model,
                         &animator);
        const auto drawEnded = std::chrono::steady_clock::now();
        if (!model.fpsPaused) {
            model.drawTimes.erase(model.drawTimes.begin());
            model.drawTimes.push_back(
                std::chrono::duration<double, std::milli>(drawEnded - drawBegan).count());
        }

        // No sleep here on purpose. The display paces the loop — `present`
        // returns when the frame has been shown — so the only thing a sleep
        // could add is a delay the compositor then rounds up to a whole refresh.
        window->present();
    }
    return 0;
}
