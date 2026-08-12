// The vocabulary the six screens share.
//
// A stat tile, a gauge, a meter, a sparkline, a status pill — the handful of
// things every dashboard in every industry draws, written once here so each
// demo is about *its* domain rather than about laying out a card again.
//
// Everything in this file is built from the public library and nothing else:
// it is the same exercise a reader would do in their own application, and
// docs/guide/writing-a-component.md is the walkthrough. Internal to the demos,
// so it is a header in `src` rather than one anybody includes.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/core/path.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/containers.hpp"
#include "gbui/widgets/controls.hpp"

namespace gbui::demos::kit {

// ---------------------------------------------------------------------------
// Tone
// ---------------------------------------------------------------------------

/**
 * What a reading *means*, before it is a colour.
 *
 * Industrial screens are read at a glance from across a room, and the glance
 * carries one bit: is this fine. Naming the meaning rather than the token is
 * what lets the six screens agree — a tank at 94% and a build that failed are
 * the same red, because they are the same `Alarm`.
 */
enum class Tone { Neutral, Ok, Warn, Alarm, Info };

inline Token toneToken(Tone tone) {
    switch (tone) {
        case Tone::Ok:
            return Token::Added;
        case Tone::Warn:
            return Token::Modified;
        case Tone::Alarm:
            return Token::Removed;
        case Tone::Info:
            return Token::Accent;
        case Tone::Neutral:
            break;
    }
    return Token::TextMuted;
}

/** The tone a value falls into, given the two thresholds it crosses. Written
 *  this way round — warn below alarm — because that is how a setpoint is
 *  configured on every plant floor. */
inline Tone toneFor(double value, double warn, double alarm) {
    if (value >= alarm) return Tone::Alarm;
    if (value >= warn) return Tone::Warn;
    return Tone::Ok;
}

/** The same, for a reading that is bad when it falls rather than when it
 *  rises — a tank running dry, a battery, a service level. */
inline Tone toneBelow(double value, double warn, double alarm) {
    if (value <= alarm) return Tone::Alarm;
    if (value <= warn) return Tone::Warn;
    return Tone::Ok;
}

// ---------------------------------------------------------------------------
// Numbers
// ---------------------------------------------------------------------------

/** `printf` into a `std::string`, which is what every readout on these screens
 *  needs and what the standard library still will not do in one call. */
inline std::string format(const char* pattern, double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), pattern, value);
    return std::string(buffer);
}

/** 12 400 as "12.4k", 3 100 000 as "3.1M" — a KPI tile has room for four
 *  characters and a reader has patience for four. */
inline std::string compact(double value) {
    const double magnitude = std::fabs(value);
    if (magnitude >= 1'000'000.0) return format("%.1fM", value / 1'000'000.0);
    if (magnitude >= 1'000.0) return format("%.1fk", value / 1'000.0);
    return format("%.0f", value);
}

/** A signed delta, always with its sign: "+4.2%", "-1.8%". The sign is the
 *  information; a bare "4.2%" beside a green arrow makes the reader check
 *  twice. */
inline std::string signedPercent(double value) {
    return (value >= 0.0 ? "+" : "") + format("%.1f%%", value);
}

/** Minutes as "4h 05m", which is how a shift, an ETA and a downtime are all
 *  actually read. */
inline std::string duration(double minutes) {
    const int total = static_cast<int>(std::lround(std::fabs(minutes)));
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%dh %02dm", total / 60, total % 60);
    return std::string(buffer);
}

/**
 * Deterministic value noise: smooth, seeded, and the same everywhere.
 *
 * The telemetry on these screens has to *move*, or a dashboard demo is a
 * screenshot. It also has to be reproducible, or no two screenshots of the
 * same frame match and the docs images churn on every build. Summed sines
 * satisfy both — no state, no clock, no random device, and `noise(t, seed)` is
 * the same number on a laptop and in CI.
 */
inline float noise(float t, float seed) {
    return 0.55f * std::sin(t * 0.90f + seed * 12.9898f) +
           0.30f * std::sin(t * 2.30f + seed * 78.233f) +
           0.15f * std::sin(t * 5.70f + seed * 43.758f);
}

/** The same wave mapped onto a range, which is what every reading here wants. */
inline double wave(float t, float seed, double low, double high) {
    const double unit = (noise(t, seed) + 1.0) * 0.5;
    return low + (high - low) * std::clamp(unit, 0.0, 1.0);
}

