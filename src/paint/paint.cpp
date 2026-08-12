#include "gbui/paint/paint.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "gbui/layout/layout.hpp"
#include "gbui/layout/textWrap.hpp"

namespace gbui {
namespace {

std::string number(float v) {
    // Two decimals: enough for half-pixel borders, short enough that a golden
    // SVG stays readable in a diff.
    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.2f", v);
    std::string s(buffer.data());
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s.empty() ? "0" : s;
}

std::string escapeXml(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out.push_back(c);
        }
    }
    return out;
}

/** Longest prefix of `text` that fits in `available` once an ellipsis is added.
 *  Cuts on UTF-8 boundaries, so a multi-byte character is never split in half. */
std::string elide(const MeasureText& measure, std::string_view text, const TextStyle& style,
                  const Typography& typography, float available) {
    static constexpr std::string_view kEllipsis = "\xe2\x80\xa6";  // U+2026

    const float ellipsisWidth =
        measure(kEllipsis, style, typography, std::numeric_limits<float>::infinity()).width;
    const float budget = available - ellipsisWidth;
    if (budget <= 0.0f) return std::string(kEllipsis);

    std::size_t fits = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        // Only consider cuts at a character boundary.
        if (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0U) == 0x80U) continue;
        const float width =
            measure(text.substr(0, i), style, typography, std::numeric_limits<float>::infinity())
                .width;
        if (width > budget) break;
        fits = i;
    }
    return std::string(text.substr(0, fits)) + std::string(kEllipsis);
}

/**
 * Draws a wrapped run as one command per line.
 *
 * The lines come from the same `wrapText` layout used to size the box, fed the
 * same measurer — which is the whole reason that function is shared. Breaking
 * the text a second way here is how a paragraph ends up drawn on four lines in
 * a box that was made three lines tall.
 */
template <typename Emit>
void recordWrapped(const Node& node, const Typography& typography, const MeasureText& measure,
                   DisplayList& out, const Emit& emit) {
    // One line at a time, so a measurer that would report the *wrapped* height
    // reports this line's width instead.
    TextStyle lineStyle = node.textStyle;
    lineStyle.overflow = TextOverflow::Clip;
    const auto lineWidth = [&](std::string_view run) {
        return measure(run, lineStyle, typography, std::numeric_limits<float>::infinity()).width;
    };

    // The same policy the measurer used. They must agree: a box sized by a
    // measure that broke anywhere, painted by one that only broke at spaces,
    // overflows the box it was given.
    const WrappedText wrapped = wrapText(node.text, node.content.width, node.textStyle.maxLines,
                                         lineWidth, node.textStyle.wordBreak);
    if (wrapped.lines.empty()) return;

    const TextMetrics block = measure(node.text, node.textStyle, typography, node.content.width);
    const float step = block.height / static_cast<float>(wrapped.lines.size());
    // The block is centred in the content box; the lines then stack from there,
    // so a one-line run lands exactly where an unwrapped one would.
    const float top = (node.content.height - block.height) / 2.0f;

    for (std::size_t i = 0; i < wrapped.lines.size(); ++i) {
        const bool last = i + 1 == wrapped.lines.size();
        std::string_view run = wrapped.lines[i].text;
        if (last && wrapped.truncated && node.content.width > 0.0f) {
            run = out.own(elide(measure, run, lineStyle, typography, node.content.width));
        }
        const Rect lineBox{node.content.x, node.content.y + top + static_cast<float>(i) * step,
                           node.content.width, step};
        emit(lineBox, run, block.baseline);
    }
}

float resolveRadius(const Style& style, const Theme& theme) {
    return isAuto(style.radius) ? theme.typography().radius : style.radius;
}

float fontSizeFor(const TextStyle& style, const Typography& typography) {
    if (!isAuto(style.size)) return style.size;
    return style.role == FontRole::Editor ? typography.editorFontSize : typography.uiFontSize;
}

std::string_view familyFor(const TextStyle& style, const Typography& typography) {
    switch (style.role) {
        case FontRole::Mono:
            return typography.monoFont;
        case FontRole::Editor:
            return typography.editorFont;
        case FontRole::Ui:
            break;
    }
    return typography.uiFont;
}

}  // namespace

