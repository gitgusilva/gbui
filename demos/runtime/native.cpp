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
#include <string>
#include <vector>

#include "gbui/platform/window.hpp"
#include "gbui_demos/demos.hpp"
#include "gbui_demos/host.hpp"

using namespace gbui;

namespace {

void printCatalogue() {
    std::printf("%-12s  %-26s  %s\n", "ID", "TITLE", "SECTOR");
    for (const demos::DemoInfo& entry : demos::catalogue()) {
        std::printf("%-12.*s  %-26.*s  %.*s\n", static_cast<int>(entry.id.size()), entry.id.data(),
                    static_cast<int>(entry.title.size()), entry.title.data(),
                    static_cast<int>(entry.sector.size()), entry.sector.data());
    }
}

/** Winds a fresh host forward to `seconds` without showing anything, so a still
 *  catches a screen mid-life rather than at rest. Several frames, not one: the
 *  components that place themselves from last frame's geometry — a chart's
 *  crosshair, a table's columns — need a frame or two to settle, and a shot
 *  taken too early shows a layout that never appears on screen. */
void settle(demos::Host& host, float seconds) {
    constexpr float kStep = 1.0f / 60.0f;
    const int steps = seconds > 0.0f ? static_cast<int>(seconds / kStep) : 0;
    for (int i = 0; i < steps; ++i) host.frame(kStep);
    for (int i = 0; i < 4; ++i) host.frame(kStep);
}

}  // namespace

int main(int argc, char** argv) {
    demos::HostOptions options;
    std::string shotPath;
    std::string svgPath;
    std::string stillsDirectory;
    float at = 0.0f;
    float fontSize = 0.0f;
    bool sized = false;

    for (int i = 1; i < argc; ++i) {
        const char* argument = argv[i];
        if (std::strcmp(argument, "--list") == 0) {
            printCatalogue();
            return 0;
        }
        if (std::strcmp(argument, "--shot") == 0 && i + 1 < argc)
            shotPath = argv[++i];
        else if (std::strcmp(argument, "--svg") == 0 && i + 1 < argc)
            svgPath = argv[++i];
        else if (std::strcmp(argument, "--stills") == 0 && i + 1 < argc)
            stillsDirectory = argv[++i];
        else if (std::strcmp(argument, "--skin") == 0 && i + 1 < argc)
            options.skin = argv[++i];
        else if (std::strcmp(argument, "--light") == 0)
            options.darkMode = false;
        else if (std::strcmp(argument, "--at") == 0 && i + 1 < argc) {
            at = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argument, "--font-size") == 0 && i + 1 < argc) {
            fontSize = static_cast<float>(std::atof(argv[++i]));
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

    // Each screen carries the size it was drawn for, so a still is composed the
    // way its author composed it unless the caller says otherwise.
    const auto applyDesignSize = [&](demos::Host& host) {
        if (sized || !host.info()) return;
        host.resize(static_cast<int>(host.info()->design.x),
                    static_cast<int>(host.info()->design.y), options.scale);
    };

    if (!stillsDirectory.empty()) {
        if (!stillsDirectory.empty() && stillsDirectory.back() != '/') stillsDirectory += '/';
        for (const demos::DemoInfo& entry : demos::catalogue()) {
            demos::HostOptions one = options;
            one.demo = std::string(entry.id);
            demos::Host host(one);
            if (fontSize > 0.0f) host.setFontSize(fontSize);
            applyDesignSize(host);
            settle(host, at);
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

    if (!shotPath.empty() || !svgPath.empty()) {
        applyDesignSize(host);
        settle(host, at);
        if (!shotPath.empty() && !host.writePpm(shotPath)) {
            std::fprintf(stderr, "gbui_demo: cannot write %s\n", shotPath.c_str());
            return 1;
        }
        if (!svgPath.empty()) {
            std::FILE* file = std::fopen(svgPath.c_str(), "wb");
            if (!file) {
                std::fprintf(stderr, "gbui_demo: cannot write %s\n", svgPath.c_str());
                return 1;
            }
            const std::string svg = host.toSvg();
            std::fwrite(svg.data(), 1, svg.size(), file);
            std::fclose(file);
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