/**
 * A rolling history that only advances on the clock's own tick.
 *
 * Charts here are fed by time rather than by frames on purpose: a 144 Hz
 * display and a 60 Hz one would otherwise scroll the same chart at different
 * speeds, and a screenshot would depend on how fast the machine was.
 */
class Rolling {
public:
    Rolling(std::size_t count, double low, double high, float seed, float period = 0.6f)
        : low_(low), high_(high), seed_(seed), period_(period) {
        values_.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            values_[i] = wave(static_cast<float>(i) * period_, seed_, low_, high_);
        }
    }

    /** Appends whatever samples the elapsed time has earned since the last
     *  call, dropping the oldest. */
    void advance(float time) {
        const long ticks = static_cast<long>(time / period_);
        // A demo left open for an hour must not spend that hour catching up:
        // more than a bufferful of missed ticks is a reset, not a replay.
        const long count = static_cast<long>(values_.size());
        const long first = std::max(lastTick_ + 1, ticks - count + 1);
        for (long i = first; i <= ticks; ++i) {
            values_.erase(values_.begin());
            values_.push_back(
                wave(static_cast<float>(i + count - 1) * period_, seed_, low_, high_));
        }
        lastTick_ = ticks;
    }

    const std::vector<double>& values() const { return values_; }
    double latest() const { return values_.empty() ? 0.0 : values_.back(); }
    /** The change from the sample before the latest, as a percentage. */
    double trend() const {
        if (values_.size() < 2) return 0.0;
        const double previous = values_[values_.size() - 2];
        if (std::fabs(previous) < 1e-9) return 0.0;
        return (values_.back() - previous) / previous * 100.0;
    }
    double mean() const {
        if (values_.empty()) return 0.0;
        double total = 0.0;
        for (double v : values_) total += v;
        return total / static_cast<double>(values_.size());
    }
    double peak() const {
        return values_.empty() ? 0.0 : *std::max_element(values_.begin(), values_.end());
    }

private:
    std::vector<double> values_;
    double low_;
    double high_;
    float seed_;
    float period_;
    long lastTick_ = 0;
};

// ---------------------------------------------------------------------------
// Vector helpers
// ---------------------------------------------------------------------------

/** Appends an arc as a polyline. The path layer flattens curves anyway, so a
 *  segment every few degrees is the same geometry a cubic would become — and
 *  the maths fits on one screen. */
inline void arc(Path& path, Vec2 centre, float radius, float fromDegrees, float toDegrees,
                bool startNew = true) {
    constexpr float kPi = 3.14159265358979323846f;
    const float sweep = std::fabs(toDegrees - fromDegrees);
    const int steps = std::max(6, static_cast<int>(sweep / 3.0f));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float angle = (fromDegrees + (toDegrees - fromDegrees) * t) * kPi / 180.0f;
        const Vec2 point{centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)};
        if (i == 0 && startNew) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
}

// ---------------------------------------------------------------------------
// Components
// ---------------------------------------------------------------------------

struct CardOptions {
    std::string_view title{};
    /** Drawn small and muted to the right of the title. */
    std::string_view note{};
    Direction direction = Direction::Column;
    float gap = 10.0f;
    Edges padding = Edges::all(14.0f);
    float grow = 0.0f;
    float width = kAuto;
    float height = kAuto;
    float minWidth = kAuto;
    Token background = Token::BgElevated;
    bool border = true;
};

/** A titled surface. Everything on these screens that is not a bar lives in
 *  one, which is what makes six unrelated industries look like one product. */
inline Ui::Scope beginCard(Ui& ui, const CardOptions& options = {}) {
    Style outer;
    outer.direction = Direction::Column;
    outer.gap = options.title.empty() ? 0.0f : 10.0f;
    outer.padding = options.padding;
    outer.background = Fill{options.background};
    if (options.border) outer.border = Border{1.0f, Fill{Token::Border}};
    outer.grow = options.grow;
    outer.shrink = 1.0f;
    outer.width = options.width;
    outer.height = options.height;
    outer.minWidth = options.minWidth;
    outer.basis = options.grow > 0.0f ? Length{0.0f} : Length{};
    outer.overflow = Overflow::Hidden;

    auto scope = ui.begin(outer);
    if (!options.title.empty()) {
        Style header;
        header.direction = Direction::Row;
        header.align = Align::Center;
        header.gap = 8.0f;
        header.shrink = 0.0f;
        auto headerScope = ui.begin(header);
        sectionHeading(ui, options.title);
        if (!options.note.empty()) {
            spacer(ui);
            text(ui, options.note, {.color = Token::TextMuted, .size = 11.0f});
        }
    }

    // The body, so the caller's children get their own direction and gap
    // without fighting the header for the column.
    //
    // It grows on an auto basis, not from zero: a basis of zero would make the
    // body ignore its own content, so a card with no height of its own — an
    // alarm list, a schedule — would collapse to its title and draw nothing.
    Style body;
    body.direction = options.direction;
    body.gap = options.gap;
    body.grow = 1.0f;
    body.minWidth = 0.0f;
    body.minHeight = 0.0f;
    auto bodyScope = ui.begin(body);

    // The inner scope closes both: see the note on `adopt` in ui.hpp.
    bodyScope.adopt();
    scope.disown();
    return bodyScope;
}

