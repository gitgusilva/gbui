#include "gbui/paint/canvas.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace gbui {
namespace {

/** Signed distance from a point to a rounded rectangle: negative inside,
 *  positive outside. One function covers fills, strokes and every radius. */
float roundedRectDistance(float px, float py, const Rect& rect, float radius) {
    const float halfW = rect.width / 2.0f;
    const float halfH = rect.height / 2.0f;
    const float r = std::min(radius, std::min(halfW, halfH));
    const float dx = std::fabs(px - (rect.x + halfW)) - (halfW - r);
    const float dy = std::fabs(py - (rect.y + halfH)) - (halfH - r);
    const float outside = std::hypot(std::max(dx, 0.0f), std::max(dy, 0.0f));
    return outside + std::min(std::max(dx, dy), 0.0f) - r;
}

/** Coverage from a distance, over one pixel of falloff. */
float coverageFor(float distance) {
    return std::clamp(0.5f - distance, 0.0f, 1.0f);
}

/**
 * sRGB is not a linear measure of light, and blending as though it were is the
 * single most common way a software renderer gets text wrong.
 *
 * Half coverage of white over black is not the byte 128 — that is *far* brighter
 * than half the light. Interpolating the encoded bytes therefore darkens every
 * partly covered pixel, which is why light text on a dark background looks thin
 * and spindly while dark text on light looks fat: the antialiased edges of a
 * stem are exactly the pixels this gets wrong, and a stem at these sizes is
 * mostly edge.
 *
 * So the two ends are decoded to light, mixed there, and encoded back. The
 * tables make that four lookups and a multiply instead of two `pow` calls per
 * channel; the fully opaque path below never reaches them at all, which is what
 * keeps a filled panel as fast as it was.
 */
struct GammaTables {
    std::array<float, 256> toLinear{};
    /** Indexed by linear light scaled to the table's size. 4096 entries keeps
     *  the error under half a byte where sRGB is steepest, near black. */
    std::array<std::uint8_t, 4096> toSrgb{};

    GammaTables() {
        for (int i = 0; i < 256; ++i) {
            const float encoded = static_cast<float>(i) / 255.0f;
            toLinear[static_cast<std::size_t>(i)] =
                encoded <= 0.04045f ? encoded / 12.92f
                                    : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
        }
        for (std::size_t i = 0; i < toSrgb.size(); ++i) {
            const float linear = static_cast<float>(i) / static_cast<float>(toSrgb.size() - 1);
            const float encoded = linear <= 0.0031308f
                                      ? linear * 12.92f
                                      : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
            toSrgb[i] = static_cast<std::uint8_t>(std::lround(std::clamp(encoded, 0.0f, 1.0f) * 255.0f));
        }
    }
};

const GammaTables& gamma() {
    static const GammaTables tables;
    return tables;
}

std::uint8_t encodeSrgb(float linear) {
    const auto& table = gamma().toSrgb;
    // `+ 0.5` and a truncating cast rather than `lround`: this runs on every
    // channel of every partly covered pixel, and the two are the same answer.
    const float clamped = linear < 0.0f ? 0.0f : (linear > 1.0f ? 1.0f : linear);
    return table[static_cast<std::size_t>(clamped * static_cast<float>(table.size() - 1) + 0.5f)];
}

/**
 * A source colour with its channels already in linear light.
 *
 * The gamma round trip is per *pixel*, but the colour being drawn is per *draw
 * call* — so decoding it once and carrying it down the loop removes three table
 * lookups from every pixel of every translucent fill, which is most of what a
 * chart's shaded area costs.
 */
struct SourceColor {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    Color srgb{};
};

SourceColor linearise(Color color) {
    const auto& table = gamma().toLinear;
    return SourceColor{table[color.r], table[color.g], table[color.b], color};
}

void blendPixel(std::uint8_t* pixel, const SourceColor& source, float coverage) {
    const float alpha = source.srgb.a * coverage;
    if (alpha <= 0.0f) return;
    if (alpha >= 1.0f) {
        pixel[0] = source.srgb.r;
        pixel[1] = source.srgb.g;
        pixel[2] = source.srgb.b;
        pixel[3] = 255;
        return;
    }
    const auto& toLinear = gamma().toLinear;
    const float inverse = 1.0f - alpha;
    pixel[0] = encodeSrgb(source.red * alpha + toLinear[pixel[0]] * inverse);
    pixel[1] = encodeSrgb(source.green * alpha + toLinear[pixel[1]] * inverse);
    pixel[2] = encodeSrgb(source.blue * alpha + toLinear[pixel[2]] * inverse);
    // Alpha is coverage, not light: it is already linear and is not encoded.
    pixel[3] = static_cast<std::uint8_t>(255.0f * alpha + static_cast<float>(pixel[3]) * inverse);
}

/** For the pixels whose colour is not constant — a gradient. */
void blendPixel(std::uint8_t* pixel, Color color, float coverage) {
    blendPixel(pixel, linearise(color), coverage);
}

/** Squared distance from a point to a segment, and the winding contribution of
 *  that segment. Both come from the same walk, because a fill needs to know
 *  inside from outside and a stroke needs to know how far. */
float distanceToSegment(float px, float py, Vec2 a, Vec2 b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0f) return std::hypot(px - a.x, py - a.y);
    const float t = std::clamp(((px - a.x) * dx + (py - a.y) * dy) / lengthSquared, 0.0f, 1.0f);
    return std::hypot(px - (a.x + t * dx), py - (a.y + t * dy));
}

