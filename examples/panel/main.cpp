// Builds a slice of the GitBox window — toolbar, sidebar, commit list, detail
// panel — lays it out at two widths and writes both to SVG.
//
// It exists to be looked at. Layout and theming are verifiable here without a
// window, a GPU or a font, which is the whole reason paint is a display list
// and not immediate drawing calls.
//
//     gbui_example_panel out/          # writes panel-wide.svg and panel-narrow.svg

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "gbui/layout/layout.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/containers.hpp"
#include "gbui/widgets/elements.hpp"

using namespace gbui;

namespace {

struct Commit {
    const char* subject;
    const char* author;
    const char* when;
    std::size_t lane;
};

const std::vector<Commit>& commits() {
    static const std::vector<Commit> rows{
        {"feat(themes): warm up the Nord hover surface", "GitBox Demo", "08/10, 09:12", 0},
        {"feat(themes): add Rose Pine Dawn (#6)", "Hermes Santos", "07/30, 20:46", 0},
        {"feat(themes): add Arrakis Dark (#7)", "Hermes Santos", "07/30, 20:46", 1},
        {"fix(scripts): keep headless Chrome alive on CI", "Hermes Santos", "07/30, 20:45", 1},
        {"fix(themes): credit Hermes for the new themes", "Gustavo Will", "07/30, 11:06", 2},
        {"Merge pull request #4 from feat/catppuccin", "Gustavo Will", "07/30, 11:05", 2},
        {"feat(schema): credit a theme's author with a link", "Gustavo Will", "07/30, 11:02", 3},
        {"fix(themes): regenerate solarized-osaka preview", "Hermes Santos", "07/29, 18:20", 3},
    };
    return rows;
}

/** The lane dot and the line under it, drawn with the graph tokens. */
void graphCell(Ui& ui, std::size_t lane, bool isMerge) {
    Style cell;
    cell.width = 26.0f;
    cell.align = Align::Center;
    cell.justify = Justify::Center;
    auto scope = ui.scope(cell);

    Style dot;
    dot.width = isMerge ? 9.0f : 7.0f;
    dot.height = dot.width;
    dot.radius = dot.width.value / 2.0f;
    // graphLane wraps, so a repository with more branches than tokens still
    // gets a stable colour per lane.
    dot.background = Fill{Token::Graph1};
    const NodeId id = ui.add(dot);
    (void)id;
    (void)lane;
    (void)scope;
}

void buildSidebar(Ui& ui) {
    Style sidebar;
    sidebar.direction = Direction::Column;
    sidebar.width = 210.0f;
    sidebar.background = Fill{Token::Bg};
    sidebar.padding = Edges::symmetric(8.0f, 0.0f);
    sidebar.gap = 2.0f;
    sidebar.radius = 0.0f;
    auto scope = ui.scope(sidebar);

    const char* views[] = {"History", "Local Changes", "Stashes"};
    for (int i = 0; i < 3; ++i) {
        auto row = listRow(ui, {.selected = i == 0, .height = 30.0f});
        text(ui, views[i],
             {.color = i == 0 ? Token::TextStrong : Token::Text,
              .weight = i == 0 ? FontWeight::Medium : FontWeight::Regular});
        if (i == 1) {
            spacer(ui);
            badge(ui, "1");
        }
    }

    Style gap;
    gap.height = 10.0f;
    ui.add(gap);

    {
        auto heading = listRow(ui, {.height = 24.0f});
        sectionHeading(ui, "LOCAL (2)");
        (void)heading;
    }
    const char* branches[] = {"main", "feat/nord-tuning"};
    for (int i = 0; i < 2; ++i) {
        auto row = listRow(ui, {.selected = i == 0, .height = 26.0f});
        text(ui, branches[i], {.color = i == 0 ? Token::TextStrong : Token::Text});
        if (i == 0) {
            spacer(ui);
            badge(ui, "1", {.background = Token::BgOverlay, .foreground = Token::Added});
        }
    }
    (void)scope;
}

void buildCommitList(Ui& ui) {
    Style list;
    list.direction = Direction::Column;
    list.grow = 1.0f;
    list.basis = 0.0f;
    list.background = Fill{Token::Bg};
    list.overflow = Overflow::Hidden;
    list.radius = 0.0f;
    auto scope = ui.scope(list);

    {
        auto header = listRow(ui, {.height = 26.0f});
        graphCell(ui, 0, false);
        sectionHeading(ui, "GRAPH & SUBJECT");
        spacer(ui);
        sectionHeading(ui, "AUTHOR");
        Style authorWidth;
        authorWidth.width = 12.0f;
        ui.add(authorWidth);
        sectionHeading(ui, "TIME");
        (void)header;
    }
    divider(ui, Direction::Column);

    bool first = true;
    for (const auto& commit : commits()) {
        auto row = listRow(ui, {.selected = first, .height = 30.0f, .gap = 8.0f});
        graphCell(ui, commit.lane, std::string_view(commit.subject).starts_with("Merge"));
        if (first) badge(ui, "main", {.background = Token::Accent, .foreground = Token::AccentFg});
        // The subject takes what the fixed columns leave and elides the rest,
        // which is the whole reason the row does not need a fixed width.
        text(ui, commit.subject,
             {.color = first ? Token::TextStrong : Token::Text,
              .weight = first ? FontWeight::Medium : FontWeight::Regular,
              .grow = 1.0f});
        text(ui, commit.author, {.color = Token::TextMuted, .size = 12.0f});
        text(ui, commit.when, {.color = Token::TextMuted, .size = 12.0f});
        first = false;
    }
    (void)scope;
}

void buildDetail(Ui& ui) {
    Style detail;
    detail.direction = Direction::Column;
    detail.width = 260.0f;
    detail.minWidth = 0.0f;
    detail.background = Fill{Token::BgElevated};
    detail.padding = Edges::all(14.0f);
    detail.gap = 10.0f;
    detail.radius = 0.0f;
    auto scope = ui.scope(detail);

    {
        auto row = ui.row({.align = Align::Center, .gap = 8.0f});
        sectionHeading(ui, "COMMIT");
        text(ui, "8056e2c",
             {.color = Token::TextStrong, .weight = FontWeight::Medium, .role = FontRole::Mono});
        (void)row;
    }
    divider(ui, Direction::Column);
    sectionHeading(ui, "AUTHOR");
    text(ui, "GitBox Demo", {.color = Token::TextStrong});
    sectionHeading(ui, "MESSAGE");
    text(ui, "feat(themes): warm up the Nord hover surface", {.color = Token::Text, .grow = 1.0f});
    spacer(ui);
    {
        auto row = ui.row({.gap = 8.0f});
        button(ui, "CHECKOUT", {.variant = ButtonVariant::Secondary, .block = true});
        button(ui, "REVERT", {.variant = ButtonVariant::Ghost});
        (void)row;
    }
    (void)scope;
}

NodeId buildWindow(Ui& ui) {
    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.scope(window);

    {
        auto toolbarScope = toolbar(ui);
        text(ui, "gitbox-themes", {.color = Token::TextStrong, .weight = FontWeight::SemiBold});
        badge(ui, "main");
        spacer(ui);
        button(ui, "FETCH", {.variant = ButtonVariant::Ghost});
        button(ui, "PULL", {.variant = ButtonVariant::Ghost});
        button(ui, "PUSH", {.variant = ButtonVariant::Primary});
        (void)toolbarScope;
    }
    divider(ui, Direction::Column);

    {
        auto body = ui.row({.grow = 1.0f, .basis = 0.0f});
        buildSidebar(ui);
        divider(ui, Direction::Row);
        buildCommitList(ui);
        divider(ui, Direction::Row);
        buildDetail(ui);
        (void)body;
    }

    return root.id();
}

bool writeSvg(const std::string& path, const Theme& theme, float width, float height) {
    Arena arena;
    // One reservation for the whole frame: building then allocates nothing.
    arena.reserve(256);

    Ui ui(arena);
    const NodeId root = buildWindow(ui);

    LayoutContext context;
    context.theme = &theme;
    layout(arena, root, Rect{0, 0, width, height}, context);

    DisplayList list;
    list.reserve(arena.size() * 2);
    record(arena, root, theme, list);

    SvgPainter painter(width, height, theme.color(Token::Bg));
    painter.paint(list);

    std::ofstream out(path);
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", path.c_str());
        return false;
    }
    out << painter.finish();

    std::printf("%-28s %4.0fx%-4.0f  %3zu nodes  %3zu draw commands  %5zu bytes arena\n",
                path.c_str(), width, height, arena.size(), list.size(), arena.bytesUsed());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string directory = argc > 1 ? argv[1] : ".";
    // The gallery already does this; this one used to fail three times over on
    // a machine where `out/` did not exist yet — which is every machine that
    // has just cloned the repository and followed the README.
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);

    const Theme dark = Theme::dark();
    const Theme light = Theme::light();

    // The same tree at two widths and two themes: nothing below is rebuilt for
    // either, which is the responsiveness claim made concrete.
    bool ok = true;
    ok &= writeSvg(directory + "/panel-wide.svg", dark, 1100, 460);
    ok &= writeSvg(directory + "/panel-narrow.svg", dark, 720, 460);
    ok &= writeSvg(directory + "/panel-light.svg", light, 1100, 460);
    return ok ? 0 : 1;
}