struct PillOptions {
    Tone tone = Tone::Neutral;
    /** A filled pill rather than a washed one, for the single most important
     *  state on the screen. */
    bool solid = false;
    float size = 10.0f;
};

/** A status chip: "RUNNING", "DEGRADED", "3 ALARMS". */
inline NodeId pill(Ui& ui, std::string_view value, const PillOptions& options = {}) {
    const Token token = toneToken(options.tone);
    Style style;
    style.direction = Direction::Row;
    style.align = Align::Center;
    style.justify = Justify::Center;
    style.height = options.size + 10.0f;
    style.padding = Edges::symmetric(0.0f, 8.0f);
    style.radius = (options.size + 10.0f) / 2.0f;
    style.shrink = 0.0f;
    style.background = options.solid ? Fill{token} : Fill{token, 0.18f};

    auto scope = ui.begin(style);
    text(ui, value,
         {.color = options.solid ? Token::AccentFg : token,
          .weight = FontWeight::SemiBold,
          .size = options.size});
    return scope.id();
}

/** A fixed gap. A row's `gap` handles the usual case; this is for the places
 *  that have no row of their own — a table cell, which is built by a callback
 *  into a box the widget owns. */
inline NodeId hspace(Ui& ui, float width) {
    Style style;
    style.width = width;
    style.shrink = 0.0f;
    return ui.add(style);
}

/** The dot beside a name in a list of machines, feeds or services. */
inline NodeId statusDot(Ui& ui, Tone tone, float size = 8.0f) {
    Style style;
    style.width = size;
    style.height = size;
    style.radius = size / 2.0f;
    style.shrink = 0.0f;
    style.alignSelf = Align::Center;
    style.background = Fill{toneToken(tone)};
    return ui.add(style);
}

struct SparklineOptions {
    float width = 96.0f;
    float height = 30.0f;
    Tone tone = Tone::Info;
    /** Shades the area under the line. */
    bool area = true;
    float thickness = 1.6f;
};

/**
 * A line with no axes, no grid and no readout.
 *
 * Not `lineChart` with everything turned off: a sparkline is a *glyph*, sized
 * to sit inside a tile beside its number, and the chart module's smallest
 * useful form is still a chart. Twenty lines here against a hundred there.
 */
inline NodeId sparkline(Ui& ui, const std::vector<double>& values,
                        const SparklineOptions& options = {}) {
    Style style;
    style.width = options.width;
    style.height = options.height;
    style.alignSelf = Align::Center;
    // The glyph gives way before the number does. Its geometry is built for
    // `options.width` and does not rebuild when the node is squeezed, so the
    // node clips instead — losing the left of a sparkline is a decoration
    // getting smaller, where losing the right of "1 013" is a lie.
    style.shrink = 1.0f;
    style.minWidth = 0.0f;
    style.overflow = Overflow::Hidden;

    if (values.size() < 2) return ui.add(style);

    const auto [low, high] = std::minmax_element(values.begin(), values.end());
    const double span = *high - *low;
    // A flat series would otherwise divide by zero and draw at the top; the
    // honest picture of "nothing changed" is a line through the middle.
    const auto at = [&](std::size_t i) {
        const float x =
            options.width * static_cast<float>(i) / static_cast<float>(values.size() - 1);
        const double unit = span > 1e-9 ? (values[i] - *low) / span : 0.5;
        const float y = options.height - static_cast<float>(unit) * (options.height - 2.0f) - 1.0f;
        return Vec2{x, y};
    };

    std::vector<Shape> shapes;
    const Token token = toneToken(options.tone);

    if (options.area) {
        Path fill;
        fill.moveTo({0.0f, options.height});
        for (std::size_t i = 0; i < values.size(); ++i) fill.lineTo(at(i));
        fill.lineTo({options.width, options.height});
        fill.close();
        shapes.push_back(Shape{fill, Fill{token, 0.16f}, 0.0f});
    }

    Path line;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i == 0) {
            line.moveTo(at(i));
        } else {
            line.lineTo(at(i));
        }
    }
    shapes.push_back(Shape{line, Fill{token}, options.thickness});

    return ui.draw(style, std::move(shapes));
}

