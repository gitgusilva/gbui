#include "gbui/anim/easing.hpp"

#include <algorithm>
#include <cmath>

namespace gbui {
namespace {

constexpr float kPi = 3.14159265358979f;

/** One coordinate of a unit cubic Bézier at parameter `t`, with the ends fixed
 *  at 0 and 1. Expanded from the Bernstein form, which is the arrangement that
 *  costs three multiplies instead of ten. */
float bezierAxis(float t, float p1, float p2) {
    const float a = 1.0f + 3.0f * p1 - 3.0f * p2;
    const float b = 3.0f * p2 - 6.0f * p1;
    const float c = 3.0f * p1;
    return ((a * t + b) * t + c) * t;
}

float bezierSlope(float t, float p1, float p2) {
    const float a = 1.0f + 3.0f * p1 - 3.0f * p2;
    const float b = 3.0f * p2 - 6.0f * p1;
    const float c = 3.0f * p1;
    return (3.0f * a * t + 2.0f * b) * t + c;
}

/** Turns a fraction of the duration into the Bézier's own parameter.
 *
 * Newton-Raphson, which converges in a handful of steps for the curves anyone
 * actually writes, with bisection behind it for the ones where the slope goes
 * flat and Newton would wander off. */
float bezierParamFor(float x, float x1, float x2) {
    float t = x;
    for (int i = 0; i < 8; ++i) {
        const float error = bezierAxis(t, x1, x2) - x;
        if (std::fabs(error) < 1e-6f) return t;
        const float slope = bezierSlope(t, x1, x2);
        if (std::fabs(slope) < 1e-6f) break;
        t -= error / slope;
    }

    float low = 0.0f;
    float high = 1.0f;
    t = x;
    for (int i = 0; i < 32; ++i) {
        const float value = bezierAxis(t, x1, x2);
        if (std::fabs(value - x) < 1e-6f) break;
        if (value > x) high = t; else low = t;
        t = (low + high) / 2.0f;
    }
    return t;
}

/** Mirrors an "in" curve into its "out", and the pair into an "in-out". Written
 *  once so thirty curves are ten formulas rather than thirty. */
float outOf(float (*in)(float), float t) { return 1.0f - in(1.0f - t); }

float inOutOf(float (*in)(float), float t) {
    return t < 0.5f ? in(2.0f * t) / 2.0f : 1.0f - in(2.0f - 2.0f * t) / 2.0f;
}

float sineIn(float t) { return 1.0f - std::cos(t * kPi / 2.0f); }
float quadIn(float t) { return t * t; }
float cubicIn(float t) { return t * t * t; }
float quartIn(float t) { return t * t * t * t; }
float quintIn(float t) { return t * t * t * t * t; }
float expoIn(float t) { return t <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f); }
float circIn(float t) { return 1.0f - std::sqrt(std::max(0.0f, 1.0f - t * t)); }

/** The overshoot constant the classic set uses: about 10% past the target. */
float backIn(float t) {
    constexpr float kOvershoot = 1.70158f;
    return (kOvershoot + 1.0f) * t * t * t - kOvershoot * t * t;
}

float elasticIn(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    constexpr float kPeriod = 2.0f * kPi / 3.0f;
    return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * kPeriod);
}

/** Bounce is defined as the *out* curve — four arcs of decreasing height — and
 *  the others are mirrored from it. */
float bounceOut(float t) {
    constexpr float n = 7.5625f;
    constexpr float d = 2.75f;
    if (t < 1.0f / d) return n * t * t;
    if (t < 2.0f / d) {
        t -= 1.5f / d;
        return n * t * t + 0.75f;
    }
    if (t < 2.5f / d) {
        t -= 2.25f / d;
        return n * t * t + 0.9375f;
    }
    t -= 2.625f / d;
    return n * t * t + 0.984375f;
}

float bounceIn(float t) { return 1.0f - bounceOut(1.0f - t); }

}  // namespace

float ease(const CubicBezier& curve, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    // CSS clamps the control points' x to 0..1; outside it the curve is no
    // longer a function of time and there is no single answer to solve for.
    const float x1 = std::clamp(curve.x1, 0.0f, 1.0f);
    const float x2 = std::clamp(curve.x2, 0.0f, 1.0f);
    if (x1 == curve.y1 && x2 == curve.y2) return t;  // the identity, i.e. linear
    if (t <= 0.0f || t >= 1.0f) return t;
    return bezierAxis(bezierParamFor(t, x1, x2), curve.y1, curve.y2);
}

float ease(Easing curve, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (curve) {
        case Easing::Linear: return t;

        // The CSS keywords, as CSS defines them.
        case Easing::Ease: return ease(CubicBezier{0.25f, 0.1f, 0.25f, 1.0f}, t);
        case Easing::EaseIn: return ease(CubicBezier{0.42f, 0.0f, 1.0f, 1.0f}, t);
        case Easing::EaseOut: return ease(CubicBezier{0.0f, 0.0f, 0.58f, 1.0f}, t);
        case Easing::EaseInOut: return ease(CubicBezier{0.42f, 0.0f, 0.58f, 1.0f}, t);

        case Easing::SineIn: return sineIn(t);
        case Easing::SineOut: return outOf(sineIn, t);
        case Easing::SineInOut: return inOutOf(sineIn, t);

        case Easing::QuadIn: return quadIn(t);
        case Easing::QuadOut: return outOf(quadIn, t);
        case Easing::QuadInOut: return inOutOf(quadIn, t);

        case Easing::CubicIn: return cubicIn(t);
        case Easing::CubicOut: return outOf(cubicIn, t);
        case Easing::CubicInOut: return inOutOf(cubicIn, t);

        case Easing::QuartIn: return quartIn(t);
        case Easing::QuartOut: return outOf(quartIn, t);
        case Easing::QuartInOut: return inOutOf(quartIn, t);

        case Easing::QuintIn: return quintIn(t);
        case Easing::QuintOut: return outOf(quintIn, t);
        case Easing::QuintInOut: return inOutOf(quintIn, t);

        case Easing::ExpoIn: return expoIn(t);
        case Easing::ExpoOut: return outOf(expoIn, t);
        case Easing::ExpoInOut: return inOutOf(expoIn, t);

        case Easing::CircIn: return circIn(t);
        case Easing::CircOut: return outOf(circIn, t);
        case Easing::CircInOut: return inOutOf(circIn, t);

        case Easing::BackIn: return backIn(t);
        case Easing::BackOut: return outOf(backIn, t);
        case Easing::BackInOut: return inOutOf(backIn, t);

        case Easing::ElasticIn: return elasticIn(t);
        case Easing::ElasticOut: return outOf(elasticIn, t);
        case Easing::ElasticInOut: return inOutOf(elasticIn, t);

        case Easing::BounceIn: return bounceIn(t);
        case Easing::BounceOut: return bounceOut(t);
        case Easing::BounceInOut: return inOutOf(bounceIn, t);

        case Easing::Spring: {
            // A decaying cosine: one clear overshoot and a settle, rather than
            // the four or five a low damping gives — past two, an interface
            // reads as broken rather than lively.
            if (t >= 1.0f) return 1.0f;
            constexpr float kFrequency = 3.0f;
            constexpr float kDecay = 6.0f;
            return 1.0f - std::exp(-kDecay * t) * std::cos(kFrequency * t * kPi);
        }
    }
    return t;
}

}  // namespace gbui
