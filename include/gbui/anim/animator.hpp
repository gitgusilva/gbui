// The animation clock.
//
// The model is CSS's `transition`, not its `@keyframes`, and the difference is
// what keeps it compatible with a tree that is rebuilt every frame: a component
// never says "animate from here to there". It says **where the value should be
// now** — checked, hovered, open — and asks what it currently is. The animator
// remembers the difference between frames and closes it over time.
//
// That keeps components stateless, which is the whole architecture: the tag
// they already carry for hit testing is the same identity an animation is
// keyed by, so nothing new has to be threaded through a call site.
//
//     const float on = ui.animate(id, "on", checked ? 1.0f : 0.0f);
//     knob.left = on * travel;
//
// Nothing animates on the frame a key is first seen — a switch that is already
// on when the window opens does not slide into place — which is the rule CSS
// follows and the reason a list scrolled into view does not shimmer.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "gbui/anim/easing.hpp"
#include "gbui/core/color.hpp"

namespace gbui {

/** How a value travels once its target changes. The defaults are a short
 *  ease-out: long enough to be seen, short enough that a reader clicking twice
 *  never waits for it. */
struct Transition {
    /** Seconds. Zero makes the change immediate, which is how a component opts
     *  out without the call site changing shape. */
    float duration = 0.14f;
    float delay = 0.0f;
    Easing easing = Easing::EaseOut;
    /** A curve the named set does not have, as `cubic-bezier()` in CSS. Set,
     *  it wins over `easing`. */
    std::optional<CubicBezier> bezier{};

    float shape(float t) const { return bezier ? ease(*bezier, t) : ease(easing, t); }
};

class Animator {
public:
    /**
     * Advances every running animation. Call once a frame, *before* building
     * the tree, so what the components read is this frame's value.
     *
     * `deltaSeconds` is clamped: a window that was dragged, or a debugger that
     * stopped the process for a minute, would otherwise deliver one enormous
     * step and finish every animation at once.
     */
    void tick(float deltaSeconds);

    /** Where `property` of `id` is now, on its way to `target`. */
    float animate(std::string_view id, std::string_view property, float target,
                  const Transition& transition = {});

    /** The same, for a colour: each channel travels on its own, which is what
     *  makes a fade between two theme tokens read as a fade rather than a
     *  jump through grey. */
    Color animate(std::string_view id, std::string_view property, Color target,
                  const Transition& transition = {});

    /**
     * A one-shot: starts when `trigger` is true and reports how far through it
     * is, 0 at the moment it starts and 1 once it is over — which is also what
     * it reports when nothing is running, so `progress < 1` means "a pulse is
     * playing" and needs no second question.
     *
     * A transition is the wrong shape for this. A ripple, a flash, a shake are
     * not a value travelling to a new target; they are an *event* with a
     * lifetime, and they end where they began. Retriggering restarts it, which
     * is what a second click on the same button should do.
     */
    float pulse(std::string_view id, std::string_view property, bool trigger,
                const Transition& transition = {});

    /**
     * Remembers a number across frames, replacing it only when `set` is true.
     *
     * The companion to `pulse`: an effect that starts at the pointer has to
     * keep the point it started from for as long as it runs, and the component
     * that draws it is rebuilt from scratch every frame. Storing it here keeps
     * the component stateless rather than pushing another field into every
     * application's model.
     */
    float latch(std::string_view id, std::string_view property, float value, bool set);

    /**
     * Seconds since the animator started, advanced by `tick` with the same cap.
     *
     * For the things that *loop* rather than travel: a caret's blink, a
     * spinner. They have no target and never finish, so they are a phase read
     * off a clock rather than an animation — and having one clock here means an
     * application does not have to thread its own through every component.
     */
    float now() const { return clock_; }

    /** True while anything is still moving. An application that redraws only on
     *  change asks this to decide whether the next frame has to be drawn. */
    bool animating() const { return running_ > 0; }

    /** Forgets everything. For a view being torn down, so the next one to use
     *  the same tags does not inherit its positions. */
    void clear() { states_.clear(); }

    std::size_t size() const { return states_.size(); }

private:
    struct State {
        float from = 0.0f;
        float to = 0.0f;
        float current = 0.0f;
        float elapsed = 0.0f;
        Transition transition{};
        /** The tick this was last asked about, so anything that stopped being
         *  built — a row scrolled out of a virtualised list — is dropped
         *  instead of accumulating for the life of the process. */
        std::uint64_t touched = 0;
        bool done = true;
        /** A pulse runs on its own clock and ends where it started, so it is
         *  driven by `elapsed` alone rather than by a from/to pair. */
        bool oneShot = false;
    };

    float track(std::uint64_t key, float target, const Transition& transition);

    std::unordered_map<std::uint64_t, State> states_;
    std::uint64_t frame_ = 0;
    float clock_ = 0.0f;
    int running_ = 0;
};

}  // namespace gbui