struct StatOptions {
    std::string_view label;
    std::string value;
    /** Drawn small beside the value: "ms", "kW", "%". */
    std::string_view unit{};
    /** Empty draws no trend line. */
    std::string trend{};
    Tone trendTone = Tone::Neutral;
    Tone tone = Tone::Neutral;
    /** Empty draws no sparkline. */
    const std::vector<double>* history = nullptr;
    float grow = 1.0f;
    float valueSize = 26.0f;
};

/**
 * The KPI tile: a label, one big number, and how it is moving.
 *
 * The number is the largest thing in it and the label is the smallest, which
 * is the whole trick — a reader scanning eight tiles reads eight numbers and
 * only goes back for the label of the one that surprised them.
 */
inline NodeId statTile(Ui& ui, const StatOptions& options) {
    Style tile;
    tile.direction = Direction::Column;
    tile.gap = 6.0f;
    tile.padding = Edges::all(12.0f);
    tile.background = Fill{Token::BgElevated};
    tile.border = Border{1.0f, Fill{Token::Border}};
    tile.grow = options.grow;
    tile.basis = 0.0f;
    tile.minWidth = 0.0f;
    tile.overflow = Overflow::Hidden;
    auto scope = ui.begin(tile);

    {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 6.0f;
        auto rowScope = ui.begin(row);
        if (options.tone != Tone::Neutral) statusDot(ui, options.tone, 7.0f);
        text(
            ui, options.label,
            {.color = Token::TextMuted, .weight = FontWeight::Medium, .size = 10.5f, .grow = 1.0f});
    }
    {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 8.0f;
        auto rowScope = ui.begin(row);
        {
            // Value and unit share a baseline-ish row of their own, so the
            // sparkline on the far side does not stretch the number's line box.
            Style pair;
            pair.direction = Direction::Row;
            pair.align = Align::Center;
            pair.gap = 3.0f;
            pair.shrink = 0.0f;
            auto pairScope = ui.begin(pair);
            text(ui, options.value,
                 {.color = Token::TextStrong,
                  .weight = FontWeight::SemiBold,
                  .size = options.valueSize});
            if (!options.unit.empty()) {
                text(ui, options.unit, {.color = Token::TextMuted, .size = 11.0f});
            }
        }
        spacer(ui);
        if (options.history && options.history->size() > 1) {
            sparkline(ui, *options.history,
                      {.width = 76.0f,
                       .height = 30.0f,
                       .tone = options.tone == Tone::Neutral ? Tone::Info : options.tone});
        }
    }
    if (!options.trend.empty()) {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 4.0f;
        auto rowScope = ui.begin(row);
        icon(ui, options.trendTone == Tone::Alarm ? Icon::ChevronDown : Icon::ChevronUp,
             {.color = toneToken(options.trendTone), .size = 13.0f});
        text(ui, options.trend, {.color = toneToken(options.trendTone), .size = 11.0f});
    }
    return scope.id();
}

struct MeterOptions {
    std::string_view label;
    double value = 0.0;
    double minimum = 0.0;
    double maximum = 100.0;
    std::string_view unit{};
    Tone tone = Tone::Info;
    /** Draws the reading on the right of the label row. */
    bool showValue = true;
    float height = 8.0f;
    /**
     * Take the free space on the parent's main axis.
     *
     * Needed inside a table cell, and only there. A bar has no intrinsic width
     * — it is however wide it is given — so a meter with neither a label nor a
     * reading to size it collapses to nothing in a row that sizes to content.
     */
    float grow = 0.0f;
    const char* valueFormat = "%.1f";
};

/**
 * A labelled bar: a tank level, a queue depth, a utilisation.
 *
 * `progressBar` is the same rectangle, and this is deliberately not it — a
 * meter is *a reading against a range*, so it carries its own label, its unit
 * and the tone the reading falls into. A dashboard draws forty of these and a
 * loading bar exactly none.
 */
