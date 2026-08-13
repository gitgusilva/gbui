// Painting, in two halves.
//
// A tree is turned into a DisplayList — a flat, ordered sequence of drawing
// commands with everything already resolved to absolute coordinates and
// concrete colours. Backends consume that list and know nothing about nodes,
// styles or themes.
//
// The split is what keeps a GPU out of the test suite: the SVG backend below
// produces a file that can be diffed or looked at, so layout and styling are
// verifiable long before a window exists.
#pragma once

#include <deque>
#include <utility>
#include <iosfwd>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "gbui/core/path.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/tree.hpp"
#include "gbui/style/theme.hpp"

namespace gbui {

/** A gradient with its colours already resolved against the theme. */
struct ResolvedGradient {
    GradientKind kind = GradientKind::Linear;
    float angle = 180.0f;
    std::vector<std::pair<float, Color>> stops;
};

/**
 * What a backend paints with: one colour, or a gradient.
 *
 * A struct rather than a variant, because every backend has to handle both and
 * `paint.isGradient()` reads better at each of those sites than a visitor.
 */
struct Paint {
    Color color{};
    ResolvedGradient gradient{};

    Paint() = default;
    Paint(Color c) : color(c) {}
    Paint(Color c, ResolvedGradient g) : color(c), gradient(std::move(g)) {}

    bool isGradient() const { return gradient.stops.size() >= 2; }

    /** The colour at a position along the gradient, 0 to 1. Linear between
     *  stops, and clamped outside the first and last. */
    Color at(float t) const;

    /** Multiplies the alpha of every stop — how opacity is applied to a whole
     *  subtree without touching the theme's colours. */
    void scaleAlpha(float factor);
};

struct FillRect {
    Rect rect;
    Paint paint;
    float radius = 0.0f;
};

struct StrokeRect {
    Rect rect;
    Paint paint;
    float width = 1.0f;
    float radius = 0.0f;
};

/** A picture, already fitted and ready to be put down. `box` may reach outside
 *  `clip` — `Cover` and `None` crop — so a backend draws the intersection. */
struct DrawImage {
    Rect box;
    Bitmap source;
    float radius = 0.0f;
    float opacity = 1.0f;
};

struct DrawText {
    Rect box;          ///< The content box the run is aligned inside.
    std::string_view text;
    Paint paint;
    std::string_view family;
    float size = 13.0f;
    FontWeight weight = FontWeight::Regular;
    FontSlant slant = FontSlant::Normal;
    TextAlign align = TextAlign::Start;
    float baseline = 0.0f;  ///< Offset from the top of `box` to the baseline.
};

/** Vector art — an icon, a chart, a graph lane. The geometry is already
 *  flattened and in absolute coordinates, so a backend only has to fill or
 *  stroke what it is handed. */
struct DrawPath {
    Path path;
    Paint paint;
    /** Zero fills the contours; anything else strokes them at that width. */
    float strokeWidth = 0.0f;
};

struct PushClip {
    Rect rect;
    float radius = 0.0f;
};

struct PopClip {};

using DrawCommand =
    std::variant<FillRect, StrokeRect, DrawText, DrawImage, DrawPath, PushClip, PopClip>;

/** An ordered, absolute, theme-resolved description of one frame. */
class DisplayList {
public:
    /**
     * Device pixels per logical pixel — the display's scale factor.
     *
     * Everything above this line works in **logical** units: layout, hit
     * testing and the input events all speak the same coordinates, which is
     * what makes a 200% display a property of the output and not something
     * every component has to know about. The conversion happens here, once, on
     * the way into the list, so there is exactly one multiply in the pipeline
     * and no component can forget it.
     */
    void setScale(float scale) { scale_ = scale > 0.0f ? scale : 1.0f; }
    float scale() const { return scale_; }

    void add(DrawCommand command) { commands_.push_back(scaled(std::move(command))); }
    const std::vector<DrawCommand>& commands() const { return commands_; }
    std::size_t size() const { return commands_.size(); }
    bool empty() const { return commands_.empty(); }
    void clear() {
        commands_.clear();
        owned_.clear();
    }
    void reserve(std::size_t n) { commands_.reserve(n); }

    /** Takes ownership of a string the list has to outlive — an elided run.
     *  A deque, so earlier views stay valid as more are added. */
    std::string_view own(std::string text) {
        owned_.push_back(std::move(text));
        return owned_.back();
    }

private:
    /** One command in device pixels. Geometry *and* stroke widths and font
     *  sizes: a border that stayed one pixel while everything around it
     *  doubled would be the giveaway. */
    DrawCommand scaled(DrawCommand command) const;

    std::vector<DrawCommand> commands_;
    std::deque<std::string> owned_;
    float scale_ = 1.0f;
};

/**
 * Walks a laid-out tree and records what to draw. Layout must have run first:
 * this reads frames, it does not compute them.
 *
 * `measure` has to be the same function layout was given. Recording decides
 * where a run is elided, and measuring it differently from the pass that sized
 * its box means text is cut while it still fits — or overflows a box that was
 * sized for less.
 */
void record(const Arena& arena, NodeId root, const Theme& theme, DisplayList& out,
            const MeasureText& measure = &approximateTextMetrics);

/** What every backend implements. Kept to five calls on purpose — a rasterizer,
 *  a GPU renderer and the SVG writer below all fit behind it. */
class Painter {
public:
    virtual ~Painter() = default;
    virtual void fillRect(const FillRect&) = 0;
    virtual void strokeRect(const StrokeRect&) = 0;
    virtual void drawText(const DrawText&) = 0;
    virtual void drawImage(const DrawImage&) = 0;
    virtual void drawPath(const DrawPath&) = 0;
    virtual void pushClip(const PushClip&) = 0;
    virtual void popClip() = 0;

    /** Replays a list in order. Backends rarely need to override this. */
    void paint(const DisplayList& list);
};

/** Writes SVG. Useful in three places: golden-image tests, design review
 *  without a build of the app, and documentation screenshots. */
class SvgPainter final : public Painter {
public:
    SvgPainter(float width, float height, Color background);

    void fillRect(const FillRect&) override;
    void strokeRect(const StrokeRect&) override;
    void drawText(const DrawText&) override;
    void drawImage(const DrawImage&) override;
    void drawPath(const DrawPath&) override;
    void pushClip(const PushClip&) override;
    void popClip() override;

    /** The finished document, including the closing tag. */
    std::string finish();

private:
    /** A colour, or a `url(#…)` after writing the gradient into the defs. */
    std::string paintReference(const Paint& paint, const Rect& box);

    float width_;
    float height_;
    Color background_;
    std::string defs_;
    std::string body_;
    int gradientCounter_ = 0;
    int clipCounter_ = 0;
    std::vector<int> openClips_;
};

}  // namespace gbui
