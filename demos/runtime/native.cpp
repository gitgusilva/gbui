// gbui_demo — the demo catalogue in a window, or in a file.
//
//     gbui_demo                          open the first screen
//     gbui_demo --list                   print the catalogue and exit
//     gbui_demo weather                  open one by id
//     gbui_demo weather --shot out.ppm   render one frame and exit
//     gbui_demo weather --svg out.svg    record the same frame as SVG
//     gbui_demo --stills out/            a still of every screen, no window
//
//     --size W H     logical size, default the screen's own design size
//     --scale S      device pixels per logical pixel
//     --skin NAME    gitbox | material | cupertino | fluent
//     --light        the light palette
//     --at SECONDS   wind the demo clock forward before the shot
//     --at-pointer X Y   park the pointer there first, so a still can catch a
//                        hover — a chart's readout, a row's highlight
//     --font-size N
//
// While it runs: left and right move between screens, `t` cycles the skin,
// `d` toggles light and dark, escape quits.
//
// There is almost nothing in this file, and that is the point — every decision
// lives in `Host`, so the browser build makes the same calls in the same order.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gbui/meta/components.hpp"
#include "gbui/platform/window.hpp"
#include "gbui_demos/catalog.hpp"
#include "gbui_demos/demos.hpp"
#include "gbui_demos/host.hpp"

using namespace gbui;

namespace {

/**
 * Every component in the metadata with no live example, and the reverse.
 *
 * The only thing holding the two halves together. `gbui::meta` is generated
 * from the headers and cannot fall behind them; the examples are written by
 * hand and can. A gallery that quietly omits a component is worse than one
 * that refuses to build, so this is a build step rather than a good intention.
 */
int reportCoverage() {
    int problems = 0;

    std::size_t names = 0;
    for (const meta::ComponentInfo& entry : meta::components()) {
        // A component is declared once per overload; only the first of each
        // name is counted, and only one example is expected.
        bool first = true;
        for (const meta::ComponentInfo& other : meta::components()) {
            if (&other == &entry) break;
            if (other.name == entry.name) first = false;
        }
        if (!first) continue;
        ++names;
        if (demos::catalog::find(entry.name)) continue;
        std::printf("no example: %-20.*s (%.*s)\n", static_cast<int>(entry.name.size()),
                    entry.name.data(), static_cast<int>(entry.header.size()), entry.header.data());
        ++problems;
    }

    // The other direction matters too: an example for a component that no
    // longer exists stops compiling the day someone renames it, and until
    // then it is a lie in the gallery.
    for (const demos::catalog::Example& example : demos::catalog::examples()) {
        if (meta::find(example.component)) continue;
        std::printf("example for an unknown component: %.*s\n",
                    static_cast<int>(example.component.size()), example.component.data());
        ++problems;
    }

    std::printf("%zu components, %zu examples, %d missing\n", names,
                demos::catalog::examples().size(), problems);
    return problems;
}

void printCatalogue() {
    std::printf("%-12s  %-26s  %s\n", "ID", "TITLE", "SECTOR");
    for (const demos::DemoInfo& entry : demos::catalogue()) {
        std::printf("%-12.*s  %-26.*s  %.*s\n", static_cast<int>(entry.id.size()), entry.id.data(),
                    static_cast<int>(entry.title.size()), entry.title.data(),
                    static_cast<int>(entry.sector.size()), entry.sector.data());
    }
}

/**
 * Creates the directory a file is about to be written into.
 *
 * `std::ofstream` does not, and the error it gives when the directory is
 * missing is the same one it gives when the disk is full — so a CI job asked
 * for `out/demos/` got "cannot write out/demos/analytics.ppm" and no hint that
 * the only thing wrong was a missing `mkdir`.
 */
bool ensureDirectory(const std::filesystem::path& directory) {
    if (directory.empty() || std::filesystem::exists(directory)) return true;
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return !error;
}

/** Winds a fresh host forward to `seconds` without showing anything, so a still
 *  catches a screen mid-life rather than at rest. Several frames, not one: the
 *  components that place themselves from last frame's geometry — a chart's
 *  crosshair, a table's columns — need a frame or two to settle, and a shot
 *  taken too early shows a layout that never appears on screen. */
void settle(demos::Host& host, float seconds, Vec2 pointer, bool pointed) {
    constexpr float kStep = 1.0f / 60.0f;
    const int steps = seconds > 0.0f ? static_cast<int>(seconds / kStep) : 0;
    for (int i = 0; i < steps; ++i) host.frame(kStep);
    // The pointer goes down last and gets frames of its own: hit testing reads
    // the tree the *previous* frame laid out, so a hover asked for and shot in
    // the same frame lands on a layout that had not happened yet.
    if (pointed) host.pointerMove(pointer.x, pointer.y);
    for (int i = 0; i < 4; ++i) host.frame(kStep);
}

}  // namespace