inline NodeId meter(Ui& ui, const MeterOptions& options) {
    Style column;
    column.direction = Direction::Column;
    column.gap = 5.0f;
    column.justify = Justify::Center;
    column.grow = options.grow;
    column.minWidth = 0.0f;
    auto scope = ui.begin(column);

    // With neither a label nor a reading there is nothing to put above the
    // bar, and an empty row would still take a line — which is exactly the
    // case a meter drawn inside a table cell wants.
    if (!options.label.empty() || options.showValue) {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 8.0f;
        auto rowScope = ui.begin(row);
        text(ui, options.label, {.color = Token::Text, .size = 11.5f, .grow = 1.0f});
        if (options.showValue) {
            text(ui, format(options.valueFormat, options.value),
                 {.color = Token::TextStrong,
                  .weight = FontWeight::Medium,
                  .role = FontRole::Mono,
                  .size = 11.5f});
            if (!options.unit.empty()) {
                text(ui, options.unit, {.color = Token::TextMuted, .size = 10.5f});
            }
        }
    }

    const double span = options.maximum - options.minimum;
    const float fraction =
        span > 1e-9
            ? static_cast<float>(std::clamp((options.value - options.minimum) / span, 0.0, 1.0))
            : 0.0f;
    {
        Style track;
        track.height = options.height;
        track.radius = options.height / 2.0f;
        track.background = Fill{Token::BgOverlay};
        track.overflow = Overflow::Hidden;
        auto trackScope = ui.begin(track);

        Style fill;
        fill.width = Length::percent(fraction * 100.0f);
        fill.radius = options.height / 2.0f;
        fill.background = Fill{toneToken(options.tone)};
        ui.add(fill);
    }
    return scope.id();
}

struct GaugeOptions {
    double value = 0.0;
    double minimum = 0.0;
    double maximum = 100.0;
    std::string_view label{};
    std::string_view unit{};
    Tone tone = Tone::Info;
    float size = 132.0f;
    float thickness = 10.0f;
    /** Where the arc starts and ends, in degrees clockwise from three o'clock.
     *  The default is the open-bottom dial every instrument panel uses. */
    float from = 135.0f;
    float to = 405.0f;
    const char* valueFormat = "%.0f";
};

/**
 * The dial.
 *
 * Every process screen has one, and it is worth knowing that nothing in the
 * toolkit knows what a gauge is: this is two arcs and a dot handed to
 * `Ui::draw`, which is the same vector node an icon and a chart ride on. The
 * text in the middle is an ordinary column positioned over it.
 */
inline NodeId gauge(Ui& ui, const GaugeOptions& options) {
    const double span = options.maximum - options.minimum;
    const float fraction =
        span > 1e-9
            ? static_cast<float>(std::clamp((options.value - options.minimum) / span, 0.0, 1.0))
            : 0.0f;

    const float radius = (options.size - options.thickness) / 2.0f;
    const Vec2 centre{options.size / 2.0f, options.size / 2.0f};
    const float end = options.from + (options.to - options.from) * fraction;

    std::vector<Shape> shapes;
    Path track;
    arc(track, centre, radius, options.from, options.to);
    shapes.push_back(Shape{track, Fill{Token::BgOverlay}, options.thickness});

    if (fraction > 0.001f) {
        Path value;
        arc(value, centre, radius, options.from, end);
        shapes.push_back(Shape{value, Fill{toneToken(options.tone)}, options.thickness});
    }

    Style stack;
    stack.width = options.size;
    stack.height = options.size;
    stack.shrink = 0.0f;
    stack.justify = Justify::Center;
    stack.align = Align::Center;
    auto scope = ui.begin(stack);
    {
        // The arcs, filling the stack, with the readout absolutely positioned
        // over them — the dial is art and the numbers are text, and keeping
        // them apart is what lets the theme restyle each on its own.
        Style art;
        art.position = Position::Absolute;
        art.left = 0.0f;
        art.top = 0.0f;
        art.width = options.size;
        art.height = options.size;
        ui.draw(art, std::move(shapes));
    }
    {
        Style readout;
        readout.direction = Direction::Column;
        readout.align = Align::Center;
        readout.justify = Justify::Center;
        readout.gap = 1.0f;
        auto readoutScope = ui.begin(readout);
        {
            Style row;
            row.direction = Direction::Row;
            row.align = Align::Center;
            row.gap = 2.0f;
            auto rowScope = ui.begin(row);
            text(ui, format(options.valueFormat, options.value),
                 {.color = Token::TextStrong,
                  .weight = FontWeight::SemiBold,
                  .size = options.size * 0.20f});
            if (!options.unit.empty()) {
                text(ui, options.unit, {.color = Token::TextMuted, .size = options.size * 0.09f});
            }
        }
        if (!options.label.empty()) {
            text(ui, options.label,
                 {.color = Token::TextMuted,
                  .size = options.size * 0.082f,
                  .align = TextAlign::Center});
        }
    }
    return scope.id();
}

