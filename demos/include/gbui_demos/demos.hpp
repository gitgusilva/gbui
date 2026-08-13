// The demo catalogue: seven application screens, built from nothing but the
// public library.
//
// Each demo is a class with one method. It is handed a `Ui`, the frame's
// `Interaction` and a clock, and it writes a tree — exactly what an application
// does, and nothing more. It never sees a window, a canvas, a font or a theme,
// which is why the same screens run in a native window, in a headless
// screenshot and in a browser through WebAssembly without one line of them
// knowing which.
//
// The host does the rest: `gbui_demos/host.hpp`.
#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "gbui/core/geometry.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui::demos {

/** Everything a demo is given for one frame. */
struct Frame {
    Ui& ui;
    /** Resolved against last frame's tree, as the toolkit's contract says. */
    const Interaction& input;
    /** Seconds since this demo was selected. The telemetry runs on it, so a
     *  screenshot at a given time is the same picture on every machine. */
    float time = 0.0f;
    /** Seconds since the previous frame. */
    float delta = 0.0f;
    /** The rectangle the tree will be laid out into, in logical pixels. */
    Vec2 viewport{};
};

/**
 * One screen.
 *
 * A class rather than a function because a real application screen has state —
 * which row is selected, where a chart is panned to, what the last hundred
 * samples were — and that state belongs to the demo, not to the toolkit. That
 * is the same division every component here works under: the widget owns the
 * geometry, the application owns the value.
 */
class Demo {
public:
    virtual ~Demo() = default;

    /** Builds the screen and returns its root. */
    virtual NodeId build(Frame& frame) = 0;
};

/** Which palette a screen was designed against. */
enum class Palette {
    /** Follows the reader — the site's dark mode, the window's flag. */
    Follow,
    /** Always dark. A control room screen in a light palette is not a control
     *  room screen; the operators are looking at it in a dim room at 3am. */
    Dark,
    /** Always light. */
    Light,
};

/** What the catalogue knows about a screen, without building it. */
struct DemoInfo {
    /** Stable, URL-safe, and what every entry point takes: `--demo weather`,
     *  `<Demo id="weather" />`. */
    std::string_view id;
    std::string_view title;
    /** The industry it is drawn from, for the gallery's grouping. */
    std::string_view sector;
    /** One sentence, for the gallery card and the `--list` output. */
    std::string_view summary;
    /** What the screen is worth looking at for, three or four words each. */
    std::vector<std::string_view> highlights;
    /**
     * One sentence telling a reader what to actually *do* with it.
     *
     * Here rather than in the documentation because it is the one piece of
     * prose that goes stale the moment the screen changes, and a field beside
     * the screen's own definition is edited by whoever changes it. The
     * documentation site reads this straight out of the source. */
    std::string_view tryThis;
    /** The size it was designed at, in logical pixels. A host with less room
     *  scales down rather than reflowing, so the composition survives. */
    Vec2 design{1280.0f, 760.0f};
    Palette palette = Palette::Follow;
    /** Builds one. Never null for an entry that reached the catalogue. */
    std::unique_ptr<Demo> (*create)() = nullptr;
};

/** Every demo, in the order the gallery shows them. */
const std::vector<DemoInfo>& catalogue();

/** One by id, or nullptr. */
const DemoInfo* find(std::string_view id);

/** One by id, built. Null when the id is not in the catalogue. */
std::unique_ptr<Demo> create(std::string_view id);

}  // namespace gbui::demos