namespace {

Rect scaleRect(const Rect& rect, float s) {
    return {rect.x * s, rect.y * s, rect.width * s, rect.height * s};
}

}  // namespace

DrawCommand DisplayList::scaled(DrawCommand command) const {
    if (scale_ == 1.0f) return command;
    const float s = scale_;
    return std::visit(
        [&](auto&& c) -> DrawCommand {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, FillRect>) {
                c.rect = scaleRect(c.rect, s);
                c.radius *= s;
            } else if constexpr (std::is_same_v<T, StrokeRect>) {
                c.rect = scaleRect(c.rect, s);
                c.radius *= s;
                c.width *= s;
            } else if constexpr (std::is_same_v<T, DrawText>) {
                c.box = scaleRect(c.box, s);
                c.size *= s;
                c.baseline *= s;
            } else if constexpr (std::is_same_v<T, DrawPath>) {
                c.path = c.path.transformed(s, Vec2{0.0f, 0.0f});
                c.strokeWidth *= s;
            } else if constexpr (std::is_same_v<T, PushClip>) {
                c.rect = scaleRect(c.rect, s);
                c.radius *= s;
            }
            return c;
        },
        std::move(command));
}

Color Paint::at(float t) const {
    if (gradient.stops.empty()) return color;
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= gradient.stops.front().first) return gradient.stops.front().second;
    if (t >= gradient.stops.back().first) return gradient.stops.back().second;

    for (std::size_t i = 1; i < gradient.stops.size(); ++i) {
        const auto& [end, endColor] = gradient.stops[i];
        if (t > end) continue;
        const auto& [start, startColor] = gradient.stops[i - 1];
        const float span = end - start;
        const float k = span > 0.0f ? (t - start) / span : 0.0f;
        // Interpolated in sRGB, which is what CSS does. Perceptually it is not
        // the best space, and moving to Oklab later changes only this line.
        return Color{
            static_cast<std::uint8_t>(startColor.r + (endColor.r - startColor.r) * k),
            static_cast<std::uint8_t>(startColor.g + (endColor.g - startColor.g) * k),
            static_cast<std::uint8_t>(startColor.b + (endColor.b - startColor.b) * k),
            startColor.a + (endColor.a - startColor.a) * k,
        };
    }
    return gradient.stops.back().second;
}

void Paint::scaleAlpha(float factor) {
    color.a *= factor;
    for (auto& stop : gradient.stops) stop.second.a *= factor;
}

/** Resolves a style's gradient against the theme, or nothing when it has none. */
ResolvedGradient resolveGradient(const Gradient& gradient, const Theme& theme) {
    ResolvedGradient out;
    if (gradient.empty()) return out;
    out.kind = gradient.kind;
    out.angle = gradient.angle;
    out.stops.reserve(gradient.stops.size());
    for (const GradientStop& stop : gradient.stops) {
        out.stops.emplace_back(stop.position, stop.color.resolve(theme));
    }
    return out;
}

void Painter::paint(const DisplayList& list) {
    for (const auto& command : list.commands()) {
        std::visit(
            [this](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, FillRect>)
                    fillRect(c);
                else if constexpr (std::is_same_v<T, StrokeRect>)
                    strokeRect(c);
                else if constexpr (std::is_same_v<T, DrawText>)
                    drawText(c);
                else if constexpr (std::is_same_v<T, DrawPath>)
                    drawPath(c);
                else if constexpr (std::is_same_v<T, PushClip>)
                    pushClip(c);
                else if constexpr (std::is_same_v<T, PopClip>)
                    popClip();
            },
            command);
    }
}

namespace {

/** Records one subtree for one layer.
 *
 * `inherited` is the layer the subtree already belongs to: a node inside a
 * popover is part of that popover's layer even though its own style never
 * mentions one. Without inheriting it, the popover's own background painted in
 * the overlay pass and its contents were skipped by both — an empty box. */
void recordLayer(const Arena& arena, NodeId root, const Theme& theme, DisplayList& out,
                 const MeasureText& measure, Layer layer, Layer inherited);

}  // namespace

