// A strip of slides, one screenful at a time.
//
// The content is a callback and the *position* is the component's business,
// which is what makes this a container rather than a composed thing: it decides
// which of the caller's slides are on screen and where, and has no opinion
// about what any of them contains.
//
// ---- one number, and everything follows from it -----------------------------
//
// `CarouselState::first` is the index of the slide at the leading edge, and it
// moves by *slides* rather than by pages even when several are showing. That is
// what a reader means by "next": the thing after the one they are looking at,
// not a jump that takes the whole screenful away and gives them nothing they
// were reading. Both conventions exist and this is the one that keeps a
// four-across gallery usable.
//
// ---- autoplay, and the button it is not allowed to ship without ------------
//
// Anything that moves on its own for more than five seconds needs a way to stop
// it — WCAG's "pause, stop, hide", and one of the few rules that is a rule
// rather than a judgement. So an autoplaying carousel *draws a pause button*,
// and there is no option to turn that off: the option would be a switch labelled
// "make this inaccessible". Hovering and focusing it pause it too, which is the
// unwritten half everybody expects and nobody asks for.
//
// ---- what a measured width is for ------------------------------------------
//
// A slide's size and the strip's offset are both in pixels, so both are read
// from last frame's viewport — the estimate-then-correct every positioned thing
// here makes. `compare` avoids it with percentages and this cannot: the offset
// is `-first × pitch`, and `left` is a distance rather than a share.
#pragma once

#include <cstddef>
#include <functional>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

/** Which way the strip runs. */
enum class CarouselOrientation { Horizontal, Vertical };

/** What the application remembers between frames. */
struct CarouselState {
    /** The slide at the leading edge. */
    std::size_t first = 0;
    /** Whether autoplay is running. The reader's answer to the pause button,
     *  which is why it is here rather than in the options. */
    bool playing = true;
    /** Seconds since the last automatic advance. The carousel's to write, the
     *  way a toast's `elapsed` is. */
    float elapsed = 0.0f;
};

struct CarouselOptions {
    /** Which way the strip runs, and therefore which pair of arrow keys moves
     *  it. */
    CarouselOrientation orientation = CarouselOrientation::Horizontal;
    /**
     * How many slides are on screen at once, and **a fraction is allowed**.
     *
     * 2.5 shows two and half of the next, which is not decoration: a strip cut
     * cleanly at the edge looks like it ends there, and half a slide is the
     * only thing that says there is more without spending a control on saying
     * it.
     */
    float slidesPerPage = 1.0f;
    /** Between two slides. Taken out of the room they share, so the arithmetic
     *  stays exact however many are showing. */
    float gap = 12.0f;
    /** Past the last slide is the first. Off, because a strip that silently
     *  starts over is a strip a reader cannot tell they have finished. */
    bool loop = false;
    /**
     * Seconds between automatic advances. Zero — the default — never moves.
     *
     * On, it also draws a pause button, and that is not optional: see the note
     * at the top of this header.
     */
    double autoplay = 0.0;
    /** The row of dots, which is also how a reader jumps straight to one. */
    bool indicators = true;
    /** The two arrows. Off for a strip driven entirely by its indicators or by
     *  the keyboard. */
    bool navigators = true;
    /** What the strip is of — "Screenshots", "Recent commits". */
    std::string_view name{};
    float height = 220.0f;
    float width = kAuto;
    float grow = 0.0f;
};

struct CarouselResult {
    /** The slide now at the leading edge. */
    std::size_t first = 0;
    bool changed = false;
    /** The reader pressed pause or play; `state.playing` already holds the
     *  answer, and this says it changed. */
    bool playingChanged = false;
};

/**
 * Draws `count` slides and shows the ones around `state.first`.
 *
 * `slide` is called for **every** index, not only the visible ones: a carousel
 * is a handful of things and the whole strip has to exist for the movement
 * between them to be a movement rather than a cut. A list long enough for that
 * to matter is a `virtualList`, not a carousel.
 *
 * `delta` is the frame's own seconds, and only autoplay reads it.
 */
CarouselResult carousel(Ui& ui, const Interaction& input, std::string_view id, std::size_t count,
                        CarouselState& state, float delta,
                        const std::function<void(Ui&, std::size_t)>& slide,
                        const CarouselOptions& options = {});

}  // namespace gbui
