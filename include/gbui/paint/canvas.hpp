// A software rasteriser and the Painter that drives it.
//
// It exists because the alternative on a machine with no Cairo and no Skia is
// no window at all, and because a CPU target keeps the toolkit honest: if the
// UI is fast enough rasterised by hand, it is fast enough anywhere. Filled and
// stroked rounded rectangles are antialiased through a signed-distance
// coverage; glyphs arrive from the font module as 8-bit coverage and are
// blended the same way.
//
// A GPU backend would implement Painter directly and never touch this file.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "gbui/core/color.hpp"
#include "gbui/core/geometry.hpp"
#include "gbui/core/path.hpp"
#include <algorithm>

#include "gbui/paint/paint.hpp"
#include "gbui/platform/font.hpp"

namespace gbui {

/**
 * The region a draw is confined to.
 *
 * A rectangle *and* a radius, because the boxes this toolkit clips against are
 * rounded: a panel, a scroll viewport, a button holding a ripple. Clipping a
 * round box with a square region lets whatever is inside it show through the
 * corners, which is exactly what it looks like.
 */
struct Clip {
    Rect rect;
    float radius = 0.0f;

    /** The intersection, keeping the tighter corner. Two rounded clips that
     *  actually overlap at their corners are rare enough that the larger radius
     *  is the honest approximation and the alternative is a region algebra. */
    Clip intersect(const Clip& other) const {
        return {rect.intersect(other.rect), std::max(radius, other.radius)};
    }
};

/** A premultiplied-alpha RGBA8 framebuffer. Rows are contiguous. */
class Canvas {
public:
    Canvas() = default;
    Canvas(int width, int height) { resize(width, height); }

    void resize(int width, int height);
    void clear(Color color);

    int width() const { return width_; }
    int height() const { return height_; }
    /** Row-major RGBA, ready to hand to a texture upload. */
    const std::uint8_t* pixels() const { return pixels_.data(); }
    std::uint8_t* pixels() { return pixels_.data(); }
    std::size_t pitch() const { return static_cast<std::size_t>(width_) * 4; }

    /** Antialiased rounded rectangle. A radius of zero is a plain rectangle.
     *  A gradient paint is sampled per pixel across `rect`. */
    void fillRoundedRect(const Rect& rect, float radius, const Paint& paint, const Clip& clip);
    void strokeRoundedRect(const Rect& rect, float radius, float thickness, const Paint& paint,
                           const Clip& clip);
    /** Strokes or fills a flattened path. Coverage comes from the distance to
     *  the geometry, which is what gives an icon the same soft edge a browser
     *  gives it, without a second rasteriser. */
    void drawPath(const Path& path, const Paint& paint, float strokeWidth, const Clip& clip);

    /** Blends an 8-bit coverage bitmap — a glyph — at an integer position. */
    void blendCoverage(int x, int y, int w, int h, const std::uint8_t* coverage, const Paint& paint,
                       const Clip& clip, const Rect& gradientBox);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> pixels_;
};

/** Replays a DisplayList onto a Canvas. */
class SoftwarePainter final : public Painter {
public:
    SoftwarePainter(Canvas& canvas, FontDatabase& fonts, const Typography& typography);

    void fillRect(const FillRect&) override;
    void strokeRect(const StrokeRect&) override;
    void drawText(const DrawText&) override;
    void drawPath(const DrawPath&) override;
    void pushClip(const PushClip&) override;
    void popClip() override;

private:
    const Clip& clip() const { return clips_.back(); }

    Canvas& canvas_;
    FontDatabase& fonts_;
    Typography typography_;
    std::vector<Clip> clips_;
};

}  // namespace gbui