void record(const Arena& arena, NodeId root, const Theme& theme, DisplayList& out,
            const MeasureText& measure) {
    // Painting a tree is depth-first, so a popup opened inside a sidebar would
    // be painted under the pane beside it. Three passes fix that without a sort:
    // content, then overlays, then modals, each in tree order.
    for (const Layer layer : {Layer::Content, Layer::Overlay, Layer::Modal}) {
        recordLayer(arena, root, theme, out, measure, layer, Layer::Content);
    }
}

namespace {

void recordLayer(const Arena& arena, NodeId root, const Theme& theme, DisplayList& out,
                 const MeasureText& measure, Layer layer, Layer inherited) {
    if (!root.valid()) return;
    const Node& node = arena[root];
    const Style& style = node.style;

    if (style.opacity <= 0.0f) return;  // fully transparent subtrees cost nothing

    // A node belongs to the highest layer between its own and the one it sits
    // inside, so raising a layer carries the whole subtree with it.
    const Layer effective = style.layer > inherited ? style.layer : inherited;
    if (effective != layer) {
        // Belongs to a later pass: leave it entirely, children included.
        if (effective > layer) return;
        for (NodeId child = node.firstChild; child.valid(); child = arena[child].nextSibling) {
            recordLayer(arena, child, theme, out, measure, layer, effective);
        }
        return;
    }

    const float radius = resolveRadius(style, theme);

    if (!style.background.empty() || !style.backgroundGradient.empty()) {
        Paint paint{style.background.resolve(theme),
                    resolveGradient(style.backgroundGradient, theme)};
        paint.scaleAlpha(style.opacity);
        out.add(FillRect{node.frame, paint, radius});
    }

    if (style.border.visible()) {
        Paint paint{style.border.color.resolve(theme)};
        paint.scaleAlpha(style.opacity);
        out.add(StrokeRect{node.frame, paint, style.border.width, radius});
    }

    // The outline sits outside the border box and takes no space, so it can
    // appear on focus without moving anything.
    if (style.outline.visible()) {
        const float spread = style.outline.offset + style.outline.width / 2.0f;
        const Rect around{node.frame.x - spread, node.frame.y - spread,
                          node.frame.width + spread * 2.0f, node.frame.height + spread * 2.0f};
        Paint paint{style.outline.color.resolve(theme)};
        paint.scaleAlpha(style.opacity);
        out.add(StrokeRect{around, paint, style.outline.width, radius + spread});
    }

    // An icon is path data on Lucide's 24-unit grid, scaled to fit its content
    // box and centred in it.
    if (!node.icon.path.empty()) {
        Paint iconPaint{node.icon.color.resolve(theme)};
        iconPaint.scaleAlpha(style.opacity);
        const float side = std::min(node.content.width, node.content.height);
        const float scale = side / 24.0f;
        const Vec2 offset{node.content.x + (node.content.width - side) / 2.0f,
                          node.content.y + (node.content.height - side) / 2.0f};
        Path path = parseSvgPath(node.icon.path).transformed(scale, offset);
        out.add(DrawPath{std::move(path), iconPaint, node.icon.stroke * scale});
    }

    // Vector art the application built, in this node's own coordinates. Drawn
    // after the background and before the children, so a chart's grid can sit
    // under labels the caller adds as children.
    for (std::uint32_t i = 0; i < node.shapeCount; ++i) {
        const Shape& shape = arena.shape(node.firstShape + i);
        if (shape.path.empty()) continue;
        Paint paint{shape.color.resolve(theme), resolveGradient(shape.gradient, theme)};
        paint.scaleAlpha(style.opacity);
        out.add(DrawPath{shape.path.transformed(1.0f, Vec2{node.content.x, node.content.y}), paint,
                         shape.stroke});
    }

    if (!node.text.empty()) {
        Paint textPaint{node.textStyle.color.resolve(theme),
                        resolveGradient(node.textStyle.colorGradient, theme)};
        textPaint.scaleAlpha(style.opacity);
        const Typography& typography = theme.typography();
        const float size = fontSizeFor(node.textStyle, typography);
        const auto emit = [&](const Rect& box, std::string_view run, float baseline) {
            out.add(DrawText{box, run, textPaint, familyFor(node.textStyle, typography), size,
                             node.textStyle.weight, node.textStyle.slant, node.textStyle.align,
                             baseline});

            // The rules go over each *line*, so a wrapped paragraph is
            // underlined line by line rather than by one bar across the block.
            const TextDecoration& decoration = node.textStyle.decoration;
            if (!decoration.any() || run.empty()) return;

            // Where the line actually starts: `DrawText` is aligned inside its
            // box by the backend, and a rule has to sit under the glyphs rather
            // than under the box.
            TextStyle lineStyle = node.textStyle;
            lineStyle.overflow = TextOverflow::Clip;
            const float runWidth =
                measure(run, lineStyle, typography, std::numeric_limits<float>::infinity()).width;
            float x = box.x;
            if (node.textStyle.align == TextAlign::Center) {
                x = box.x + (box.width - runWidth) / 2.0f;
            } else if (node.textStyle.align == TextAlign::End) {
                x = box.right() - runWidth;
            }

            // Proportional to the size, so the rule under 11-pixel text is not
            // as heavy as the one under a heading.
            const float thickness =
                decoration.thickness > 0.0f ? decoration.thickness : std::max(1.0f, size / 14.0f);
            Paint rulePaint{decoration.color.empty() ? node.textStyle.color.resolve(theme)
                                                     : decoration.color.resolve(theme)};
            rulePaint.scaleAlpha(style.opacity);

            if (decoration.underline) {
                // Just below the baseline, where a font's own underline sits.
                const float y = box.y + baseline + std::max(1.0f, size * 0.11f);
                out.add(FillRect{Rect{x, y, runWidth, thickness}, rulePaint, 0.0f});
            }
            if (decoration.strikeThrough) {
                // Through the middle of the lowercase, not the middle of the
                // line box: a line box includes the descender and a rule placed
                // by it sits visibly low.
                const float y = box.y + baseline - size * 0.28f;
                out.add(FillRect{Rect{x, y, runWidth, thickness}, rulePaint, 0.0f});
            }
        };

        if (node.textStyle.overflow == TextOverflow::Wrap) {
            recordWrapped(node, typography, measure, out, emit);
        } else {
            auto metrics = measure(node.text, node.textStyle, typography, node.content.width);
            std::string_view painted = node.text;
            // Half a pixel of slack: the box was sized from this same
            // measurement, but the number went through the layout's arithmetic
            // on the way, and an exact comparison elides text that fits
            // perfectly.
            constexpr float kOverflowSlack = 0.5f;
            if (node.textStyle.overflow == TextOverflow::Ellipsis &&
                metrics.width > node.content.width + kOverflowSlack && node.content.width > 0.0f) {
                painted = out.own(
                    elide(measure, node.text, node.textStyle, typography, node.content.width));
                metrics = measure(painted, node.textStyle, typography, node.content.width);
            }
            // Vertically centred in the content box, which is what every label,
            // button and list row in the app does.
            emit(node.content, painted,
                 (node.content.height - metrics.height) / 2.0f + metrics.baseline);
        }
    }

    const bool clipped = gbui::clips(style.overflow) && node.firstChild.valid();
    if (clipped) {
        // The padding box, not the frame: CSS clips inside the border, and
        // clipping at the frame let a long run draw over its own border — which
        // is exactly what it looked like.
        const float inset = style.border.visible() ? style.border.width : 0.0f;
        const Rect clipRect{node.frame.x + inset, node.frame.y + inset,
                            std::max(0.0f, node.frame.width - inset * 2.0f),
                            std::max(0.0f, node.frame.height - inset * 2.0f)};
        out.add(PushClip{clipRect, std::max(0.0f, radius - inset)});
    }
    // Children in `zIndex` order, low to high, ties keeping tree order — which
    // is what `stable_sort` gives and what CSS means by "in order of
    // appearance" for equal indices. Sorted only when some child asks, so the
    // ordinary case is still a walk down a linked list.
    bool ordered = true;
    int previous = std::numeric_limits<int>::min();
    for (NodeId child = node.firstChild; child.valid(); child = arena[child].nextSibling) {
        if (arena[child].style.zIndex < previous) ordered = false;
        previous = arena[child].style.zIndex;
    }
    if (ordered) {
        for (NodeId child = node.firstChild; child.valid(); child = arena[child].nextSibling) {
            recordLayer(arena, child, theme, out, measure, layer, effective);
        }
    } else {
        std::vector<NodeId> children;
        for (NodeId child = node.firstChild; child.valid(); child = arena[child].nextSibling) {
            children.push_back(child);
        }
        std::stable_sort(children.begin(), children.end(), [&](NodeId a, NodeId b) {
            return arena[a].style.zIndex < arena[b].style.zIndex;
        });
        for (const NodeId child : children) {
            recordLayer(arena, child, theme, out, measure, layer, effective);
        }
    }
    if (clipped) out.add(PopClip{});
}

}  // namespace