/** A label above a value, which is how every specification, setpoint and
 *  nameplate on these screens is written. */
inline NodeId field(Ui& ui, std::string_view label, std::string_view value,
                    Token color = Token::TextStrong, FontRole role = FontRole::Ui) {
    Style column;
    column.direction = Direction::Column;
    column.gap = 2.0f;
    column.shrink = 1.0f;
    column.minWidth = 0.0f;
    auto scope = ui.begin(column);
    text(ui, label, {.color = Token::TextMuted, .size = 10.0f});
    text(ui, value, {.color = color, .weight = FontWeight::Medium, .role = role, .size = 12.5f});
    return scope.id();
}

/** The bar across the top of every screen here: an icon, a title, whatever the
 *  screen wants on the right. Returns the scope, so the right-hand side is
 *  written by the caller after a `spacer`. */
inline Ui::Scope beginHeader(Ui& ui, Icon glyph, std::string_view title,
                             std::string_view subtitle = {}) {
    Style bar;
    bar.direction = Direction::Row;
    bar.align = Align::Center;
    bar.gap = 12.0f;
    bar.height = 54.0f;
    bar.shrink = 0.0f;
    bar.padding = Edges::symmetric(0.0f, 16.0f);
    bar.background = Fill{Token::BgElevated};
    bar.radius = 0.0f;
    auto scope = ui.begin(bar);

    {
        Style mark;
        mark.width = 30.0f;
        mark.height = 30.0f;
        mark.radius = 8.0f;
        mark.shrink = 0.0f;
        mark.align = Align::Center;
        mark.justify = Justify::Center;
        mark.background = Fill{Token::Accent, 0.18f};
        auto markScope = ui.begin(mark);
        icon(ui, glyph, {.color = Token::Accent, .size = 17.0f});
    }
    {
        Style titles;
        titles.direction = Direction::Column;
        titles.gap = 1.0f;
        titles.shrink = 1.0f;
        titles.minWidth = 0.0f;
        auto titleScope = ui.begin(titles);
        text(ui, title,
             {.color = Token::TextStrong, .weight = FontWeight::SemiBold, .size = 14.0f});
        if (!subtitle.empty()) {
            text(ui, subtitle, {.color = Token::TextMuted, .size = 11.0f});
        }
    }
    return scope;
}

/** The rule under a bar, and the one between two panes. */
inline void rule(Ui& ui, Direction containerDirection) { divider(ui, containerDirection); }

/** The bar along the bottom: small, muted, and full of the things nobody looks
 *  at until something is wrong. */
inline Ui::Scope beginStatusBar(Ui& ui) {
    Style bar;
    bar.direction = Direction::Row;
    bar.align = Align::Center;
    bar.gap = 16.0f;
    bar.height = 26.0f;
    bar.shrink = 0.0f;
    bar.padding = Edges::symmetric(0.0f, 14.0f);
    bar.background = Fill{Token::BgElevated};
    bar.radius = 0.0f;
    return ui.begin(bar);
}

inline NodeId statusItem(Ui& ui, Icon glyph, std::string_view value, Tone tone = Tone::Neutral) {
    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 5.0f;
    row.shrink = 0.0f;
    auto scope = ui.begin(row);
    icon(ui, glyph, {.color = toneToken(tone), .size = 12.0f});
    text(ui, value, {.color = Token::TextMuted, .size = 10.5f});
    return scope.id();
}

/** The row a list of machines, feeds, alarms or services is made of. */
struct EntryOptions {
    std::string_view id{};
    bool selected = false;
    bool hovered = false;
    float height = 34.0f;
    float gap = 10.0f;
};

inline Ui::Scope beginEntry(Ui& ui, const EntryOptions& options) {
    return beginListRow(ui, {.selected = options.selected,
                             .hovered = options.hovered,
                             .height = options.height,
                             .padding = Edges::symmetric(0.0f, 10.0f),
                             .gap = options.gap,
                             .id = options.id});
}

}  // namespace gbui::demos::kit