/** Nonzero winding: how many times the outline turns around the point. */
int windingContribution(float px, float py, Vec2 a, Vec2 b) {
    if (a.y <= py) {
        if (b.y > py && (b.x - a.x) * (py - a.y) - (px - a.x) * (b.y - a.y) > 0.0f) return 1;
    } else if (b.y <= py && (b.x - a.x) * (py - a.y) - (px - a.x) * (b.y - a.y) < 0.0f) {
        return -1;
    }
    return 0;
}

/** Where a point sits along a gradient, 0 to 1, within `box`.
 *
 * Linear projects onto the axis the angle points along; radial measures
 * distance from the centre against the half-diagonal, so the last stop lands
 * at the corners rather than at the edges. */
float gradientPosition(const ResolvedGradient& gradient, float px, float py, const Rect& box) {
    if (box.width <= 0.0f || box.height <= 0.0f) return 0.0f;
    const float cx = box.x + box.width / 2.0f;
    const float cy = box.y + box.height / 2.0f;

    if (gradient.kind == GradientKind::Radial) {
        const float radius = std::hypot(box.width, box.height) / 2.0f;
        return radius > 0.0f ? std::clamp(std::hypot(px - cx, py - cy) / radius, 0.0f, 1.0f) : 0.0f;
    }

    // CSS angles: 0 points up, and they turn clockwise.
    const float radians = (gradient.angle - 90.0f) * 3.14159265f / 180.0f;
    const float dx = std::cos(radians);
    const float dy = std::sin(radians);
    // The gradient line spans the projection of the box onto that direction.
    const float half = (std::fabs(dx) * box.width + std::fabs(dy) * box.height) / 2.0f;
    if (half <= 0.0f) return 0.0f;
    const float distance = (px - cx) * dx + (py - cy) * dy;
    return std::clamp(distance / (2.0f * half) + 0.5f, 0.0f, 1.0f);
}

/**
 * How much of a pixel a rounded clip lets through.
 *
 * A square clip needs none of this — the loops are already bounded by its
 * rectangle — so the radius is what decides whether this costs anything.
 */
float clipCoverage(float px, float py, const Clip& clip) {
    if (clip.radius <= 0.0f) return 1.0f;
    // Against the box the radius was written for, not against the intersection
    // — see `Clip`.
    return coverageFor(roundedRectDistance(px, py, clip.rounded, clip.radius));
}

/** How far a row is bitten into by the clip's corners, so a span that clears it
 *  can be filled without asking about every pixel. */
float clipInsetAt(float py, const Clip& clip) {
    if (clip.radius <= 0.0f) return 0.0f;
    const float halfH = clip.rounded.height / 2.0f;
    const float r = std::min(clip.radius, std::min(clip.rounded.width / 2.0f, halfH));
    const float dy = std::fabs(py - (clip.rounded.y + halfH)) - (halfH - r);
    if (dy <= 0.0f) return 0.0f;
    const float bite = r * r - dy * dy;
    return bite > 0.0f ? r - std::sqrt(bite) : r;
}

/** The colour to blend at one pixel. Solid paints skip the sampling. */
Color colorAt(const Paint& paint, float px, float py, const Rect& box) {
    if (!paint.isGradient()) return paint.color;
    return paint.at(gradientPosition(paint.gradient, px, py, box));
}

}  // namespace