SvgPainter::SvgPainter(float width, float height, Color background)
    : width_(width), height_(height), background_(background) {}

std::string SvgPainter::paintReference(const Paint& paint, const Rect& box) {
    if (!paint.isGradient()) return paint.color.hex();

    // One def per use: gradients are few, and sharing them would mean hashing
    // stops for a saving nobody would notice.
    const int id = ++gradientCounter_;
    const std::string name = "grad" + std::to_string(id);
    if (paint.gradient.kind == GradientKind::Radial) {
        defs_ += "  <radialGradient id=\"" + name + "\">\n";
    } else {
        // CSS angles point up at 0 and turn clockwise; SVG wants the two ends.
        const float radians = (paint.gradient.angle - 90.0f) * 3.14159265f / 180.0f;
        const float dx = std::cos(radians) * 0.5f;
        const float dy = std::sin(radians) * 0.5f;
        defs_ += "  <linearGradient id=\"" + name + "\" x1=\"" + number(0.5f - dx) + "\" y1=\"" +
                 number(0.5f - dy) + "\" x2=\"" + number(0.5f + dx) + "\" y2=\"" +
                 number(0.5f + dy) + "\">\n";
    }
    for (const auto& [position, color] : paint.gradient.stops) {
        defs_ += "    <stop offset=\"" + number(position) + "\" stop-color=\"" + color.hex() +
                 "\" stop-opacity=\"" + number(color.a) + "\"/>\n";
    }
    defs_ += paint.gradient.kind == GradientKind::Radial ? "  </radialGradient>\n"
                                                         : "  </linearGradient>\n";
    (void)box;
    return "url(#" + name + ")";
}

