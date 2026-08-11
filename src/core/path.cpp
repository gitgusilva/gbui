#include "gbui/core/path.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>

namespace gbui {
namespace {

/** How many line segments a cubic needs to stay within `tolerance`.
 *  Derived from the control polygon's deviation, which overestimates slightly
 *  and is exactly the trade a 16-pixel icon wants. */
int segmentsFor(Vec2 a, Vec2 b, Vec2 c, Vec2 d, float tolerance) {
    const float deviation =
        std::max(std::hypot(a.x - 2 * b.x + c.x, a.y - 2 * b.y + c.y),
                 std::hypot(b.x - 2 * c.x + d.x, b.y - 2 * c.y + d.y));
    if (deviation <= 0.0f) return 1;
    const float count = std::ceil(std::sqrt(deviation * 3.0f / (4.0f * tolerance)));
    return std::clamp(static_cast<int>(count), 1, 64);
}

/**
 * SVG's endpoint arc, as cubics.
 *
 * The spec parameterises an arc by where it ends, two radii, a rotation and two
 * flags; drawing it needs the centre and the angles it sweeps, which is what
 * this recovers (SVG 1.1, appendix F.6). Lucide leans on arcs for every rounded
 * corner it does not spell out as a curve, so a parser without this draws its
 * icons in pieces.
 */
void arcToCubics(Path& path, Vec2 from, float rx, float ry, float rotationDegrees,
                 bool largeArc, bool sweep, Vec2 to, float tolerance) {
    if (rx == 0.0f || ry == 0.0f) {
        path.lineTo(to);
        return;
    }
    rx = std::fabs(rx);
    ry = std::fabs(ry);

    const float phi = rotationDegrees * 3.14159265358979323846f / 180.0f;
    const float cosPhi = std::cos(phi);
    const float sinPhi = std::sin(phi);

    const float dx2 = (from.x - to.x) / 2.0f;
    const float dy2 = (from.y - to.y) / 2.0f;
    const float x1p = cosPhi * dx2 + sinPhi * dy2;
    const float y1p = -sinPhi * dx2 + cosPhi * dy2;

    // Radii too small to reach the endpoint are scaled up, as the spec requires.
    const float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1.0f) {
        const float scale = std::sqrt(lambda);
        rx *= scale;
        ry *= scale;
    }

    const float numerator = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    const float denominator = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    float factor = denominator > 0.0f ? std::sqrt(std::max(0.0f, numerator / denominator)) : 0.0f;
    if (largeArc == sweep) factor = -factor;

    const float cxp = factor * rx * y1p / ry;
    const float cyp = -factor * ry * x1p / rx;
    const Vec2 centre{cosPhi * cxp - sinPhi * cyp + (from.x + to.x) / 2.0f,
                      sinPhi * cxp + cosPhi * cyp + (from.y + to.y) / 2.0f};

    const auto angle = [](float ux, float uy, float vx, float vy) {
        const float dot = ux * vx + uy * vy;
        const float length = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        float result = std::acos(std::clamp(length > 0.0f ? dot / length : 0.0f, -1.0f, 1.0f));
        if (ux * vy - uy * vx < 0.0f) result = -result;
        return result;
    };

    const float startAngle = angle(1.0f, 0.0f, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float sweepAngle = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx,
                             (-y1p - cyp) / ry);
    constexpr float kTwoPi = 6.28318530717958647692f;
    if (!sweep && sweepAngle > 0.0f) sweepAngle -= kTwoPi;
    if (sweep && sweepAngle < 0.0f) sweepAngle += kTwoPi;

    // A cubic approximates at most a quarter turn well; split accordingly.
    const int segments =
        std::max(1, static_cast<int>(std::ceil(std::fabs(sweepAngle) / (kTwoPi / 4.0f))));
    const float delta = sweepAngle / static_cast<float>(segments);
    const float alpha = 4.0f / 3.0f * std::tan(delta / 4.0f);

