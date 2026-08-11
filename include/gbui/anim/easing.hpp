// Easing curves, by the names CSS and the classic set give them.
//
// A curve is the difference between something moving and something reading as
// physical. `EaseOut` is the right default in an interface: a control that
// leaves fast and arrives slowly feels like it is responding to you, while one
// that eases in feels like it hesitated.
//
// The four CSS keywords are the cubic Béziers CSS actually specifies, solved
// rather than approximated — `Easing::EaseIn` is `cubic-bezier(0.42, 0, 1, 1)`,
// not `t³`, which is a visibly different curve. The families below them are the
// usual set every animation library ships, so a caller reaching for
// "easeOutBack" finds it here instead of writing it again.
#pragma once

namespace gbui {

enum class Easing {
    Linear,

    // The CSS keywords. `Ease` is CSS's default and is not symmetric.
    Ease,
    EaseIn,
    EaseOut,
    EaseInOut,

    // Gentle. A quarter-turn of a sine — the softest useful curve.
    SineIn, SineOut, SineInOut,
    // Polynomial, in increasing severity. Quad is the everyday choice; by
    // Quint the start is almost stationary.
    QuadIn, QuadOut, QuadInOut,
    CubicIn, CubicOut, CubicInOut,
    QuartIn, QuartOut, QuartInOut,
    QuintIn, QuintOut, QuintInOut,
    // Severe. Useful for something entering from off screen.
    ExpoIn, ExpoOut, ExpoInOut,
    // Circular: flat then vertical, or the reverse.
    CircIn, CircOut, CircInOut,

    // These three leave the 0..1 range, which is the point of them. Anything
    // animating a *size* or an *opacity* with one of these will overshoot past
    // the value it was given — fine for a position, wrong for an alpha.
    BackIn, BackOut, BackInOut,
    ElasticIn, ElasticOut, ElasticInOut,
    BounceIn, BounceOut, BounceInOut,

    /** Overshoots once and settles. Kept separate from `ElasticOut`, which
     *  oscillates several times; this is the one for something arriving. */
    Spring,
};

/** Maps a progress in 0..1 to an eased progress. The last three families
 *  deliberately return values outside that range. */
float ease(Easing curve, float t);

/**
 * An arbitrary curve, as `cubic-bezier()` in CSS: the two control points of a
 * unit Bézier whose ends are fixed at (0,0) and (1,1).
 *
 * The escape hatch for a curve the named set does not have — a designer handing
 * over four numbers from a tool should not need a new enum value.
 */
struct CubicBezier {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 1.0f;
    float y2 = 1.0f;
};

/** Solves `curve` for the y at a given x, which is what CSS means by a timing
 *  function: x is the fraction of the duration, y is the fraction of the way
 *  there. Control points outside 0..1 on x are clamped, as CSS requires — a
 *  non-monotonic timing function has no single answer. */
float ease(const CubicBezier& curve, float t);

}  // namespace gbui