void SvgPainter::fillRect(const FillRect& c) {
    if (c.rect.empty()) return;
    const std::string fill = paintReference(c.paint, c.rect);
    body_ += "  <rect x=\"" + number(c.rect.x) + "\" y=\"" + number(c.rect.y) + "\" width=\"" +
             number(c.rect.width) + "\" height=\"" + number(c.rect.height) + "\"";
    if (c.radius > 0.0f) body_ += " rx=\"" + number(c.radius) + "\"";
    body_ += " fill=\"" + fill + "\"";
    if (!c.paint.isGradient() && c.paint.color.a < 1.0f) {
        body_ += " fill-opacity=\"" + number(c.paint.color.a) + "\"";
    }
    body_ += "/>\n";
}

void SvgPainter::strokeRect(const StrokeRect& c) {
    if (c.rect.empty()) return;
    // Insetting by half the stroke keeps the border inside the frame, the way
    // a CSS border sits inside the box rather than straddling its edge.
    const float inset = c.width / 2.0f;
    body_ += "  <rect x=\"" + number(c.rect.x + inset) + "\" y=\"" + number(c.rect.y + inset) +
             "\" width=\"" + number(std::max(0.0f, c.rect.width - c.width)) + "\" height=\"" +
             number(std::max(0.0f, c.rect.height - c.width)) + "\"";
    if (c.radius > 0.0f) body_ += " rx=\"" + number(std::max(0.0f, c.radius - inset)) + "\"";
    body_ += " fill=\"none\" stroke=\"" + paintReference(c.paint, c.rect) + "\" stroke-width=\"" +
             number(c.width) + "\"";
    if (!c.paint.isGradient() && c.paint.color.a < 1.0f) {
        body_ += " stroke-opacity=\"" + number(c.paint.color.a) + "\"";
    }
    body_ += "/>\n";
}