void Canvas::drawPath(const Path& path, const Paint& paint, float strokeWidth, const Clip& clip) {
    if (path.empty() || (!paint.isGradient() && paint.color.a <= 0.0f)) return;
    const Rect gradientBox = path.bounds();

    const float halfWidth = strokeWidth / 2.0f;
    // The band a stroke covers reaches half its width beyond the geometry, plus
    // a pixel for the antialiased edge.
    const float margin = strokeWidth > 0.0f ? halfWidth + 1.0f : 1.0f;
    Rect bounds = path.bounds();
    bounds = Rect{bounds.x - margin, bounds.y - margin, bounds.width + margin * 2.0f,
                  bounds.height + margin * 2.0f}
                 .intersect(clip.rect);
    if (bounds.empty()) return;

    const int x0 = std::max(0, static_cast<int>(std::floor(bounds.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(bounds.y)));
    const int x1 = std::min(width_, static_cast<int>(std::ceil(bounds.right())));
    const int y1 = std::min(height_, static_cast<int>(std::ceil(bounds.bottom())));

    // ---- the segments, once ------------------------------------------------
    //
    // This used to ask every segment about every pixel in the path's bounding
    // box. For a 24-pixel icon with eight segments that is nothing; for a chart
    // — 90 samples across 600 pixels — it is seven million distance
    // computations for one line, and it cost 35 ms a path.
    //
    // A segment can only affect the rows its own y-range touches, so the rows
    // are walked with a list of the segments that reach them, and each pixel
    // asks only those. Building that list is rows × segments; the inner loop
    // becomes pixels × *active* segments, which for a polyline is two or three.
    struct Edge {
        Vec2 a;
        Vec2 b;
        float top;
        float bottom;
        float left;
        float right;
    };
    std::vector<Edge> edges;
    for (const auto& contour : path.contours()) {
        const std::size_t count = contour.points.size();
        if (count < 2) continue;
        const bool closing = contour.closed || strokeWidth <= 0.0f;
        const std::size_t last = closing ? count : count - 1;
        for (std::size_t i = 0; i < last; ++i) {
            const Vec2 a = contour.points[i];
            const Vec2 b = contour.points[(i + 1) % count];
            edges.push_back(Edge{a, b, std::min(a.y, b.y), std::max(a.y, b.y),
                                 std::min(a.x, b.x), std::max(a.x, b.x)});
        }
    }
    if (edges.empty()) return;

    const bool flat = !paint.isGradient();
    const SourceColor solid = linearise(paint.color);

    std::vector<const Edge*> active;
    active.reserve(edges.size());
    /** One row's accumulated stroke coverage, cleared as it is consumed. Only a
     *  stroke needs it, and allocating it for every fill as well showed up in
     *  the measurements immediately. */
    std::vector<float> coverageRow;
    if (strokeWidth > 0.0f) coverageRow.assign(static_cast<std::size_t>(std::max(0, width_)), 0.0f);

    // Crossings of one row, for the fill path below.
    struct Crossing {
        float x;
        int direction;
    };
    std::vector<Crossing> crossings;

    for (int y = y0; y < y1; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        active.clear();
        for (const Edge& edge : edges) {
            if (py + margin < edge.top || py - margin > edge.bottom) continue;
            active.push_back(&edge);
        }
        if (active.empty()) continue;

        std::uint8_t* row = pixels_.data() + static_cast<std::size_t>(y) * pitch();

        const bool stroking = strokeWidth > 0.0f;

        /** Coverage from the exact distance, for the pixels near an edge. */
        const auto edgeCoverage = [&](float px) {
            float nearest = std::numeric_limits<float>::infinity();
            int winding = 0;
            for (const Edge* edge : active) {
                // A cheap rectangle test before the square root: most edges on
                // an active row are nowhere near this column.
                if (px + margin >= edge->left && px - margin <= edge->right) {
                    nearest = std::min(nearest, distanceToSegment(px, py, edge->a, edge->b));
                }
                // A stroke has no inside, so it never asks.
                if (!stroking) winding += windingContribution(px, py, edge->a, edge->b);
            }
            return stroking ? std::clamp(halfWidth - nearest + 0.5f, 0.0f, 1.0f)
                            : std::clamp(0.5f - (winding != 0 ? -nearest : nearest), 0.0f, 1.0f);
        };

        const auto put = [&](int x, float coverage) {
            coverage *= clipCoverage(static_cast<float>(x) + 0.5f, py, clip);
            if (coverage <= 0.0f) return;
            if (flat) {
                blendPixel(row + static_cast<std::size_t>(x) * 4, solid, coverage);
            } else {
                blendPixel(row + static_cast<std::size_t>(x) * 4,
                           colorAt(paint, static_cast<float>(x) + 0.5f, py, gradientBox), coverage);
            }
        };

        if (stroking) {
            // Each edge writes into its *own* columns, and the row is blended
            // once at the end.
            //
            // Walking the row and asking every active edge about every column is
            // the wrong way round for a stroke: a chart's segment is nearly
            // horizontal, so it is active over a row it only touches for a few
            // pixels of x, and the rest of the row answers "nowhere near
            // anything". Coverage is the *maximum* over edges — the distance
            // function takes the nearest — so a shared buffer also gets the
            // joints right, where blending edge by edge would darken twice.
            int touchedFrom = x1;
            int touchedTo = x0;
            for (const Edge* edge : active) {
                const int from = std::max(x0, static_cast<int>(std::floor(edge->left - margin)));
                const int to = std::min(x1, static_cast<int>(std::ceil(edge->right + margin)) + 1);
                for (int x = from; x < to; ++x) {
                    const float px = static_cast<float>(x) + 0.5f;
                    const float coverage = std::clamp(
                        halfWidth - distanceToSegment(px, py, edge->a, edge->b) + 0.5f, 0.0f, 1.0f);
                    if (coverage <= 0.0f) continue;
                    float& stored = coverageRow[static_cast<std::size_t>(x)];
                    if (coverage > stored) stored = coverage;
                    touchedFrom = std::min(touchedFrom, x);
                    touchedTo = std::max(touchedTo, x + 1);
                }
            }
            for (int x = touchedFrom; x < touchedTo; ++x) {
                float& stored = coverageRow[static_cast<std::size_t>(x)];
                if (stored > 0.0f) put(x, stored);
                stored = 0.0f;  // cleared as it is consumed, so the next row starts empty
            }
            continue;
        }

        // ---- the fill, by scanline ----------------------------------------
        //
        // Asking every pixel which side of every edge it is on is the honest
        // definition of a fill and a quadratic way to compute one: a donut
        // wedge is forty edges over twenty thousand pixels. The row's crossings
        // say where the inside *starts and ends* instead, so the interior is a
        // span to write and only its two ends need the exact distance.
        crossings.clear();
        for (const Edge* edge : active) {
            const float top = std::min(edge->a.y, edge->b.y);
            const float bottom = std::max(edge->a.y, edge->b.y);
            if (py < top || py >= bottom) continue;  // half-open, so a shared vertex counts once
            const float t = (py - edge->a.y) / (edge->b.y - edge->a.y);
            crossings.push_back({edge->a.x + t * (edge->b.x - edge->a.x),
                                 edge->b.y > edge->a.y ? 1 : -1});
        }
        std::sort(crossings.begin(), crossings.end(),
                  [](const Crossing& a, const Crossing& b) { return a.x < b.x; });

        int winding = 0;
        float spanStart = 0.0f;
        for (const Crossing& crossing : crossings) {
            const int before = winding;
            winding += crossing.direction;
            if (before == 0 && winding != 0) {
                spanStart = crossing.x;
            } else if (before != 0 && winding == 0) {
                // One pixel of slack at each end goes through the exact
                // coverage; everything between is solid by definition.
                const int from = std::max(x0, static_cast<int>(std::floor(spanStart)) - 1);
                const int to = std::min(x1, static_cast<int>(std::ceil(crossing.x)) + 1);
                const int solidFrom = std::max(from, static_cast<int>(std::ceil(spanStart)) + 1);
                const int solidTo = std::min(to, static_cast<int>(std::floor(crossing.x)) - 1);
                for (int x = from; x < to; ++x) {
                    if (x >= solidFrom && x < solidTo) {
                        put(x, 1.0f);
                        continue;
                    }
                    put(x, edgeCoverage(static_cast<float>(x) + 0.5f));
                }
            }
        }
    }
}

void Canvas::resize(int width, int height) {
    const int wanted = std::max(0, width);
    const int tall = std::max(0, height);
    if (wanted == width_ && tall == height_) return;
    width_ = wanted;
    height_ = tall;
    // `resize`, not `assign`. Both give a buffer of the right length, and only
    // one of them writes over the whole of it: at 1280x824 that is four
    // megabytes zeroed on the way to a frame whose first act is to clear the
    // canvas anyway. The early return above matters more — this is called with
    // an unchanged size on every frame of every window that never resizes.
    pixels_.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4);
}

