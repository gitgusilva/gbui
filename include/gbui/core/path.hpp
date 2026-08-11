// Vector paths: the SVG `d` subset the icon set uses, flattened to polylines.
//
// Curves are flattened here rather than in a backend so that every painter —
// software, SVG, a future GPU one — receives the same geometry and cannot
// disagree about the shape of a glyph-sized icon.
#pragma once

#include <string_view>
#include <vector>

#include "gbui/core/geometry.hpp"

namespace gbui {

/** A flattened outline: one or more contours of straight segments. */
class Path {
public:
    struct Contour {
        std::vector<Vec2> points;
        bool closed = false;
    };

    void moveTo(Vec2 point);
    void lineTo(Vec2 point);
    /** Cubic Bézier, flattened to `tolerance` device pixels of error. */
    void cubicTo(Vec2 control1, Vec2 control2, Vec2 end, float tolerance = 0.2f);
    void close();

    const std::vector<Contour>& contours() const { return contours_; }
    bool empty() const { return contours_.empty(); }
    void clear() { contours_.clear(); }

    /** The box every point falls inside. */
    Rect bounds() const;

    /** Same path scaled about the origin and moved — how a 24x24 icon becomes
     *  a 16 px one inside a row. */
    Path transformed(float scale, Vec2 offset) const;

private:
    Vec2 current_{};
    std::vector<Contour> contours_;
};

/**
 * Parses an SVG path `d` attribute.
 *
 * Supports M, L, H, V, C, S, Q, T, A and Z in both cases — the whole of the
 * path grammar except the parts no icon uses. Arcs matter: Lucide spells most
 * of its rounded corners as `a`, and a parser without them draws those icons in
 * pieces. An unsupported command stops the parse and returns what was read so
 * far rather than throwing, because a malformed icon should draw partially
 * instead of taking the frame down.
 */
Path parseSvgPath(std::string_view d, float tolerance = 0.2f);

}  // namespace gbui