void SvgPainter::drawText(const DrawText& c) {
    if (c.text.empty()) return;
    float x = c.box.x;
    std::string anchor = "start";
    if (c.align == TextAlign::Center) {
        x = c.box.x + c.box.width / 2.0f;
        anchor = "middle";
    } else if (c.align == TextAlign::End) {
        x = c.box.right();
        anchor = "end";
    }
    body_ +=
        "  <text x=\"" + number(x) + "\" y=\"" + number(c.box.y + c.baseline) +
        "\" font-family=\"" + escapeXml(c.family) + "\" font-size=\"" + number(c.size) +
        "\" font-weight=\"" + std::to_string(static_cast<int>(c.weight)) +
        (c.slant == FontSlant::Italic ? std::string("\" font-style=\"italic") : std::string()) +
        "\" text-anchor=\"" + anchor + "\" fill=\"" + paintReference(c.paint, c.box) + "\"";
    if (!c.paint.isGradient() && c.paint.color.a < 1.0f) {
        body_ += " fill-opacity=\"" + number(c.paint.color.a) + "\"";
    }
    body_ += ">" + escapeXml(c.text) + "</text>\n";
}

void SvgPainter::drawPath(const DrawPath& c) {
    if (c.path.empty()) return;
    std::string data;
    for (const auto& contour : c.path.contours()) {
        if (contour.points.empty()) continue;
        data += "M" + number(contour.points.front().x) + "," + number(contour.points.front().y);
        for (std::size_t i = 1; i < contour.points.size(); ++i) {
            data += "L" + number(contour.points[i].x) + "," + number(contour.points[i].y);
        }
        if (contour.closed) data += "Z";
    }
    body_ += "  <path d=\"" + data + "\" ";
    const std::string reference = paintReference(c.paint, c.path.bounds());
    if (c.strokeWidth > 0.0f) {
        body_ += "fill=\"none\" stroke=\"" + reference + "\" stroke-width=\"" +
                 number(c.strokeWidth) + "\" stroke-linecap=\"round\" stroke-linejoin=\"round\"";
    } else {
        body_ += "fill=\"" + reference + "\"";
    }
    if (!c.paint.isGradient() && c.paint.color.a < 1.0f) {
        body_ += " opacity=\"" + number(c.paint.color.a) + "\"";
    }
    body_ += "/>\n";
}

void SvgPainter::pushClip(const PushClip& c) {
    const int id = ++clipCounter_;
    body_ += "  <clipPath id=\"clip" + std::to_string(id) + "\"><rect x=\"" + number(c.rect.x) +
             "\" y=\"" + number(c.rect.y) + "\" width=\"" + number(c.rect.width) + "\" height=\"" +
             number(c.rect.height) + "\"";
    if (c.radius > 0.0f) body_ += " rx=\"" + number(c.radius) + "\"";
    body_ += "/></clipPath>\n";
    body_ += "  <g clip-path=\"url(#clip" + std::to_string(id) + ")\">\n";
    openClips_.push_back(id);
}

void SvgPainter::popClip() {
    if (openClips_.empty()) return;
    openClips_.pop_back();
    body_ += "  </g>\n";
}

std::string SvgPainter::finish() {
    // An unbalanced push is a bug in the caller, but leaving the document
    // malformed on top of it helps nobody.
    while (!openClips_.empty()) popClip();

    std::string out;
    out += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + number(width_) + "\" height=\"" +
           number(height_) + "\" viewBox=\"0 0 " + number(width_) + " " + number(height_) + "\">\n";
    if (!defs_.empty()) out += "  <defs>\n" + defs_ + "  </defs>\n";
    out += "  <rect width=\"100%\" height=\"100%\" fill=\"" + background_.hex() + "\"/>\n";
    out += body_;
    out += "</svg>\n";
    return out;
}

}  // namespace gbui