void Canvas::clear(Color color) {
    // One four-byte store per pixel rather than four one-byte ones. The compiler
    // will not fold the byte writes on its own — the array is `uint8_t` and it
    // has to assume they might overlap — and this runs over every pixel of every
    // frame, so it was a measurable slice of a repaint.
    const std::uint8_t pixel[4] = {color.r, color.g, color.b, 255};
    for (std::size_t i = 0; i + 4 <= pixels_.size(); i += 4) {
        std::memcpy(pixels_.data() + i, pixel, 4);
    }
}

void Canvas::fillRoundedRect(const Rect& rect, float radius, const Paint& paint,
                             const Clip& clip) {
    const Rect area = rect.intersect(clip.rect);
    if (area.empty() || (!paint.isGradient() && paint.color.a <= 0.0f)) return;

    const int x0 = std::max(0, static_cast<int>(std::floor(area.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(area.y)));
    const int x1 = std::min(width_, static_cast<int>(std::ceil(area.right())));
    const int y1 = std::min(height_, static_cast<int>(std::ceil(area.bottom())));

    const bool sharp = radius <= 0.0f;
    // A solid paint is the same colour at every pixel, so sampling it per pixel
    // is a call and a branch for an answer that cannot change.
    const bool flat = !paint.isGradient();
    const SourceColor solid = linearise(paint.color);
    // An opaque one is not even a blend: the destination cannot show through,
    // so the covered run is a pattern to be written rather than a colour to be
    // mixed. Worth separating because it is the common case *and* the expensive
    // one — a page background, a card, a table row are all opaque and all
    // large, and this is what lets the run be stored four bytes at a time
    // instead of through a call the compiler cannot vectorise past.
    const bool opaque = flat && paint.color.a >= 1.0f;
    const std::uint8_t pattern[4] = {paint.color.r, paint.color.g, paint.color.b, 255};

    // The corner geometry, so a row can say where it is fully covered without
    // asking the distance function about every pixel in it. Only the pixels
    // within a pixel of an edge can be partly covered; the rest of the span is
    // interior, and interior coverage is 1 by definition.
    const float halfW = rect.width / 2.0f;
    const float halfH = rect.height / 2.0f;
    const float r = std::min(radius, std::min(halfW, halfH));
    const float centreY = rect.y + halfH;

    for (int y = y0; y < y1; ++y) {
        std::uint8_t* row = pixels_.data() + static_cast<std::size_t>(y) * pitch();
        const float py = static_cast<float>(y) + 0.5f;

        // How far this row's straight span is inset by the corner arc. One
        // square root per row rather than one hypot per pixel.
        float inset = 0.0f;
        if (!sharp) {
            const float dy = std::fabs(py - centreY) - (halfH - r);
            if (dy > 0.0f) {
                const float bite = r * r - dy * dy;
                inset = bite > 0.0f ? r - std::sqrt(bite) : r;
            }
        }

        // The span that is at least a pixel inside every edge. Outside it the
        // exact distance still decides, which is what keeps the antialiasing
        // and the half-pixel edges identical to the straightforward loop.
        //
        // The row itself has to clear the top and bottom edges too: a row half a
        // pixel inside them is partly covered across its whole width, and
        // filling it solid draws a hard line where there should be a soft one.
        //
        // The clip's own corners bite into the row as well, and a span that
        // clears both is the only one that can be filled without asking.
        // The fast span has to clear the clip's *rounded* box as well as its
        // own, since that is where the corners it could be bitten by are.
        const float clipInset = clipInsetAt(py, clip);
        const bool rowInside = py >= rect.y + 1.0f && py <= rect.bottom() - 1.0f &&
                               (clip.radius <= 0.0f ||
                                (py >= clip.rounded.y + 1.0f && py <= clip.rounded.bottom() - 1.0f));
        const int solidFrom =
            std::max({x0, static_cast<int>(std::ceil(rect.x + inset + 1.0f)),
                      static_cast<int>(std::ceil(clip.rounded.x + clipInset + 1.0f))});
        const int solidTo =
            std::min({x1, static_cast<int>(std::floor(rect.right() - inset - 1.0f)),
                      static_cast<int>(std::floor(clip.rounded.right() - clipInset - 1.0f))});

        for (int x = x0; x < x1; ++x) {
            if (flat && rowInside && x >= solidFrom && x < solidTo) {
                // A run of fully covered pixels of one colour: fill it and skip
                // to its end. This is where a 360x680 panel spends its time.
                std::uint8_t* at = row + static_cast<std::size_t>(x) * 4;
                if (opaque) {
                    for (int n = solidTo - x; n > 0; --n, at += 4) {
                        std::memcpy(at, pattern, 4);
                    }
                } else {
                    for (int n = solidTo - x; n > 0; --n, at += 4) {
                        blendPixel(at, solid, 1.0f);
                    }
                }
                x = solidTo - 1;   // the for-loop's ++x steps past the last one
                continue;
            }
            // The +0.5 samples the pixel centre, which is what keeps a
            // half-pixel edge from looking like a whole one.
            const float px = static_cast<float>(x) + 0.5f;
            const float coverage =
                (sharp ? 1.0f : coverageFor(roundedRectDistance(px, py, rect, radius))) *
                clipCoverage(px, py, clip);
            if (flat) {
                blendPixel(row + static_cast<std::size_t>(x) * 4, solid, coverage);
            } else {
                blendPixel(row + static_cast<std::size_t>(x) * 4, colorAt(paint, px, py, rect),
                           coverage);
            }
        }
    }
}

void Canvas::blitImage(const Rect& dest, const Bitmap& source, float radius, float opacity,
                       const Clip& clip) {
    if (!source.valid() || dest.empty() || opacity <= 0.0f) return;
    const Rect area = dest.intersect(clip.rect);
    if (area.empty()) return;

    const int x0 = std::max(0, static_cast<int>(std::floor(area.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(area.y)));
    const int x1 = std::min(width_, static_cast<int>(std::ceil(area.right())));
    const int y1 = std::min(height_, static_cast<int>(std::ceil(area.bottom())));
    const auto pitchIn = static_cast<std::size_t>(std::max(0, source.pitch()));

    // Pixel centres map to pixel centres: the half-pixel on each side is what
    // keeps a picture drawn at its own size from being resampled at all, and a
    // scaled one from drifting half a texel towards the origin.
    const float scaleX = static_cast<float>(source.width) / dest.width;
    const float scaleY = static_cast<float>(source.height) / dest.height;

    for (int y = y0; y < y1; ++y) {
        std::uint8_t* row = pixels_.data() + static_cast<std::size_t>(y) * pitch();
        const float py = static_cast<float>(y) + 0.5f;
        const float v = (py - dest.y) * scaleY - 0.5f;
        const int v0 = static_cast<int>(std::floor(v));
        const float fy = v - static_cast<float>(v0);
        const int sy0 = std::clamp(v0, 0, source.height - 1);
        const int sy1 = std::clamp(v0 + 1, 0, source.height - 1);

        for (int x = x0; x < x1; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float coverage =
                (radius > 0.0f ? coverageFor(roundedRectDistance(px, py, dest, radius)) : 1.0f) *
                clipCoverage(px, py, clip);
            if (coverage <= 0.0f) continue;

            const float u = (px - dest.x) * scaleX - 0.5f;
            const int u0 = static_cast<int>(std::floor(u));
            const float fx = u - static_cast<float>(u0);
            const int sx0 = std::clamp(u0, 0, source.width - 1);
            const int sx1 = std::clamp(u0 + 1, 0, source.width - 1);

            const std::uint8_t* a = source.pixels + static_cast<std::size_t>(sy0) * pitchIn +
                                    static_cast<std::size_t>(sx0) * 4;
            const std::uint8_t* b = source.pixels + static_cast<std::size_t>(sy0) * pitchIn +
                                    static_cast<std::size_t>(sx1) * 4;
            const std::uint8_t* c = source.pixels + static_cast<std::size_t>(sy1) * pitchIn +
                                    static_cast<std::size_t>(sx0) * 4;
            const std::uint8_t* d = source.pixels + static_cast<std::size_t>(sy1) * pitchIn +
                                    static_cast<std::size_t>(sx1) * 4;

            const auto mix = [&](int channel) {
                const float top = static_cast<float>(a[channel]) * (1.0f - fx) +
                                  static_cast<float>(b[channel]) * fx;
                const float bottom = static_cast<float>(c[channel]) * (1.0f - fx) +
                                     static_cast<float>(d[channel]) * fx;
                return top * (1.0f - fy) + bottom * fy;
            };

            // The source's own alpha, the picture's opacity and the coverage of
            // the corner all multiply into one number, because they are the
            // same question asked three times: how much of this pixel is the
            // picture.
            const float alpha = mix(3) / 255.0f * opacity * coverage;
            if (alpha <= 0.0f) continue;
            const Color colour{static_cast<std::uint8_t>(mix(0) + 0.5f),
                               static_cast<std::uint8_t>(mix(1) + 0.5f),
                               static_cast<std::uint8_t>(mix(2) + 0.5f), 1.0f};
            blendPixel(row + static_cast<std::size_t>(x) * 4, colour, alpha);
        }
    }
}

void Canvas::strokeRoundedRect(const Rect& rect, float radius, float thickness, const Paint& paint,
                               const Clip& clip) {
    const Rect area = rect.intersect(clip.rect);
    if (area.empty() || thickness <= 0.0f) return;
    if (!paint.isGradient() && paint.color.a <= 0.0f) return;

    // The border sits inside the box, as a CSS border does, so the outline is
    // the band between the frame and the frame deflated by the thickness.
    const Rect inner{rect.x + thickness, rect.y + thickness,
                     std::max(0.0f, rect.width - thickness * 2.0f),
                     std::max(0.0f, rect.height - thickness * 2.0f)};
    const float innerRadius = std::max(0.0f, radius - thickness);

    const int x0 = std::max(0, static_cast<int>(std::floor(area.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(area.y)));
    const int x1 = std::min(width_, static_cast<int>(std::ceil(area.right())));
    const int y1 = std::min(height_, static_cast<int>(std::ceil(area.bottom())));

    // Everything more than the border's own thickness inside the box gets no
    // paint at all, so there is no reason to visit it. Below the corners the
    // shape is two vertical bands and nothing in between, which is most of a
    // panel's area and was most of this function's cost.
    const float guard = std::max(radius, thickness) + 1.0f;

    // Hoisted for the same reason the fill hoists it: a flat paint is one
    // colour, and asking for it per pixel costs a call and three gamma table
    // lookups an outline has no use for. A border is a thin shape over a long
    // perimeter, so that per-pixel cost was most of what one cost to draw.
    const bool flat = !paint.isGradient();
    const SourceColor solid = linearise(paint.color);

    for (int y = y0; y < y1; ++y) {
        std::uint8_t* row = pixels_.data() + static_cast<std::size_t>(y) * pitch();
        const float py = static_cast<float>(y) + 0.5f;

        const auto band = [&](int from, int to) {
            for (int x = std::max(x0, from); x < std::min(x1, to); ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float outer = coverageFor(roundedRectDistance(px, py, rect, radius));
                const float hole = coverageFor(roundedRectDistance(px, py, inner, innerRadius));
                const float coverage =
                    std::max(0.0f, outer - hole) * clipCoverage(px, py, clip);
                if (coverage <= 0.0f) continue;
                if (flat) {
                    blendPixel(row + static_cast<std::size_t>(x) * 4, solid, coverage);
                } else {
                    blendPixel(row + static_cast<std::size_t>(x) * 4,
                               colorAt(paint, px, py, rect), coverage);
                }
            }
        };

        // A row through the top or bottom *edge* is painted end to end, because
        // the edge is. A row merely level with a corner is not: the only paint
        // on it is the two arcs, and walking the twelve hundred pixels between
        // them asking each one whether it is inside a rounded rectangle was the
        // rest of what a card's outline cost.
        const float edge = thickness + 1.0f;
        if (py < rect.y + edge || py > rect.bottom() - edge) {
            band(x0, x1);
            continue;
        }
        const bool throughCorner = py < rect.y + guard || py > rect.bottom() - guard;
        const float reach = throughCorner ? guard : edge;
        band(x0, static_cast<int>(std::ceil(rect.x + reach)));
        band(static_cast<int>(std::floor(rect.right() - reach)), x1);
    }
}

void Canvas::blendCoverage(int x, int y, int w, int h, const std::uint8_t* coverage,
                           const Paint& paint, const Clip& clip, const Rect& gradientBox) {
    if (!coverage || w <= 0 || h <= 0) return;
    for (int row = 0; row < h; ++row) {
        const int py = y + row;
        if (py < 0 || py >= height_) continue;
        if (static_cast<float>(py) + 0.5f < clip.rect.y ||
            static_cast<float>(py) + 0.5f >= clip.rect.bottom())
            continue;
        std::uint8_t* destination = pixels_.data() + static_cast<std::size_t>(py) * pitch();
        for (int column = 0; column < w; ++column) {
            const int px = x + column;
            if (px < 0 || px >= width_) continue;
            if (static_cast<float>(px) + 0.5f < clip.rect.x ||
                static_cast<float>(px) + 0.5f >= clip.rect.right())
                continue;
            const std::uint8_t value = coverage[static_cast<std::size_t>(row) *
                                                    static_cast<std::size_t>(w) +
                                                static_cast<std::size_t>(column)];
            if (!value) continue;
            const float inside = clipCoverage(static_cast<float>(px) + 0.5f,
                                              static_cast<float>(py) + 0.5f, clip);
            if (inside <= 0.0f) continue;
            // The gradient spans the whole run, not each glyph, so a fade
            // across a heading is continuous rather than repeating per letter.
            blendPixel(destination + static_cast<std::size_t>(px) * 4,
                       colorAt(paint, static_cast<float>(px) + 0.5f,
                               static_cast<float>(py) + 0.5f, gradientBox),
                       static_cast<float>(value) / 255.0f * inside);
        }
    }
}

SoftwarePainter::SoftwarePainter(Canvas& canvas, FontDatabase& fonts, const Typography& typography)
    : canvas_(canvas), fonts_(fonts), typography_(typography) {
    clips_.push_back(Clip{Rect{0, 0, static_cast<float>(canvas.width()),
                               static_cast<float>(canvas.height())}, 0.0f});
}

void SoftwarePainter::fillRect(const FillRect& command) {
    canvas_.fillRoundedRect(command.rect, command.radius, command.paint, clip());
}

void SoftwarePainter::strokeRect(const StrokeRect& command) {
    canvas_.strokeRoundedRect(command.rect, command.radius, command.width, command.paint, clip());
}

void SoftwarePainter::drawImage(const DrawImage& command) {
    canvas_.blitImage(command.box, command.source, command.radius, command.opacity, clip());
}

void SoftwarePainter::drawText(const DrawText& command) {
    TextStyle style;
    style.size = command.size;
    style.weight = command.weight;
    style.slant = command.slant;
    // The family the recorder resolved decides which role this run belongs to;
    // comparing against the typography avoids threading the role through the
    // display list for the sake of one lookup.
    style.role = command.family == typography_.monoFont    ? FontRole::Mono
                 : command.family == typography_.editorFont ? FontRole::Editor
                                                            : FontRole::Ui;

    const std::shared_ptr<Font> font = fontFor(fonts_, style, typography_);
    if (!font) return;

    const TextMetrics metrics = font->measure(command.text);
    float penX = command.box.x;
    if (command.align == TextAlign::Center) {
        penX = command.box.x + (command.box.width - metrics.width) / 2.0f;
    } else if (command.align == TextAlign::End) {
        penX = command.box.right() - metrics.width;
    }
    const float baseline = command.box.y + command.baseline;
    // The box a gradient is measured across: the run itself, not the node.
    const Rect runBox{penX, command.box.y, metrics.width, command.box.height};

    std::size_t cursor = 0;
    while (cursor < command.text.size()) {
        const char32_t codepoint = nextCodepoint(command.text, cursor);
        if (codepoint == 0) break;
        const Glyph* glyph = font->glyph(codepoint);
        if (!glyph) continue;
        if (!glyph->coverage.empty()) {
            canvas_.blendCoverage(static_cast<int>(std::lround(penX)) + glyph->bearingX,
                                  static_cast<int>(std::lround(baseline)) - glyph->bearingY,
                                  glyph->width, glyph->height, glyph->coverage.data(),
                                  command.paint, clip(), runBox);
        }
        penX += glyph->advance;
    }
}

void SoftwarePainter::drawPath(const DrawPath& command) {
    canvas_.drawPath(command.path, command.paint, command.strokeWidth, clip());
}

void SoftwarePainter::pushClip(const PushClip& command) {
    clips_.push_back(clip().intersect(Clip{command.rect, command.radius}));
}

void SoftwarePainter::popClip() {
    if (clips_.size() > 1) clips_.pop_back();
}

}  // namespace gbui
