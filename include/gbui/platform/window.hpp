// A window, an event loop, and somewhere to draw.
//
// The interface is deliberately small — what an application needs to draw a
// frame and react to a pointer and a keyboard, and nothing else. Backends live
// behind `Window::create`; today there is one, on SDL2, and a second would be a
// new file rather than a change to any of these declarations.
//
// This module is optional: the library builds and its tests pass without it.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gbui/core/geometry.hpp"
#include "gbui/input/interaction.hpp"
#include "gbui/input/keys.hpp"
#include "gbui/paint/canvas.hpp"

namespace gbui {

struct WindowOptions {
    std::string title = "gbui";
    int width = 1180;
    int height = 640;
    int minWidth = 420;
    int minHeight = 320;
    /**
     * Let the display pace the loop: `present()` blocks until the next refresh.
     *
     * On by default because it is what an application wants — no tearing, and
     * the loop costs exactly one frame's work per refresh instead of spinning.
     * The alternative is not "sleep a while at the end of the frame": a fixed
     * sleep cannot know when the next refresh is, so it either burns the
     * remainder or overshoots it and lands on the one after, which halves the
     * rate. Turn it off to measure how fast the toolkit can actually draw.
     */
    bool vsync = true;
};

struct MouseState {
    Vec2 position{};
    bool leftDown = false;
    /** The secondary button. What a context menu is opened by, and the reason
     *  this struct grew past one button. */
    bool rightDown = false;
};

class Window {
public:
    virtual ~Window() = default;

    /** Opens a window with the best backend available, or nothing when none is
     *  compiled in. */
    static std::unique_ptr<Window> create(const WindowOptions& options);

    /** Drains the event queue. Returns false once the user has closed it. */
    virtual bool pumpEvents() = 0;

    /** The framebuffer for this frame, already sized to the window. */
    virtual Canvas& canvas() = 0;

    /** Uploads the framebuffer and flips. */
    virtual void present() = 0;

    /** The framebuffer's size in **logical** pixels — the units layout, hit
     *  testing and the input events all speak. */
    virtual Vec2 size() const = 0;

    /**
     * Device pixels per logical pixel: 1 on an ordinary display, 2 on a
     * doubled one, and fractional on the scaled desktops in between.
     *
     * Hand it to `measureWith` and to `DisplayList::setScale`, and nothing
     * else in the application has to know about it.
     */
    virtual float scale() const = 0;

    virtual MouseState mouse() const = 0;

    /** Everything observed since the last call — pointer, wheel, keys and
     *  typed text — ready to hand to `Interaction::update`. Drained by reading. */
    virtual InputFrame takeInput() = 0;

    /** True when the window changed size since the last frame, so a caller can
     *  skip laying out again when nothing moved. */
    virtual bool resized() const = 0;

    /**
     * Sets the pointer, once a frame, from `Interaction::cursor()`.
     *
     * The node under the pointer decides what the cursor is — that is what
     * `Style::cursor` is for — so the only thing an application does with it is
     * hand it here. Calling with the cursor that is already showing is free:
     * the backend keeps the current one and does not ask the platform again.
     */
    virtual void setCursor(Cursor cursor) = 0;
};

}  // namespace gbui
