// The screen every example draws: a slice of GitBox built from the component
// set. It is shared so that the windowed application and the SVG sweeps are
// provably the same UI — if the gallery looks right, the app does too.
//
// Header-only and inside `gbui::examples`, because an example is not part of
// the library's API.
#pragma once

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/layout/layout.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/components.hpp"

namespace gbui::examples {

constexpr float kScreenHeight = 620.0f;

/**
 * What the screen needs to know about the pointer.
 *
 * The hovered row is named, not pointed at: the tree is rebuilt from scratch
 * every frame, so a NodeId from the last one means nothing. A tag survives
 * because it is what the row is, rather than where it happened to be.
 */
struct ScreenState {
    std::string_view hovered{};
};

enum class FileStatus { Modified, Added, Deleted, Untracked };

struct StatusLook {
    Token color;
    std::string_view letter;
};

inline StatusLook lookFor(FileStatus status) {
    switch (status) {
        case FileStatus::Added: return {Token::Added, "A"};
        case FileStatus::Deleted: return {Token::Removed, "D"};
        case FileStatus::Untracked: return {Token::TextMuted, "?"};
        case FileStatus::Modified: break;
    }
    return {Token::Modified, "M"};
}

/** The square status marker beside a path in the changes list. */
inline NodeId statusPill(Ui& ui, FileStatus status, float size = 18.0f) {
    const StatusLook look = lookFor(status);

    Style style;
    style.width = size;
    style.height = size;
    style.justify = Justify::Center;
    style.align = Align::Center;
    style.radius = 4.0f;
    // The letter on a wash of its own colour reads at a glance without needing
    // a second token per status.
    style.background = Fill{look.color, 0.20f};
    style.shrink = 0.0f;  // a fixed marker never gives up its width

    auto scope = ui.scope(style);
    text(ui, look.letter,
         {.color = look.color, .weight = FontWeight::SemiBold, .size = 11.0f});
    return scope.id();
}

/** A text field. Not interactive — this is the resting state of an input. */
inline NodeId field(Ui& ui, std::string_view placeholder, float height = 32.0f,
                    std::optional<Icon> leading = std::nullopt) {
    Style style;
    style.height = height;
    style.align = Align::Center;
    style.padding = Edges::symmetric(0.0f, 10.0f);
    style.background = Fill{Token::Bg};
    style.border = Border{1.0f, Fill{Token::BorderStrong}};

    auto scope = ui.scope(style);
    if (leading) {
        icon(ui, *leading, {.color = Token::TextMuted, .size = 14.0f});
        Style spacing;
        spacing.width = 6.0f;
        spacing.shrink = 0.0f;
        ui.add(spacing);
    }
    text(ui, placeholder, {.color = Token::TextMuted, .grow = 1.0f});
    return scope.id();
}

/** The lane dot of the commit graph, coloured by lane. */
inline void graphDot(Ui& ui, const Theme& theme, std::size_t lane, bool merge) {
    Style cell;
    cell.width = 24.0f;
    cell.shrink = 0.0f;
    cell.align = Align::Center;
    cell.justify = Justify::Center;
    auto scope = ui.scope(cell);

    Style dot;
    dot.width = merge ? 10.0f : 7.0f;
    dot.height = dot.width;
    dot.radius = dot.width.value / 2.0f;
    dot.background = Fill{theme.graphLane(lane)};
    ui.add(dot);
    (void)scope;
}

// ---------------------------------------------------------------------------
// The screen
// ---------------------------------------------------------------------------

inline void titleBar(Ui& ui) {
    Style bar;
    bar.direction = Direction::Row;
    bar.align = Align::Center;
    bar.height = 36.0f;
    bar.gap = 8.0f;
    bar.padding = Edges::symmetric(0.0f, 10.0f);
    bar.background = Fill{Token::BgElevated};
    bar.radius = 0.0f;
    auto scope = ui.scope(bar);

    badge(ui, "Default");

    // The active repository tab.
    Style tab;
    tab.direction = Direction::Row;
    tab.align = Align::Center;
    tab.gap = 8.0f;
    tab.height = 26.0f;
    tab.padding = Edges::symmetric(0.0f, 10.0f);
    tab.background = Fill{Token::BgOverlay};
    tab.shrink = 0.0f;
    {
        auto t = ui.scope(tab);
        Style dot;
        dot.width = 7.0f;
        dot.height = 7.0f;
        dot.radius = 3.5f;
        dot.background = Fill{Token::Accent};
        ui.add(dot);
        text(ui, "gitbox-themes", {.color = Token::TextStrong, .weight = FontWeight::Medium});
        text(ui, "x", {.color = Token::TextMuted});
        (void)t;
    }
    text(ui, "+", {.color = Token::TextMuted, .weight = FontWeight::Medium});
    spacer(ui);
    text(ui, "—   □   x", {.color = Token::TextMuted});
    (void)scope;
}

inline void toolBar(Ui& ui) {
    auto scope = toolbar(ui, {.height = 46.0f, .gap = 10.0f});

    {
        Style group;
        group.direction = Direction::Column;
        group.gap = 1.0f;
        group.shrink = 0.0f;
        auto g = ui.scope(group);
        text(ui, "repository", {.color = Token::TextMuted, .size = 10.0f});
        text(ui, "gitbox-themes v", {.color = Token::TextStrong, .weight = FontWeight::Medium});
        (void)g;
    }
    {
        Style group;
        group.direction = Direction::Column;
        group.gap = 1.0f;
        group.shrink = 0.0f;
        auto g = ui.scope(group);
        text(ui, "branch", {.color = Token::TextMuted, .size = 10.0f});
        text(ui, "main v", {.color = Token::TextStrong, .weight = FontWeight::Medium});
        (void)g;
    }

    spacer(ui);
    button(ui, "FETCH", {.variant = ButtonVariant::Ghost, .leading = Icon::RefreshCw});
    button(ui, "PULL", {.variant = ButtonVariant::Ghost, .leading = Icon::Download});
    button(ui, "PUSH", {.variant = ButtonVariant::Primary, .leading = Icon::Upload});
    divider(ui, Direction::Row);
    button(ui, "CREATE BRANCH",
           {.variant = ButtonVariant::Secondary, .leading = Icon::GitBranch});
    button(ui, "DISCARD", {.variant = ButtonVariant::Danger, .leading = Icon::RotateCcw});
    (void)scope;
}

inline void sideBar(Ui& ui, const ScreenState& state) {
    Style side;
    side.direction = Direction::Column;
    side.width = 220.0f;
    side.shrink = 0.0f;
    side.background = Fill{Token::Bg};
    side.padding = Edges::symmetric(8.0f, 0.0f);
    side.gap = 2.0f;
    side.radius = 0.0f;
    auto scope = ui.scope(side);

    struct View {
        const char* label;
        const char* count;
        Icon glyph;
    };
    const View views[] = {{"History", nullptr, Icon::ClockFading},
                          {"Local Changes", "3", Icon::FilePlus},
                          {"Stashes", nullptr, Icon::Archive}};
    for (int i = 0; i < 3; ++i) {
        const std::string tag = std::string("sidebar.view.") + views[i].label;
        auto row = listRow(ui, {.selected = i == 1,
                                     .hovered = state.hovered == tag,
                                     .height = 30.0f,
                                     .id = tag});
        icon(ui, views[i].glyph,
             {.color = i == 1 ? Token::TextStrong : Token::TextMuted, .size = 15.0f});
        text(ui, views[i].label,
             {.color = i == 1 ? Token::TextStrong : Token::Text,
              .weight = i == 1 ? FontWeight::Medium : FontWeight::Regular,
              .grow = 1.0f});
        if (views[i].count) badge(ui, views[i].count);
    }

    {
        Style pad;
        pad.height = 8.0f;
        pad.padding = Edges::symmetric(0.0f, 12.0f);
        auto p = ui.scope(pad);
        (void)p;
    }
    {
        Style wrap;
        wrap.padding = Edges::symmetric(0.0f, 12.0f);
        auto w = ui.scope(wrap);
        field(ui, "Search...", 28.0f, Icon::Search);
        (void)w;
    }
    {
        Style pad;
        pad.height = 8.0f;
        auto p = ui.scope(pad);
        (void)p;
    }

    struct Section {
        const char* label;
        std::vector<const char*> items;
    };
    const std::vector<Section> sections{
        {"LOCAL (2)", {"main", "feat/nord-tuning"}},
        {"REMOTES (1)", {"origin"}},
        {"TAGS (0)", {}},
        {"SUBMODULES (0)", {}},
    };

    for (const auto& section : sections) {
        {
            auto heading = listRow(ui, {.height = 24.0f});
            sectionHeading(ui, section.label);
            (void)heading;
        }
        for (const char* item : section.items) {
            const bool current = std::string_view(item) == "main";
            const std::string tag = std::string("sidebar.branch.") + item;
            auto row = listRow(ui, {.selected = current,
                                         .hovered = state.hovered == tag,
                                         .height = 26.0f,
                                         .id = tag});
            icon(ui, Icon::GitBranch,
                 {.color = current ? Token::Accent : Token::TextMuted, .size = 14.0f});
            text(ui, item,
                 {.color = current ? Token::TextStrong : Token::Text, .grow = 1.0f});
            if (current) {
                badge(ui, "1", {.background = Token::BgOverlay, .foreground = Token::Added});
            }
        }
    }
    (void)scope;
}

inline void changesPane(Ui& ui, const ScreenState& state) {
    Style pane;
    pane.direction = Direction::Column;
    pane.width = 300.0f;
    pane.shrink = 0.0f;
    pane.background = Fill{Token::Bg};
    pane.radius = 0.0f;
    auto scope = ui.scope(pane);

    struct Change {
        FileStatus status;
        const char* path;
    };
    const Change unstaged[] = {
        {FileStatus::Modified, "themes/nord/theme.json"},
        {FileStatus::Added, "themes/arrakis-dark/theme.json"},
        {FileStatus::Untracked, "scripts/preview.mjs"},
    };

    {
        auto header = listRow(ui, {.height = 28.0f});
        sectionHeading(ui, "UNSTAGED (3)");
        (void)header;
    }
    divider(ui, Direction::Column);
    for (int i = 0; i < 3; ++i) {
        const std::string tag = std::string("changes.file.") + unstaged[i].path;
        auto row = listRow(ui, {.selected = i == 0,
                                     .hovered = state.hovered == tag,
                                     .height = 28.0f,
                                     .gap = 8.0f,
                                     .id = tag});
        statusPill(ui, unstaged[i].status);
        text(ui, unstaged[i].path,
             {.color = i == 0 ? Token::TextStrong : Token::Text, .grow = 1.0f});
    }

    {
        Style pad;
        pad.height = 10.0f;
        auto p = ui.scope(pad);
        (void)p;
    }
    {
        auto header = listRow(ui, {.height = 28.0f});
        sectionHeading(ui, "STAGED (1)");
        (void)header;
    }
    divider(ui, Direction::Column);
    {
        auto row = listRow(ui, {.height = 28.0f, .gap = 8.0f});
        statusPill(ui, FileStatus::Deleted);
        text(ui, "themes/legacy/theme.json", {.grow = 1.0f});
    }

    spacer(ui);
    divider(ui, Direction::Column);

    {
        Style commitBox;
        commitBox.direction = Direction::Column;
        commitBox.gap = 8.0f;
        commitBox.padding = Edges::all(10.0f);
        commitBox.background = Fill{Token::BgElevated};
        commitBox.radius = 0.0f;
        auto box = ui.scope(commitBox);
        sectionHeading(ui, "COMMIT MESSAGE");
        field(ui, "Summary (required)");
        button(ui, "COMMIT", {.variant = ButtonVariant::Primary,
                              .leading = Icon::GitCommitHorizontal,
                              .block = true,
                              .height = 32.0f});
        (void)box;
    }
    (void)scope;
}

inline void diffPane(Ui& ui, const Theme& theme, const ScreenState& state) {
    Style pane;
    pane.direction = Direction::Column;
    pane.grow = 1.0f;
    pane.basis = 0.0f;
    pane.background = Fill{Token::Bg};
    pane.overflow = Overflow::Hidden;
    pane.radius = 0.0f;
    auto scope = ui.scope(pane);

    {
        auto header = listRow(ui, {.height = 30.0f, .gap = 10.0f});
        sectionHeading(ui, "THEMES/NORD/THEME.JSON");
        spacer(ui);
        badge(ui, "FILE");
        badge(ui, "DIFF", {.background = Token::Accent, .foreground = Token::AccentFg});
        (void)header;
    }
    divider(ui, Direction::Column);

    struct Line {
        char kind;
        const char* text;
    };
    const Line lines[] = {
        {' ', "  \"meta\": {"},
        {'-', "    \"version\": \"1.0.1\","},
        {'+', "    \"version\": \"1.1.0\","},
        {' ', "    \"author\": \"GitBox\","},
        {'-', "    \"description\": \"An arctic, north-bluish palette.\""},
        {'+', "    \"description\": \"An arctic palette, tuned for long sessions.\""},
        {' ', "  },"},
        {' ', "  \"colors\": {"},
        {' ', "    \"bg\": \"#2E3440\","},
        {'-', "    \"surfaceHover\": \"#4C566A\","},
        {'+', "    \"surfaceHover\": \"#48536B\","},
        {' ', "    \"border\": \"#3B4252\","},
    };

    int number = 5;
    for (const auto& line : lines) {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.height = 19.0f;
        row.gap = 10.0f;
        row.padding = Edges::symmetric(0.0f, 10.0f);
        row.radius = 0.0f;
        if (line.kind == '+') row.background = Fill{Token::Added, 0.16f};
        if (line.kind == '-') row.background = Fill{Token::Removed, 0.16f};
        auto r = ui.scope(row);

        Style gutter;
        gutter.width = 26.0f;
        gutter.shrink = 0.0f;
        gutter.justify = Justify::End;
        {
            auto g = ui.scope(gutter);
            text(ui, std::to_string(number++),
                 {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.0f});
            (void)g;
        }
        const Token colour = line.kind == '+'   ? Token::Added
                             : line.kind == '-' ? Token::Removed
                                                : Token::Text;
        text(ui, line.text, {.color = colour, .role = FontRole::Mono, .size = 12.0f, .grow = 1.0f});
        (void)r;
    }

    divider(ui, Direction::Column);
    {
        auto header = listRow(ui, {.height = 28.0f});
        sectionHeading(ui, "HISTORY");
        (void)header;
    }
    struct Row {
        const char* subject;
        const char* author;
        std::size_t lane;
        bool merge;
    };
    const Row history[] = {
        {"feat(themes): warm up the Nord hover surface", "GitBox Demo", 0, false},
        {"Merge pull request #4 from feat/catppuccin", "Gustavo Will", 1, true},
        {"feat(themes): add Arrakis Dark (#7)", "Hermes Santos", 2, false},
        {"fix(scripts): keep headless Chrome alive on CI", "Hermes Santos", 3, false},
    };
    for (const auto& row : history) {
        const std::string tag = std::string("history.commit.") + row.subject;
        auto r = listRow(ui, {.hovered = state.hovered == tag,
                                   .height = 26.0f,
                                   .gap = 8.0f,
                                   .id = tag});
        graphDot(ui, theme, row.lane, row.merge);
        if (row.merge) icon(ui, Icon::GitMerge, {.color = Token::TextMuted, .size = 13.0f});
        text(ui, row.subject, {.grow = 1.0f});
        text(ui, row.author, {.color = Token::TextMuted, .size = 11.0f});
    }
    (void)scope;
}

inline void statusBar(Ui& ui) {
    Style bar;
    bar.direction = Direction::Row;
    bar.align = Align::Center;
    bar.height = 26.0f;
    bar.gap = 14.0f;
    bar.padding = Edges::symmetric(0.0f, 10.0f);
    bar.background = Fill{Token::BgElevated};
    bar.radius = 0.0f;
    auto scope = ui.scope(bar);

    icon(ui, Icon::Terminal, {.color = Token::TextMuted, .size = 13.0f});
    text(ui, "Command Log", {.color = Token::TextMuted, .size = 11.0f});
    icon(ui, Icon::ChartPie, {.color = Token::TextMuted, .size = 13.0f});
    text(ui, "Statistics", {.color = Token::TextMuted, .size = 11.0f});
    spacer(ui);
    text(ui, "100%", {.color = Token::TextMuted, .size = 11.0f});
    text(ui, "GITBOX v1.3.2", {.color = Token::TextMuted, .size = 11.0f});
    (void)scope;
}

inline NodeId buildScreen(Ui& ui, const Theme& theme, const ScreenState& state = {}) {
    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.scope(window);

    titleBar(ui);
    toolBar(ui);
    divider(ui, Direction::Column);
    {
        auto body = ui.row({.grow = 1.0f, .basis = 0.0f});
        sideBar(ui, state);
        divider(ui, Direction::Row);
        changesPane(ui, state);
        divider(ui, Direction::Row);
        diffPane(ui, theme, state);
        (void)body;
    }
    divider(ui, Direction::Column);
    statusBar(ui);
    return root.id();
}

}  // namespace gbui::examples
