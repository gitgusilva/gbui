// A circle that turns while something is happening.
//
// The indeterminate half of `progressBar`, and a separate element rather than a
// flag on it for one reason: they are not interchangeable in a layout. A bar is
// a horizontal rule that wants a row of its own; a spinner is a glyph that goes
// *inside* things — in a button that is submitting, at the end of a row that is
// loading, in the corner of a panel that is refreshing. Every application ends
// up with both, and a caller choosing between them is choosing a shape.
//
// **Use a bar when you know how far along you are.** A spinner says only "still
// working", and a reader watching one has no idea whether to wait or leave.
// Where a fraction exists, `progressBar` is the better answer.
#pragma once

#include <string_view>

#include "gbui/scene/ui.hpp"

namespace gbui {

struct SpinnerOptions {
    float size = 16.0f;
    /** How thick the ring is. Zero takes a sixth of `size`, which keeps a
     *  large spinner from looking like a hairline and a small one from
     *  looking like a doughnut. */
    float thickness = 0.0f;
    Token color = Token::Accent;
    /** The part of the ring that is not the moving arc, behind it. */
    Token track = Token::Border;
    /**
     * What is happening — "Cloning", "Signing in".
     *
     * Announced as a live region, because a spinner is the one control whose
     * whole meaning is invisible to a reader who cannot see it turning. Empty
     * says nothing at all, which is right when the surrounding text already
     * says it or when a `busy` state on the container does.
     */
    std::string_view name{};
    /**
     * Where in its turn it is, in turns. Feed it a clock.
     *
     * The caller's, exactly as `progressBar`'s indeterminate form takes it, and
     * for the same reason: a component here holds no state, and "how long has
     * this been spinning" is state. One turn per second is `time`; half that
     * speed is `time * 0.5f`. The fractional part is all that is read, so a
     * clock that never resets is fine.
     */
    float phase = 0.0f;
};

/** A turning ring. */
NodeId spinner(Ui& ui, const SpinnerOptions& options = {});

}  // namespace gbui