    float theta = startAngle;
    for (int i = 0; i < segments; ++i) {
        const float cosTheta = std::cos(theta);
        const float sinTheta = std::sin(theta);
        const float thetaEnd = theta + delta;
        const float cosEnd = std::cos(thetaEnd);
        const float sinEnd = std::sin(thetaEnd);

        const auto pointAt = [&](float c, float s) {
            return Vec2{centre.x + rx * cosPhi * c - ry * sinPhi * s,
                        centre.y + rx * sinPhi * c + ry * cosPhi * s};
        };
        const auto tangentAt = [&](float c, float s) {
            return Vec2{-rx * cosPhi * s - ry * sinPhi * c, -rx * sinPhi * s + ry * cosPhi * c};
        };

        const Vec2 p0 = pointAt(cosTheta, sinTheta);
        const Vec2 p3 = pointAt(cosEnd, sinEnd);
        const Vec2 t0 = tangentAt(cosTheta, sinTheta);
        const Vec2 t3 = tangentAt(cosEnd, sinEnd);
        path.cubicTo({p0.x + alpha * t0.x, p0.y + alpha * t0.y},
                     {p3.x - alpha * t3.x, p3.y - alpha * t3.y}, p3, tolerance);
        theta = thetaEnd;
    }
}

class Reader {
public:
    explicit Reader(std::string_view text) : text_(text) {}

    void skipSeparators() {
        while (cursor_ < text_.size() &&
               (std::isspace(static_cast<unsigned char>(text_[cursor_])) || text_[cursor_] == ',')) {
            ++cursor_;
        }
    }

    bool done() {
        skipSeparators();
        return cursor_ >= text_.size();
    }

    bool peekCommand() {
        skipSeparators();
        return cursor_ < text_.size() && std::isalpha(static_cast<unsigned char>(text_[cursor_]));
    }

    char command() { return text_[cursor_++]; }

    /** Reads one number. Returns false at anything that is not one, which is
     *  how a command's argument list ends. */
    bool number(float& out) {
        skipSeparators();
        const std::size_t start = cursor_;
        if (cursor_ < text_.size() && (text_[cursor_] == '-' || text_[cursor_] == '+')) ++cursor_;
        bool digits = false;
        while (cursor_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[cursor_]))) {
            ++cursor_;
            digits = true;
        }
        if (cursor_ < text_.size() && text_[cursor_] == '.') {
            ++cursor_;
            while (cursor_ < text_.size() &&
                   std::isdigit(static_cast<unsigned char>(text_[cursor_]))) {
                ++cursor_;
                digits = true;
            }
        }
        if (!digits) {
            cursor_ = start;
            return false;
        }
        if (cursor_ < text_.size() && (text_[cursor_] == 'e' || text_[cursor_] == 'E')) {
            const std::size_t save = cursor_++;
            if (cursor_ < text_.size() && (text_[cursor_] == '-' || text_[cursor_] == '+')) ++cursor_;
            if (cursor_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[cursor_]))) {
                while (cursor_ < text_.size() &&
                       std::isdigit(static_cast<unsigned char>(text_[cursor_]))) {
                    ++cursor_;
                }
            } else {
                cursor_ = save;
            }
        }
        const auto result =
            std::from_chars(text_.data() + start, text_.data() + cursor_, out);
        return result.ec == std::errc{};
    }

private:
    std::string_view text_;
    std::size_t cursor_ = 0;
};

}  // namespace

void Path::moveTo(Vec2 point) {
    contours_.push_back(Contour{{point}, false});
    current_ = point;
}

void Path::lineTo(Vec2 point) {
    if (contours_.empty()) moveTo(point);
    else contours_.back().points.push_back(point);
    current_ = point;
}

void Path::cubicTo(Vec2 c1, Vec2 c2, Vec2 end, float tolerance) {
    if (contours_.empty()) moveTo(current_);
    const Vec2 start = current_;
    const int steps = segmentsFor(start, c1, c2, end, tolerance);
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float u = 1.0f - t;
        const float w0 = u * u * u;
        const float w1 = 3.0f * u * u * t;
        const float w2 = 3.0f * u * t * t;
        const float w3 = t * t * t;
        contours_.back().points.push_back(
            {start.x * w0 + c1.x * w1 + c2.x * w2 + end.x * w3,
             start.y * w0 + c1.y * w1 + c2.y * w2 + end.y * w3});
    }
    current_ = end;
}

void Path::close() {
    if (contours_.empty()) return;
    contours_.back().closed = true;
    if (!contours_.back().points.empty()) current_ = contours_.back().points.front();
}

Rect Path::bounds() const {
    float minX = std::numeric_limits<float>::infinity();
    float minY = minX;
    float maxX = -minX;
    float maxY = -minX;
    for (const Contour& contour : contours_) {
        for (const Vec2& point : contour.points) {
            minX = std::min(minX, point.x);
            minY = std::min(minY, point.y);
            maxX = std::max(maxX, point.x);
            maxY = std::max(maxY, point.y);
        }
    }
    if (minX > maxX) return {};
    return {minX, minY, maxX - minX, maxY - minY};
}

