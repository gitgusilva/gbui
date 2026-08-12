// gbui_gallery — the shared example screen, rendered in every theme it finds,
// at four widths, with an index.html to flip through them.
//
//     gbui_gallery out/ ../gitbox-themes/themes
//
// It exists for review without a window: a layout regression or a theme with a
// bad token shows up here in one glance across 44 images.

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gbui/layout/layout.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "screen.hpp"

using namespace gbui;
using gbui::examples::buildScreen;
using gbui::examples::kScreenHeight;

namespace {

constexpr float kHeight = kScreenHeight;
/** The widths every theme is rendered at. A layout that survives all four is
 *  the whole claim of the responsiveness milestone. */
constexpr float kWidths[] = {1600.0f, 1180.0f, 900.0f, 680.0f};

// ---------------------------------------------------------------------------
// Rendering the same screen once per theme
// ---------------------------------------------------------------------------

struct Rendered {
    std::string id;
    std::string name;
    bool dark = true;
};

/** One theme, one width. The tree is rebuilt per render — which costs nothing
 *  worth measuring and proves the layout is a function of the viewport, not of
 *  something cached from the last one. */
void renderOne(const std::filesystem::path& directory, const Theme& theme, float width) {
    Arena arena;
    arena.reserve(512);
    Ui ui(arena);
    const NodeId root = buildScreen(ui, theme);

    LayoutContext context;
    context.theme = &theme;
    layout(arena, root, Rect{0, 0, width, kHeight}, context);

    DisplayList list;
    list.reserve(arena.size() * 2);
    record(arena, root, theme, list);

    SvgPainter painter(width, kHeight, theme.color(Token::Bg));
    painter.paint(list);

    char name[128];
    std::snprintf(name, sizeof(name), "%s-%.0f.svg", theme.id().c_str(), width);
    std::ofstream out(directory / name);
    out << painter.finish();
}

Rendered renderTheme(const std::filesystem::path& directory, const Theme& theme) {
    for (const float width : kWidths) renderOne(directory, theme, width);
    std::printf("  %-18s %-20s %zu widths\n", theme.id().c_str(), theme.name().c_str(),
                std::size(kWidths));
    return {theme.id(), theme.name(), theme.isDark()};
}

void writeIndex(const std::filesystem::path& directory, const std::vector<Rendered>& themes) {
    std::string themeButtons;
    for (const auto& theme : themes) {
        themeButtons +=
            "      <button data-theme=\"" + theme.id + "\">" + theme.name + "</button>\n";
    }
    std::string widthButtons;
    for (const float width : kWidths) {
        char entry[128];
        std::snprintf(entry, sizeof(entry), "      <button data-width=\"%.0f\">%.0f px</button>\n",
                      width, width);
        widthButtons += entry;
    }

    std::ofstream out(directory / "index.html");
    out << R"(<!doctype html>
<meta charset="utf-8">
<title>gbui — the same screen in every theme</title>
<style>
  :root { color-scheme: dark light; }
  body { margin: 0; font: 14px/1.5 system-ui, sans-serif; background: #16161a; color: #ddd; }
  header { padding: 18px 24px 12px; }
  h1 { margin: 0 0 4px; font-size: 16px; font-weight: 600; }
  p { margin: 0; color: #999; font-size: 13px; }
  nav { display: flex; flex-wrap: wrap; gap: 6px; padding: 14px 24px; }
  button { font: inherit; padding: 6px 12px; border-radius: 6px; cursor: pointer;
           border: 1px solid #333; background: #202028; color: #ddd; }
  button:hover { background: #2a2a33; }
  button[aria-current="true"] { background: #2563eb; border-color: #2563eb; color: #fff; }
  main { padding: 0 24px 32px; }
  img { display: block; width: 100%; border: 1px solid #2a2a33; border-radius: 8px; }
  kbd { font: 11px monospace; border: 1px solid #444; border-radius: 3px; padding: 1px 4px;
        margin: 0 1px; }
  #widths button { min-width: 76px; }
</style>
<header>
  <h1>gbui — the same screen in every theme</h1>
  <p>One tree. The theme changes the colours, the width changes the layout, and
     nothing in the screen's code knows about either.
     <kbd>&larr;</kbd><kbd>&rarr;</kbd> themes, <kbd>&uarr;</kbd><kbd>&darr;</kbd> widths.</p>
</header>
<nav id="themes">
)" << themeButtons
        << R"(</nav>
<nav id="widths">
)" << widthButtons
        << R"(</nav>
<main><img id="screen" alt="rendered screen"></main>
<script>
  const themes = document.getElementById('themes');
  const widths = document.getElementById('widths');
  const screen = document.getElementById('screen');
  const current = { theme: themes.children[0].dataset.theme,
                    width: widths.children[1]?.dataset.width ?? widths.children[0].dataset.width };

  const mark = (nav, key, value) => {
    for (const button of nav.children) {
      button.setAttribute('aria-current', String(button.dataset[key] === value));
    }
  };
  const render = () => {
    screen.src = `${current.theme}-${current.width}.svg`;
    screen.style.maxWidth = `${current.width}px`;
    mark(themes, 'theme', current.theme);
    mark(widths, 'width', current.width);
  };

  themes.addEventListener('click', e => {
    if (!e.target.dataset.theme) return;
    current.theme = e.target.dataset.theme;
    render();
  });
  widths.addEventListener('click', e => {
    if (!e.target.dataset.width) return;
    current.width = e.target.dataset.width;
    render();
  });

  // Left/right walk the themes, up/down the widths. Holding the layout still
  // while only the colours move is the fastest way to spot a token a theme got
  // wrong; holding the theme still while the width moves is how you see what
  // the layout does when it runs out of room.
  const step = (nav, key, delta) => {
    const buttons = [...nav.children];
    const index = buttons.findIndex(b => b.dataset[key] === current[key]);
    current[key] = buttons[(index + delta + buttons.length) % buttons.length].dataset[key];
    render();
  };
  addEventListener('keydown', event => {
    if (event.key === 'ArrowRight') step(themes, 'theme', 1);
    if (event.key === 'ArrowLeft') step(themes, 'theme', -1);
    if (event.key === 'ArrowDown') step(widths, 'width', 1);
    if (event.key === 'ArrowUp') step(widths, 'width', -1);
  });
  render();
</script>
)";
}

std::vector<Theme> collectThemes(const std::filesystem::path& registry) {
    std::vector<Theme> themes;
    if (registry.empty() || !std::filesystem::exists(registry)) {
        themes.push_back(Theme::dark());
        themes.push_back(Theme::light());
        return themes;
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(registry)) {
        if (entry.is_regular_file() && entry.path().filename() == "theme.json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        std::string error;
        if (auto theme = Theme::fromFile(file.string(), &error)) {
            themes.push_back(std::move(*theme));
        } else {
            // A broken theme names its own fault and does not stop the sweep.
            std::printf("  %-18s SKIPPED: %s\n", file.parent_path().filename().string().c_str(),
                        error.c_str());
        }
    }
    if (themes.empty()) themes.push_back(Theme::dark());
    return themes;
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path directory = argc > 1 ? argv[1] : ".";
    const std::filesystem::path registry = argc > 2 ? argv[2] : "";
    std::filesystem::create_directories(directory);

    const std::vector<Theme> themes = collectThemes(registry);
    std::vector<Rendered> rendered;
    rendered.reserve(themes.size());
    for (const Theme& theme : themes) rendered.push_back(renderTheme(directory, theme));

    writeIndex(directory, rendered);
    std::printf("\n  %zu themes x %zu widths = %zu renders -> %s/index.html\n", rendered.size(),
                std::size(kWidths), rendered.size() * std::size(kWidths),
                directory.string().c_str());
    return 0;
}
