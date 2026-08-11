// Geometry primitives. Header-only and dependency-free on purpose: everything
// above this file — layout, hit testing, painting — is arithmetic over these
// four types, and keeping them trivial is what lets the layout engine be tested
// without a window, a GPU or a font.
#pragma once

#include <algorithm>
#include <cmath>

namespace gbui {

struct Vec2 {
    float x = 0;
    float y = 0;
};

/** Padding, margin and border widths — always in the CSS order. */
struct Edges {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;

    static Edges all(float v) { return {v, v, v, v}; }
    static Edges symmetric(float vertical, float horizontal) {
        return {vertical, horizontal, vertical, horizontal};
    }

    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
};

struct Rect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    float right() const { return x + width; }
    float bottom() const { return y + height; }

    bool contains(Vec2 p) const {
        return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
    }

    /** The area left after taking the edges out — a node's content box. */
    Rect deflate(const Edges& e) const {
        return {
            x + e.left,
            y + e.top,
            std::max(0.0f, width - e.horizontal()),
            std::max(0.0f, height - e.vertical()),
        };
    }

    Rect translated(float dx, float dy) const { return {x + dx, y + dy, width, height}; }

    /** The overlap of two rects, used for clipping. Empty when they miss. */
    Rect intersect(const Rect& o) const {
        const float nx = std::max(x, o.x);
        const float ny = std::max(y, o.y);
        const float nr = std::min(right(), o.right());
        const float nb = std::min(bottom(), o.bottom());
        if (nr <= nx || nb <= ny) return {nx, ny, 0, 0};
        return {nx, ny, nr - nx, nb - ny};
    }

    bool empty() const { return width <= 0 || height <= 0; }
};

/** Values that mean "not set". Layout treats NaN as auto, which keeps the style
 *  struct a plain aggregate instead of a pile of std::optional. */
inline constexpr float kAuto = std::numeric_limits<float>::quiet_NaN();
inline bool isAuto(float v) { return std::isnan(v); }
inline float resolve(float v, float fallback) { return isAuto(v) ? fallback : v; }

/**
 * A size that is either absolute or a share of something else.
 *
 * The implicit constructor from `float` is the whole reason this can be
 * introduced without touching a single call site: `.width = 240.0f` still means
 * 240 pixels, and only code that wants a share writes `Length::percent(25)`.
 * That matters because `Style` is an aggregate written inline all over the
 * place, and a type that broke those braces would be a rewrite rather than an
 * addition.
 *
 * A percentage resolves against the container's *content box* along the same
 * axis, as CSS does. When that basis is not known — an intrinsic pass, a
 * container sizing itself to its children — a percentage behaves as `auto`,
 * which is also CSS's rule and the only answer that cannot loop.
 */
struct Length {
    float value = kAuto;
    bool relative = false;  ///< `value` is a percentage of the basis, 0..100

    constexpr Length() = default;
    constexpr Length(float pixels) : value(pixels) {}  // NOLINT(*-explicit-constructor)

    static constexpr Length px(float pixels) { return Length{pixels}; }
    static constexpr Length percent(float share) {
        Length out;
        out.value = share;
        out.relative = true;
        return out;
    }
    /** Sized by its content, which is what `auto` means everywhere here. */
    static constexpr Length autoSize() { return Length{}; }

    bool isAuto() const { return gbui::isAuto(value); }

    /** Equality, so a caller can ask whether two sizes were written the same
     *  way. Deliberately the only operator: arithmetic on a length that might
     *  be a percentage is a question with no answer until it is resolved, and a
     *  type that quietly behaves like a float would hide exactly that. */
    friend bool operator==(const Length& a, const Length& b) {
        if (a.relative != b.relative) return false;
        if (a.isAuto() || b.isAuto()) return a.isAuto() && b.isAuto();
        return a.value == b.value;
    }
    friend bool operator!=(const Length& a, const Length& b) { return !(a == b); }

    /** The absolute size, or `kAuto` when a percentage has nothing to measure
     *  against. `basis` is the container's content size along this axis. */
    float resolve(float basis) const {
        if (!relative) return value;
        if (gbui::isAuto(value) || gbui::isAuto(basis) || std::isinf(basis)) return kAuto;
        return basis * value / 100.0f;
    }
};

}  // namespace gbui
