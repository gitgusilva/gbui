// gbui_app — the toolkit in a real window.
//
//     gbui_app                       open the window
//     gbui_app --themes <dir>        cycle the themes found in a directory
//     gbui_app --shot <file.ppm>     render one frame to a file and exit
//
// While it runs:
//     left / right     previous / next theme
//     +  /  -          larger / smaller UI font
//     escape           quit
//
// The frame loop is the whole point of this file, and it is five lines: build
// the tree, lay it out at the window's size, record a display list, rasterise,
// present. Resizing the window re-runs exactly that — there is no separate
// "handle resize" path, because the layout is a function of the viewport.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/paint/canvas.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/platform/font.hpp"
#include "gbui/platform/window.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "screen.hpp"

using namespace gbui;

namespace {

std::vector<Theme> loadThemes(const std::string& directory) {
    std::vector<Theme> themes;
    if (!directory.empty() && std::filesystem::exists(directory)) {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().filename() == "theme.json") {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        for (const auto& file : files) {
            std::string error;
            if (auto theme = Theme::fromFile(file.string(), &error)) themes.push_back(*theme);
        }
    }
    if (themes.empty()) {
        themes.push_back(Theme::dark());
        themes.push_back(Theme::light());
    }
    return themes;
}

/** Writes the canvas as a binary PPM — enough to look at a frame from a script
 *  or a CI job without a screenshot tool. */
bool writePpm(const Canvas& canvas, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";
    const std::uint8_t* pixels = canvas.pixels();
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * canvas.pitch()) +
                                      static_cast<std::size_t>(x) * 4;
            out.put(static_cast<char>(pixels[index]));
            out.put(static_cast<char>(pixels[index + 1]));
            out.put(static_cast<char>(pixels[index + 2]));
        }
    }
    return true;
}

/** One frame, start to finish. Everything the application does is here. */
NodeId drawFrame(float scale, Canvas& canvas, FontDatabase& fonts, const Theme& theme,
                 Arena& arena,
                 const examples::ScreenState& state, Animator* animator = nullptr) {
    arena.reset();  // O(1): the previous frame's nodes are simply forgotten
    Ui ui(arena);
    ui.setMeasure(measureWith(fonts, scale), theme.typography());
    ui.setAnimator(animator);
    const NodeId root = examples::buildScreen(ui, theme, state);

    LayoutContext context;
    context.theme = &theme;
    context.measure = measureWith(fonts, scale);  // real metrics, from the real font
    layout(arena, root, Rect{0, 0, static_cast<float>(canvas.width()) / scale,
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
    std::string themeDirectory;
    std::string shotPath;
    std::string forcedHover;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--themes") == 0 && i + 1 < argc) themeDirectory = argv[++i];
        else if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shotPath = argv[++i];
        // Forcing a hovered row is how an offscreen render exercises a state
        // that otherwise needs a pointer.
        else if (std::strcmp(argv[i], "--hover") == 0 && i + 1 < argc) forcedHover = argv[++i];
    }

    std::vector<Theme> themes = loadThemes(themeDirectory);
    std::size_t current = 0;
    FontDatabase fonts;
    Arena arena;
    arena.reserve(512);

    // --shot renders one frame offscreen, which is how a build machine with no
    // display checks that the whole pipeline still produces pixels.
    if (!shotPath.empty()) {
        Canvas canvas(1180, static_cast<int>(examples::kScreenHeight));
        drawFrame(1.0f, canvas, fonts, themes[current], arena, {.hovered = forcedHover});
        if (!writePpm(canvas, shotPath)) {
            std::fprintf(stderr, "gbui_app: cannot write %s\n", shotPath.c_str());
            return 1;
        }
        std::printf("wrote %s (%dx%d)\n", shotPath.c_str(), canvas.width(), canvas.height());
        return 0;
    }

    WindowOptions options;
    options.title = "gbui — " + themes[current].name();
    options.height = static_cast<int>(examples::kScreenHeight);
    std::unique_ptr<Window> window = Window::create(options);
    if (!window) {
        std::fprintf(stderr, "gbui_app: no window backend could open a window\n");
        return 1;
    }

    std::printf("gbui_app: %zu themes loaded. left/right to switch, +/- to resize the UI font,\n"
                "          escape to quit.\n", themes.size());

    bool running = true;
    bool dirty = true;
    Interaction interaction;
    examples::ScreenState state;
    std::string hovered;
    NodeId root;
    Animator animator;
    auto previous = std::chrono::steady_clock::now();
    while (running) {
        if (!window->pumpEvents()) break;
        if (window->resized()) dirty = true;

        const auto frameAt = std::chrono::steady_clock::now();
        animator.tick(std::chrono::duration<float>(frameAt - previous).count());
        previous = frameAt;
        // A screen that repaints only on change still has to repaint while
        // something is moving — otherwise a transition starts and freezes on
        // its first frame, which looks far worse than no animation at all.
        if (animator.animating()) dirty = true;

        // The events are resolved against the tree the last frame left behind,
        // which is the tree the user was looking at when they moved or clicked.
        const InputFrame input = window->takeInput();
        interaction.update(arena, root, input);
        window->setCursor(interaction.cursor());

        // Hover only forces a repaint when the answer changes, so moving the
        // pointer inside one row paints nothing.
        if (interaction.hovered() != hovered) {
            hovered = std::string(interaction.hovered());
            dirty = true;
        }

        for (const KeyEvent& event : interaction.keys()) {
            switch (event.key) {
                case Key::Escape: running = false; break;
                case Key::Right:
                    current = (current + 1) % themes.size();
                    dirty = true;
                    break;
                case Key::Left:
                    current = (current + themes.size() - 1) % themes.size();
                    dirty = true;
                    break;
                case Key::Plus:
                case Key::Minus: {
                    // The type scale lives in the theme, so changing it is a
                    // one-line edit and everything laid out from it follows.
                    Typography& typography = themes[current].typography();
                    const float step = event.key == Key::Plus ? 1.0f : -1.0f;
                    typography.uiFontSize = std::clamp(typography.uiFontSize + step, 9.0f, 24.0f);
                    typography.editorFontSize =
                        std::clamp(typography.editorFontSize + step, 9.0f, 24.0f);
                    dirty = true;
                    break;
                }
                default: break;
            }
        }

        if (dirty) {
            // The tag is copied into `hovered` because the arena that owns the
            // string is about to be reset under it.
            state.hovered = hovered;
            root = drawFrame(window->scale(), window->canvas(), fonts, themes[current], arena, state, &animator);
            window->present();
            dirty = false;
        } else {
            // Nothing changed, so nothing is drawn. A UI that repaints only on
            // change is the difference between an idle process at 0% and one
            // burning a core to show a static list.
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }
    return 0;
}
