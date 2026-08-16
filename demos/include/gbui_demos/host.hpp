// Everything around a demo that is not the demo.
//
// A screen in `demos.hpp` writes a tree and knows nothing else. Something still
// has to own the arena, the fonts, the theme, the animation clock and the
// interaction state, run the four pipeline stages in order and hand the result
// to whatever is going to show it. That is this class, and there is exactly one
// of it — the native runner, the screenshot writer and the WebAssembly module
// are three thin shells over the same object.
//
// It is also the shortest honest answer to "how do I embed this toolkit?":
// construct a Host, call `resize`, push input, call `frame` and upload
// `canvas().pixels()`.
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/a11y/tree.hpp"
#include "gbui/anim/animator.hpp"
#include "gbui/core/cursor.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/input/keys.hpp"
#include "gbui/paint/canvas.hpp"
#include "gbui/platform/font.hpp"
#include "gbui/scene/tree.hpp"
#include "gbui/style/design.hpp"
#include "gbui/style/theme.hpp"
#include "gbui_demos/demos.hpp"

namespace gbui::demos {

/** The palettes and shape rules a host can be switched between, by name. */
struct Skin {
    std::string_view id;
    std::string_view name;
};

/** Every skin, in the order a picker should list them. */
const std::vector<Skin>& skins();

struct HostOptions {
    /** Logical pixels — the units the demo lays out in. */
    int width = 1280;
    int height = 760;
    /** Device pixels per logical pixel. 2 on a doubled display. */
    float scale = 1.0f;
    std::string skin = "gitbox";
    bool darkMode = true;
    /** Which demo to start on. Empty takes the first in the catalogue. */
    std::string demo{};
};

class Host {
public:
    explicit Host(const HostOptions& options = {});
    ~Host();

    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    // ---- fonts ----------------------------------------------------------
    //
    // A browser has no /usr/share/fonts, so the WebAssembly build embeds its
    // faces and names them here. A native run needs none of this and finds the
    // theme's families on the machine.

    /** Forgets the platform's font directories, so only registered files are
     *  searched. Call before `addFont`. */
    void useBundledFontsOnly();
    /** Registers one file under a family name — CSS's `@font-face`. */
    bool addFont(std::string_view family, const std::string& path,
                 FontWeight weight = FontWeight::Regular, FontSlant slant = FontSlant::Normal);
    /** Forces every theme's UI and mono families to these, whatever the theme
     *  itself asks for. Empty leaves the theme's own choice alone. */
    void setFontFamilies(std::string_view ui, std::string_view mono);

    // ---- what is on screen ----------------------------------------------

    /** Switches demos, restarting the clock. False when the id is unknown, and
     *  in that case what was showing stays. */
    bool select(std::string_view id);

    /**
     * Shows one *component* instead of an application screen: its live example
     * from the catalogue, with its name and signature from the metadata.
     *
     * The same host, the same pipeline, the same input — a component preview
     * is a demo whose tree happens to be four lines. False when no example is
     * registered for that name, which `gbui_demo --coverage` exists to catch
     * before a reader does.
     */
    bool selectComponent(std::string_view component);
    std::string_view selected() const { return selectedId_; }
    const DemoInfo* info() const { return info_; }

    /**
     * The size what is showing was composed for, in logical pixels.
     *
     * A screen carries its own — a control room desk is drawn for a wide one —
     * and a component carries the preview frame's, which is a good deal smaller
     * than any of them. Without this a still of `slider` came out as one
     * slider in the corner of a dashboard-sized canvas, because the only size
     * on offer was the last screen's.
     */
    Vec2 designSize() const;

    /** One of `skins()`. Unknown names are ignored. */
    void setSkin(std::string_view id);
    std::string_view skin() const { return skinId_; }

    /** The reader's light/dark preference. A demo whose `Palette` is fixed
     *  overrides it — a control room screen is dark at noon too. */
    void setDarkMode(bool dark);
    bool darkMode() const { return darkMode_; }

    /** Base UI font size in logical pixels; everything else scales off it. */
    void setFontSize(float size);

    void resize(int width, int height, float scale);
    int width() const { return width_; }
    int height() const { return height_; }
    float scale() const { return scale_; }

    /** Starts the demo's clock over, keeping the demo and its state. */
    void restart();

    // ---- input ----------------------------------------------------------
    //
    // Level state (where the pointer is, which modifiers are held) is set;
    // events (keys, typed text, wheel) accumulate until the next `frame`.

    void pointerMove(float x, float y);
    void pointerButton(bool down);
    /** Parks the pointer outside the window, which is what ends a hover. */
    void pointerLeave();
    void wheel(float lines);
    void setModifiers(Modifiers modifiers);
    void key(Key key, bool repeat = false);
    void type(std::string_view utf8);

    /** Merges a whole frame of platform input — what `Window::takeInput`
     *  returns — into whatever has accumulated since the last `frame`. */
    void submit(const InputFrame& input);

    // ---- the frame ------------------------------------------------------

    /** Resolves input, builds, lays out, records and rasterises. `delta` is
     *  seconds since the last call. */
    void frame(float delta);

    /** The framebuffer the last `frame` wrote: row-major premultiplied RGBA8,
     *  `width * scale` by `height * scale` device pixels. */
    const Canvas& canvas() const { return canvas_; }

    /** What the pointer should look like, decided by the node under it. */
    Cursor cursor() const { return interaction_.cursor(); }

    /**
     * What the last `frame` built, as a screen reader would meet it.
     *
     * Exposed because the tree is the only way to check a *call site*: the
     * toolkit names everything it can, and a name it cannot invent — an
     * icon-only button, a chart, a table — is the application's to supply.
     * `gbui_demo --a11y` walks this over every screen and every example and
     * fails naming what it found, which is the same gate
     * `tests/accessibilityTest` is for the library.
     */
    AccessibilityTree accessibility() const {
        return buildAccessibilityTree(arena_, root_, interaction_);
    }

    /** The tag of the control holding the keyboard, or empty. An embedder uses
     *  it to decide whether a key belongs to the screen or to itself — a demo
     *  switcher on `left`/`right` must not steal them from a focused list. */
    std::string_view focused() const { return interaction_.focused(); }

    /** Seconds the current demo has been running. */
    float time() const { return time_; }

    /** Writes the canvas as a binary PPM. The one thing this class does that a
     *  real application would not — it is how the documentation's stills and
     *  CI's smoke test get a frame with no display attached. */
    bool writePpm(const std::string& path) const;

    /** Records the same frame as SVG instead of rasterising it: text stays
     *  text and the file can be diffed. */
    std::string toSvg();

private:
    Theme themeFor() const;
    Design designFor() const;
    /** Builds and lays out into the current viewport, leaving the tree in the
     *  arena and the root in `root_`. Shared by `frame` and `toSvg`. */
    NodeId buildTree(MeasureText& measure, const Theme& theme);

    Arena arena_;
    FontDatabase fonts_;
    Interaction interaction_;
    Animator animator_;
    Canvas canvas_;
    InputFrame pending_{};
    NodeId root_{};

    std::unique_ptr<Demo> demo_;
    const DemoInfo* info_ = nullptr;
    std::string selectedId_;
    std::string skinId_ = "gitbox";
    std::string uiFamily_;
    std::string monoFamily_;

    int width_ = 1280;
    int height_ = 760;
    float scale_ = 1.0f;
    float fontSize_ = 13.0f;
    float time_ = 0.0f;
    float lastDelta_ = 0.0f;
    bool darkMode_ = true;
};

}  // namespace gbui::demos