int main(int argc, char** argv) {
    demos::HostOptions options;
    std::string component;
    std::string shotPath;
    std::string svgPath;
    std::string stillsDirectory;
    float at = 0.0f;
    float fontSize = 0.0f;
    bool sized = false;
    // Where the pointer is parked before a still is taken. Off the canvas by
    // default, which is what "no pointer" means to the hit testing.
    Vec2 pointer{-1.0f, -1.0f};
    bool pointed = false;

    for (int i = 1; i < argc; ++i) {
        const char* argument = argv[i];
        if (std::strcmp(argument, "--list") == 0) {
            printCatalogue();
            return 0;
        }
        if (std::strcmp(argument, "--coverage") == 0) {
            return reportCoverage() == 0 ? 0 : 1;
        }
        if (std::strcmp(argument, "--components") == 0) {
            for (const meta::ComponentInfo& entry : meta::components()) {
                std::printf("%-20.*s %.*s\n", static_cast<int>(entry.name.size()),
                            entry.name.data(), static_cast<int>(entry.group.size()),
                            entry.group.data());
            }
            return 0;
        }
        if (std::strcmp(argument, "--shot") == 0 && i + 1 < argc)
            shotPath = argv[++i];
        else if (std::strcmp(argument, "--svg") == 0 && i + 1 < argc)
            svgPath = argv[++i];
        else if (std::strcmp(argument, "--stills") == 0 && i + 1 < argc)
            stillsDirectory = argv[++i];
        else if (std::strcmp(argument, "--component") == 0 && i + 1 < argc)
            component = argv[++i];
        else if (std::strcmp(argument, "--skin") == 0 && i + 1 < argc)
            options.skin = argv[++i];
        else if (std::strcmp(argument, "--light") == 0)
            options.darkMode = false;
        else if (std::strcmp(argument, "--at") == 0 && i + 1 < argc) {
            at = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argument, "--font-size") == 0 && i + 1 < argc) {
            fontSize = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argument, "--at-pointer") == 0 && i + 2 < argc) {
            pointer.x = static_cast<float>(std::atof(argv[++i]));
            pointer.y = static_cast<float>(std::atof(argv[++i]));
            pointed = true;
        } else if (std::strcmp(argument, "--scale") == 0 && i + 1 < argc) {
            options.scale = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argument, "--size") == 0 && i + 2 < argc) {
            options.width = std::atoi(argv[++i]);
            options.height = std::atoi(argv[++i]);
            sized = true;
        } else if (argument[0] != '-') {
            options.demo = argument;
        } else {
            std::fprintf(stderr, "gbui_demo: unknown option %s\n", argument);
            return 2;
        }
    }

    if (demos::catalogue().empty()) {
        std::fprintf(stderr, "gbui_demo: the catalogue is empty\n");
        return 1;
    }
    if (!options.demo.empty() && !demos::find(options.demo)) {
        std::fprintf(stderr, "gbui_demo: no demo called '%s'\n", options.demo.c_str());
        printCatalogue();
        return 2;
    }

    // Each screen carries the size it was drawn for — and a component preview
    // its own, which is far smaller than any screen — so a still is composed
    // the way its author composed it unless the caller says otherwise.
    const auto applyDesignSize = [&](demos::Host& host) {
        if (sized) return;
        const Vec2 design = host.designSize();
        host.resize(static_cast<int>(design.x), static_cast<int>(design.y), options.scale);
    };

    if (!stillsDirectory.empty()) {
        if (!ensureDirectory(stillsDirectory)) {
            std::fprintf(stderr, "gbui_demo: cannot create %s\n", stillsDirectory.c_str());
            return 1;
        }
        if (stillsDirectory.back() != '/') stillsDirectory += '/';
        for (const demos::DemoInfo& entry : demos::catalogue()) {
            demos::HostOptions one = options;
            one.demo = std::string(entry.id);
            demos::Host host(one);
            if (fontSize > 0.0f) host.setFontSize(fontSize);
            applyDesignSize(host);
            settle(host, at, pointer, pointed);
            const std::string path = stillsDirectory + std::string(entry.id) + ".ppm";
            if (!host.writePpm(path)) {
                std::fprintf(stderr, "gbui_demo: cannot write %s\n", path.c_str());
                return 1;
            }
            std::printf("wrote %s\n", path.c_str());
        }
        return 0;
    }

    demos::Host host(options);
    if (fontSize > 0.0f) host.setFontSize(fontSize);
    if (!component.empty() && !host.selectComponent(component)) {
        std::fprintf(stderr, "gbui_demo: no example for component '%s'\n", component.c_str());
        return 2;
    }

    if (!shotPath.empty() || !svgPath.empty()) {
        if (!ensureDirectory(std::filesystem::path(shotPath).parent_path()) ||
            !ensureDirectory(std::filesystem::path(svgPath).parent_path())) {
            std::fprintf(stderr, "gbui_demo: cannot create the output directory\n");
            return 1;
        }
        applyDesignSize(host);
        settle(host, at, pointer, pointed);
        if (!shotPath.empty() && !host.writePpm(shotPath)) {
            std::fprintf(stderr, "gbui_demo: cannot write %s\n", shotPath.c_str());
            return 1;
        }
        if (!svgPath.empty()) {
            // An ofstream rather than `std::fopen`, which MSVC deprecates in
            // favour of a function only MSVC has.
            std::ofstream file(svgPath, std::ios::binary);
            const std::string svg = host.toSvg();
            file << svg;
            if (!file) {
                std::fprintf(stderr, "gbui_demo: cannot write %s\n", svgPath.c_str());
                return 1;
            }
        }
        std::printf("wrote %s%s%s\n", shotPath.c_str(), shotPath.empty() ? "" : " ",
                    svgPath.c_str());
        return 0;
    }

    applyDesignSize(host);
    auto window = Window::create({.title = "gbui — demos",
                                  .width = host.width(),
                                  .height = host.height(),
                                  .minWidth = 640,
                                  .minHeight = 420});
    if (!window) {
        std::fprintf(stderr,
                     "gbui_demo: no window backend was compiled in. Use --shot to render a "
                     "frame to a file instead.\n");
        return 1;
    }

    // Which demo is showing, so the arrow keys can step through the catalogue.
    std::size_t index = 0;
    for (std::size_t i = 0; i < demos::catalogue().size(); ++i) {
        if (demos::catalogue()[i].id == host.selected()) index = i;
    }
    std::size_t skinIndex = 0;

    auto previous = std::chrono::steady_clock::now();
    while (window->pumpEvents()) {
        const auto now = std::chrono::steady_clock::now();
        const float delta = std::chrono::duration<float>(now - previous).count();
        previous = now;

        const Vec2 size = window->size();
        host.resize(static_cast<int>(size.x), static_cast<int>(size.y), window->scale());

        InputFrame input = window->takeInput();
        bool quit = false;
        // The runner's own shortcuts are taken before the screen sees them,
        // and only while nothing has focus — otherwise `d` could not be typed
        // into a field.
        if (host.focused().empty()) {
            std::vector<KeyEvent> forwarded;
            for (const KeyEvent& event : input.keys) {
                const auto& list = demos::catalogue();
                if (event.key == Key::Escape) {
                    quit = true;
                } else if (event.key == Key::Right) {
                    index = (index + 1) % list.size();
                    host.select(list[index].id);
                } else if (event.key == Key::Left) {
                    index = (index + list.size() - 1) % list.size();
                    host.select(list[index].id);
                } else if (event.key == Key::T) {
                    skinIndex = (skinIndex + 1) % demos::skins().size();
                    host.setSkin(demos::skins()[skinIndex].id);
                } else if (event.key == Key::D) {
                    host.setDarkMode(!host.darkMode());
                } else {
                    forwarded.push_back(event);
                }
            }
            input.keys.swap(forwarded);
        }
        if (quit) break;

        host.submit(input);
        host.frame(delta);
        window->setCursor(host.cursor());

        // The framebuffer the host rasterised into is not the window's, so it
        // is copied across. A backend that let an application draw straight
        // into its own would skip this; SDL2's does not.
        Canvas& target = window->canvas();
        const Canvas& source = host.canvas();
        if (target.width() == source.width() && target.height() == source.height()) {
            std::memcpy(target.pixels(), source.pixels(),
                        source.pitch() * static_cast<std::size_t>(source.height()));
        }
        window->present();
    }
    return 0;
}