Path Path::transformed(float scale, Vec2 offset) const {
    Path out;
    out.contours_ = contours_;
    for (Contour& contour : out.contours_) {
        for (Vec2& point : contour.points) {
            point.x = point.x * scale + offset.x;
            point.y = point.y * scale + offset.y;
        }
    }
    return out;
}

Path parseSvgPath(std::string_view d, float tolerance) {
    Path path;
    Reader reader(d);
    char command = 0;
    Vec2 cursor{};
    Vec2 subpathStart{};
    Vec2 lastControl{};
    bool lastWasCubic = false;

    while (!reader.done()) {
        if (reader.peekCommand()) command = reader.command();

        const bool relative = std::islower(static_cast<unsigned char>(command)) != 0;
        const char op = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
        const Vec2 origin = relative ? cursor : Vec2{0, 0};

        auto pair = [&](Vec2& out) {
            float x = 0;
            float y = 0;
            if (!reader.number(x) || !reader.number(y)) return false;
            out = {origin.x + x, origin.y + y};
            return true;
        };

        switch (op) {
            case 'M': {
                Vec2 point;
                if (!pair(point)) return path;
                path.moveTo(point);
                cursor = subpathStart = point;
                // Extra pairs after a moveto are implicit linetos, per SVG.
                command = relative ? 'l' : 'L';
                lastWasCubic = false;
                break;
            }
            case 'L': {
                Vec2 point;
                if (!pair(point)) return path;
                path.lineTo(point);
                cursor = point;
                lastWasCubic = false;
                break;
            }
            case 'H': {
                float x = 0;
                if (!reader.number(x)) return path;
                cursor = {origin.x + x, cursor.y};
                path.lineTo(cursor);
                lastWasCubic = false;
                break;
            }
            case 'V': {
                float y = 0;
                if (!reader.number(y)) return path;
                cursor = {cursor.x, origin.y + y};
                path.lineTo(cursor);
                lastWasCubic = false;
                break;
            }
            case 'C': {
                Vec2 c1;
                Vec2 c2;
                Vec2 end;
                if (!pair(c1) || !pair(c2) || !pair(end)) return path;
                path.cubicTo(c1, c2, end, tolerance);
                cursor = end;
                lastControl = c2;
                lastWasCubic = true;
                break;
            }
            case 'S': {
                Vec2 c2;
                Vec2 end;
                if (!pair(c2) || !pair(end)) return path;
                // The first control is the reflection of the previous one.
                const Vec2 c1 = lastWasCubic
                                    ? Vec2{2 * cursor.x - lastControl.x, 2 * cursor.y - lastControl.y}
                                    : cursor;
                path.cubicTo(c1, c2, end, tolerance);
                cursor = end;
                lastControl = c2;
                lastWasCubic = true;
                break;
            }
            case 'Q':
            case 'T': {
                Vec2 control;
                Vec2 end;
                if (op == 'Q') {
                    if (!pair(control) || !pair(end)) return path;
                } else {
                    control = lastWasCubic
                                  ? Vec2{2 * cursor.x - lastControl.x, 2 * cursor.y - lastControl.y}
                                  : cursor;
                    if (!pair(end)) return path;
                }
                // A quadratic is a cubic whose controls sit two thirds along.
                const Vec2 c1{cursor.x + 2.0f / 3.0f * (control.x - cursor.x),
                              cursor.y + 2.0f / 3.0f * (control.y - cursor.y)};
                const Vec2 c2{end.x + 2.0f / 3.0f * (control.x - end.x),
                              end.y + 2.0f / 3.0f * (control.y - end.y)};
                path.cubicTo(c1, c2, end, tolerance);
                cursor = end;
                lastControl = control;
                lastWasCubic = true;
                break;
            }
            case 'A': {
                float rx = 0;
                float ry = 0;
                float rotation = 0;
                float largeArc = 0;
                float sweep = 0;
                Vec2 end;
                if (!reader.number(rx) || !reader.number(ry) || !reader.number(rotation) ||
                    !reader.number(largeArc) || !reader.number(sweep) || !pair(end)) {
                    return path;
                }
                arcToCubics(path, cursor, rx, ry, rotation, largeArc != 0.0f, sweep != 0.0f, end,
                            tolerance);
                cursor = end;
                lastWasCubic = false;
                break;
            }
            case 'Z':
                path.close();
                cursor = subpathStart;
                lastWasCubic = false;
                break;
            default:
                // Anything else — an arc that escaped the generator, a typo —
                // ends the parse with whatever was read.
                return path;
        }
    }
    return path;
}

}  // namespace gbui
