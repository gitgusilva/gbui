#include "gbui/widgets/chart.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "detail.hpp"
#include "gbui/widgets/button.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

// What this shares with its siblings, rather than a copy in each.
using namespace detail;

namespace {

/** The nearest 1, 2, 5 or 10 times a power of ten. Those four are what a reader
 *  counts in; anything else makes them do arithmetic to read a chart. */
double niceStep(double rough) {
    if (rough <= 0.0) return 1.0;
    const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));
    const double normalised = rough / magnitude;
    if (normalised <= 1.0) return magnitude;
    if (normalised <= 2.0) return 2.0 * magnitude;
    if (normalised <= 5.0) return 5.0 * magnitude;
    return 10.0 * magnitude;
}

constexpr float kPi = 3.14159265358979f;

Vec2 onCircle(Vec2 centre, float radius, float degrees) {
    const float radians = degrees * kPi / 180.0f;
    return {centre.x + radius * std::cos(radians), centre.y + radius * std::sin(radians)};
}

/**
 * Sweeps an arc onto `path` as cubic Béziers.
 *
 * The handle length is `(4/3)·tan(Δ/4)` times the radius, **signed** by the
 * sweep. Both halves of that matter: a linear approximation of the tangent is
 * only right at exactly a quarter turn, and dropping the sign draws the return
 * arc of a ring backwards — which is what turned a donut's hole into a polygon.
 *
 * Cut into quarter turns because the approximation is only good over a short
 * sweep; four of them are a circle to within a thousandth of the radius.
 */
void arcTo(Path& path, Vec2 centre, float radius, float from, float to) {
    const int steps = std::max(1, static_cast<int>(std::ceil(std::fabs(to - from) / 90.0f)));
    const float sweep = (to - from) / static_cast<float>(steps);
    const float handle = radius * (4.0f / 3.0f) * std::tan(sweep * kPi / 180.0f / 4.0f);

    float at = from;
    for (int i = 0; i < steps; ++i) {
        const float next = at + sweep;
        const float a = at * kPi / 180.0f;
        const float b = next * kPi / 180.0f;
        const Vec2 start = onCircle(centre, radius, at);
        const Vec2 end = onCircle(centre, radius, next);
        // The handles run along the tangent at each end: (-sin, cos) is the
        // tangent to a circle, and `handle` carries the direction.
        const Vec2 c1{start.x - handle * std::sin(a), start.y + handle * std::cos(a)};
        const Vec2 c2{end.x + handle * std::sin(b), end.y - handle * std::cos(b)};
        path.cubicTo(c1, c2, end);
        at = next;
    }
}

/**
 * Draws a readout beside a mark, from lines that are already written.
 *
 * The half every chart shares, and the reason there is one of it: what differs
 * between a line, a bubble and a wedge is *which* mark the pointer found and
 * what there is to say about it. Where the panel goes, how it is flipped away
 * from an edge and what it looks like do not differ at all.
 *
 * Absolutely positioned inside the plot and flipped to whichever side has room,
 * so a readout for the last sample does not hang off the right edge. Placed
 * from *last* frame's plot geometry like everything else here.
 *
 * `atY` unset pins the panel to the top of the plot, which is right where the
 * mark is a whole column — a line's crosshair or a bar spans the height, and a
 * panel chasing the pointer up and down beside it only flickers. A mark that is
 * a *point* passes its own y, and the panel sits beside that instead.
 */
void drawReadoutPanel(Ui& ui, const Rect& plotFrame, float atX, std::optional<float> atY,
                      const std::string& heading, const std::vector<TooltipRow>& rows,
                      const ChartTooltip& config, const ChartTooltipStyle& base,
                      const TooltipContext& context) {
    if (plotFrame.empty()) return;
    if (rows.empty() && !config.body) return;

    const float radius = config.radius.value_or(base.radius);
    const bool swatches = config.swatches.value_or(base.swatches);
    const bool showTitle = base.showCategory && !heading.empty();

    // Guessed at, then corrected by measuring — the panel has to be placed this
    // frame, and its own width does not exist until it is laid out.
    float widest = showTitle ? ui.measure(heading, {.weight = FontWeight::SemiBold,
                                                    .size = base.fontSize}).width
                             : 0.0f;
    for (const TooltipRow& row : rows) {
        const float label = ui.measure(row.label, {.size = base.fontSize}).width;
        const float value = ui.measure(row.value, {.role = FontRole::Mono,
                                                   .size = base.fontSize}).width;
        widest = std::max(widest, label + value + 26.0f);
    }
    const float panelWidth = std::clamp(widest + base.padding * 2.0f, 74.0f, 260.0f);

    // Flipped to whichever side has room, and clamped so it never leaves the
    // plot — the readout is worthless where it cannot be read.
    constexpr float kGap = 12.0f;
    float left = atX + kGap;
    if (left + panelWidth > plotFrame.width) left = atX - kGap - panelWidth;
    left = std::clamp(left, 0.0f, std::max(0.0f, plotFrame.width - panelWidth));

    // The height is guessed rather than measured: a row is one line of text and
    // the gap under it, and the only thing the guess decides is whether the
    // panel is nudged up off the bottom edge. Being a few pixels out costs
    // nothing; not clamping at all costs a readout drawn past the plot.
    float top = 6.0f;
    if (atY) {
        const float line = base.fontSize + 5.0f;
        const float guess = base.padding * 2.0f + (showTitle ? line : 0.0f) +
                            line * static_cast<float>(rows.size());
        top = std::clamp(*atY - guess / 2.0f, 4.0f,
                         std::max(4.0f, plotFrame.height - guess - 4.0f));
    }

    Style panel;
    panel.position = Position::Absolute;
    panel.left = left;
    panel.top = top;
    panel.width = panelWidth;
    panel.direction = Direction::Column;
    panel.gap = 3.0f;
    panel.padding = Edges::all(base.padding);
    panel.radius = radius;
    panel.background = Fill{config.background.value_or(base.background)};
    panel.border = Border{1.0f, Fill{base.border}};
    // Above the plot's own marks, and never a pointer target: a readout that
    // can be hovered chases itself around the chart.
    panel.zIndex = 2;
    auto panelScope = ui.begin(panel);
    ui.ignoresPointer();

    if (config.body) {
        config.body(ui, context);
    } else {
        if (showTitle) {
            text(ui, heading, {.color = Token::TextMuted, .weight = FontWeight::SemiBold,
                               .size = base.fontSize});
        }
        for (const TooltipRow& row : rows) {
            Style lineStyle;
            lineStyle.direction = Direction::Row;
            lineStyle.align = Align::Center;
            lineStyle.gap = 6.0f;
            auto line = ui.begin(lineStyle);
            if (swatches && row.color) {
                Style dot;
                dot.width = 8.0f;
                dot.height = 8.0f;
                dot.shrink = 0.0f;
                dot.radius = 2.0f;
                dot.background = Fill{*row.color};
                ui.add(dot);
            }
            text(ui, row.label,
                 {.color = Token::Text, .size = base.fontSize, .grow = 1.0f,
                  .overflow = TextOverflow::Ellipsis});
            text(ui, row.value,
                 {.color = Token::TextStrong, .weight = FontWeight::SemiBold,
                  .role = FontRole::Mono, .size = base.fontSize});
            (void)line;
        }
    }
    (void)panelScope;
}

/** The heading a readout carries when the caller has not written one: the
 *  category's name where there is one, and the mark's number where there is
 *  not. */
std::string headingFor(const TooltipContext& context, const ChartTooltip& config,
                       const std::vector<std::string>& categories) {
    if (config.title) return config.title(context);
    if (context.index < categories.size()) return categories[context.index];
    return "#" + std::to_string(context.index);
}

/** One row per series at `index`, which is what a chart drawn from `Series`
 *  says when nobody has said otherwise. Shared by the line and the bars. */
std::vector<TooltipRow> seriesRows(const std::vector<Series>& series, std::size_t index,
                                   std::string_view valueFormat, const ChartStyle& style) {
    std::vector<TooltipRow> rows;
    for (std::size_t s = 0; s < series.size(); ++s) {
        const Series& one = series[s];
        if (index >= one.values.size()) continue;
        char value[48];
        std::snprintf(value, sizeof(value), std::string(valueFormat).c_str(), one.values[index]);
        rows.push_back({one.color.value_or(style.palette.empty()
                                               ? Token::Accent
                                               : style.palette[s % style.palette.size()]),
                        std::string(one.name), value});
    }
    return rows;
}

/** The readout for the two charts drawn from `Series`: a line and a bar. */
void drawSeriesReadout(Ui& ui, const Rect& plotFrame, float atX, std::optional<float> atY,
                       std::size_t index, const std::vector<Series>& series,
                       const std::vector<std::string>& categories,
                       std::string_view valueFormat, const ChartTooltip& config,
                       const ChartStyle& style) {
    TooltipContext context;
    context.index = index;
    context.series = &series;
    const std::vector<TooltipRow> rows =
        config.rows ? config.rows(context) : seriesRows(series, index, valueFormat, style);
    drawReadoutPanel(ui, plotFrame, atX, atY, headingFor(context, config, categories), rows,
                     config, style.tooltip, context);
}

/** One entry of a chart's key. */
struct LegendEntry {
    std::string_view name;
    Token color = Token::Accent;
};

/**
 * Draws the key under a chart and reports which series is singled out.
 *
 * Its own function because four charts want the identical thing, and because
 * what it *is* — a row of clickable labels — has nothing to do with any of
 * them: it never sees a value, a scale or a plot. The click is read after the
 * entries are built, so the tag it is matched against is this frame's.
 */
int drawLegend(Ui& ui, const Interaction& input, std::string_view id,
               const std::vector<LegendEntry>& entries, const ChartLegend& legend) {
    int focused = legend.focused ? *legend.focused : -1;
    if (focused >= static_cast<int>(entries.size())) focused = -1;

    bool named = false;
    for (const LegendEntry& entry : entries) named = named || !entry.name.empty();
    if (!legend.show || !named) return focused;

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.justify = Justify::Center;
    row.gap = 14.0f;
    row.height = legend.height;
    row.shrink = 0.0f;
    row.wrap = true;
    auto scope = ui.begin(row);

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name.empty()) continue;
        const std::string entryId = std::string(id) + ".legend." + std::to_string(i);
        const bool picked = focused == static_cast<int>(i);

        Style key;
        key.direction = Direction::Row;
        key.align = Align::Center;
        key.gap = 6.0f;
        key.shrink = 0.0f;
        key.padding = Edges::symmetric(2.0f, 6.0f);
        key.radius = 4.0f;
        // Only where a click means something: without somewhere to remember the
        // answer the legend is a key, and a key that lights up under the
        // pointer is promising something it cannot do.
        if (legend.focused) {
            key.cursorHint = Cursor::Pointer;
            if (picked) key.background = Fill{Token::SurfaceHover};
            else if (input.isHovered(entryId)) key.background = Fill{Token::SurfaceHover, 0.5f};
            key.opacity = focused < 0 || picked ? 1.0f : 0.55f;
        }
        auto keyScope = ui.begin(key);
        if (legend.focused) ui.tag(entryId).cursor(Cursor::Pointer);

        Style swatch;
        swatch.width = 9.0f;
        swatch.height = 9.0f;
        swatch.shrink = 0.0f;
        swatch.radius = 2.0f;
        swatch.background = Fill{entries[i].color};
        ui.add(swatch);

        text(ui, entries[i].name,
             {.color = picked ? Token::TextStrong : Token::TextMuted, .size = 11.0f});
        (void)keyScope;
    }
    (void)scope;

    if (legend.focused) {
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (!input.clicked(std::string(id) + ".legend." + std::to_string(i))) continue;
            // Clicking the one already singled out puts everything back, which
            // is the way out a reader looks for first.
            *legend.focused = focused == static_cast<int>(i) ? -1 : static_cast<int>(i);
            focused = *legend.focused;
        }
    }
    return focused;
}

/**
 * Whether the mark under the pointer should be picked out this frame.
 *
 * A hovered readout lives only as long as the pointer is over the plot; a
 * click-triggered one is remembered until the next click elsewhere, which is
 * what a reader on a touchscreen needs and what lets a long readout be read
 * without holding the pointer still.
 *
 * `None` still tracks. The caller asked for no *panel*, not to be told nothing:
 * a chart whose readout is drawn somewhere else on the page still needs to know
 * what is under the pointer, and its marks still highlight.
 */
bool marksTracked(Ui& ui, const Interaction& input, std::string_view id,
                  const std::string& plotId, ChartTrigger trigger) {
    if (trigger != ChartTrigger::Click) return input.isHovered(plotId);
    return ui.latch(id, "readout", 1.0f, input.clicked(plotId)) > 0.0f &&
           input.focused() == plotId;
}

/**
 * Reconciles what this chart found under its own pointer with what the rest of
 * its group found under theirs.
 *
 * Three cases, and the third is the whole feature: the chart being pointed at
 * publishes and keeps its own answer; the chart that published last time and is
 * no longer pointed at withdraws it, so a group goes quiet together; and a
 * chart pointed at by nobody adopts whatever the group is showing.
 *
 * `samples` is what the linked index is clamped against — a group whose charts
 * hold different numbers of samples still agrees where it can and says nothing
 * where it cannot, rather than reading off the end of the shorter one.
 */
int linkedIndex(ChartLink* link, std::string_view id, int mine, std::size_t samples) {
    if (!link) return mine;
    if (mine >= 0) {
        link->index = mine;
        link->source = std::string(id);
        return mine;
    }
    if (link->source == id) {
        link->index = -1;
        link->source.clear();
        return -1;
    }
    return link->index < static_cast<int>(samples) ? link->index : -1;
}

/** A full circle, as a path.
 *
 * `arcTo` takes **degrees** — it splits the sweep into 90-degree steps, since a
 * single cubic cannot bend further than that without the control handles
 * running off to infinity. Handing it radians draws a six-degree sliver. */
Path circlePath(Vec2 centre, float radius) {
    Path path;
    if (radius <= 0.0f) return path;
    path.moveTo(onCircle(centre, radius, 0.0f));
    arcTo(path, centre, radius, 0.0f, 360.0f);
    path.close();
    return path;
}

/** The samples `view` selects from a series, plus where the window starts.
 *
 * Inclusive of the sample just outside each end when there is one, so a line
 * drawn from a zoomed view still reaches both edges of the plot instead of
 * stopping short at the last whole sample inside it. */
struct Window {
    std::size_t first = 0;
    std::size_t last = 0;   ///< one past the end
    std::size_t count() const { return last > first ? last - first : 0; }
};

Window windowOf(std::size_t samples, const ChartView& view) {
    if (samples == 0) return {};
    const auto span = static_cast<double>(samples - 1);
    auto first = static_cast<std::size_t>(std::floor(view.from * span));
    auto last = static_cast<std::size_t>(std::ceil(view.to * span)) + 1;
    first = std::min(first, samples - 1);
    last = std::min(last, samples);
    if (last <= first) last = std::min(samples, first + 1);
    return {first, last};
}

/** Which pair of corners a bar's radius applies to — the end it grows to. */
enum class BarSide { Top, Bottom, Left, Right };

/** A rectangle with rounded top corners, as a path.
 *
 * Only the two corners the bar grows *towards* are rounded: rounding the pair
 * at the baseline would lift the bar off its own axis, which is exactly the
 * kind of small lie a chart cannot afford. `side` is which way that is. */
Path barPath(Rect box, float radius, BarSide side) {
    Path path;
    const float r = std::max(0.0f, std::min(radius, std::min(box.width, box.height) / 2.0f));
    const float x0 = box.x;
    const float y0 = box.y;
    const float x1 = box.right();
    const float y1 = box.bottom();
    // A quarter circle is close enough to a quadratic at these radii, and the
    // path layer has no arc primitive that takes a corner.
    const float k = r * 0.4477152502f;   // 1 - kappa, the control-point inset
    if (r <= 0.0f) {
        path.moveTo({x0, y0});
        path.lineTo({x1, y0});
        path.lineTo({x1, y1});
        path.lineTo({x0, y1});
        path.close();
        return path;
    }
    switch (side) {
        case BarSide::Top:
            path.moveTo({x0, y1});
            path.lineTo({x0, y0 + r});
            path.cubicTo({x0, y0 + k}, {x0 + k, y0}, {x0 + r, y0});
            path.lineTo({x1 - r, y0});
            path.cubicTo({x1 - k, y0}, {x1, y0 + k}, {x1, y0 + r});
            path.lineTo({x1, y1});
            break;
        case BarSide::Bottom:
            path.moveTo({x0, y0});
            path.lineTo({x0, y1 - r});
            path.cubicTo({x0, y1 - k}, {x0 + k, y1}, {x0 + r, y1});
            path.lineTo({x1 - r, y1});
            path.cubicTo({x1 - k, y1}, {x1, y1 - k}, {x1, y1 - r});
            path.lineTo({x1, y0});
            break;
        case BarSide::Right:
            path.moveTo({x0, y0});
            path.lineTo({x1 - r, y0});
            path.cubicTo({x1 - k, y0}, {x1, y0 + k}, {x1, y0 + r});
            path.lineTo({x1, y1 - r});
            path.cubicTo({x1, y1 - k}, {x1 - k, y1}, {x1 - r, y1});
            path.lineTo({x0, y1});
            break;
        case BarSide::Left:
            path.moveTo({x1, y0});
            path.lineTo({x0 + r, y0});
            path.cubicTo({x0 + k, y0}, {x0, y0 + k}, {x0, y0 + r});
            path.lineTo({x0, y1 - r});
            path.cubicTo({x0, y1 - k}, {x0 + k, y1}, {x0 + r, y1});
            path.lineTo({x1, y1});
            break;
    }
    path.close();
    return path;
}

}  // namespace

Scale Scale::nice(double low, double high, int targetTicks) {
    if (!(high > low)) {
        // A flat series still needs a chart to draw: give it a band rather than
        // a zero-height box. Anchored at zero when the value is not negative,
        // because a frame-time chart reading "-0.5 ms" is worse than no chart.
        const double middle = std::isfinite(low) ? low : 0.0;
        return middle >= 0.0 ? Scale{0.0, std::max(1.0, middle * 2.0)}
                             : Scale{middle - 1.0, middle + 1.0};
    }
    const double step = niceStep((high - low) / std::max(1, targetTicks));
    return Scale{std::floor(low / step) * step, std::ceil(high / step) * step};
}

std::vector<double> Scale::ticks(int targetTicks) const {
    std::vector<double> out;
    if (!(high > low)) return out;
    const double step = niceStep((high - low) / std::max(1, targetTicks));
    // A hair of slack on the end: the last tick is usually exactly `high`, and
    // floating point turns "exactly" into "a bit over" often enough to lose it.
    for (double at = std::ceil(low / step) * step; at <= high + step * 1e-6; at += step) {
        out.push_back(at);
    }
    return out;
}

namespace {

/**
 * The rubber band a plot draws over itself while a range is being swept out.
 *
 * In fractions of the *visible* window rather than of the whole series: by the
 * time it reaches the drawing it is a rectangle, and the plot it is drawn on
 * knows nothing about views.
 */
struct Band {
    /** The plot takes a sweep at all, which is what the cursor tells the
     *  reader before they press anything. */
    bool offered = false;
    bool active = false;
    float from = 0.0f;
    float to = 0.0f;
};

/**
 * What the windowed overload knows and a bare plot does not.
 *
 * Default-constructed it says "this *is* the whole series, and nothing is being
 * swept" — which is the plain chart, and why the plain chart passes nothing.
 */
struct Windowed {
    /** Where the drawn window starts in the caller's data. */
    std::size_t first = 0;
    /** The caller's whole data, when what is being drawn is a slice of it — one
     *  of these, whichever kind the chart takes. The readout reads from it and
     *  reports indices into it, so a zoomed chart still names the sample and
     *  the category a reader would find in their own data rather than the one
     *  at that offset into the window. */
    const std::vector<Series>* wholeSeries = nullptr;
    const std::vector<Candle>* wholeCandles = nullptr;
    Band sweep{};
    /**
     * The plot should claim the wheel *this frame*, so it declares
     * `Overflow::Scroll`.
     *
     * That is how a node says it consumes the wheel — see the enum, which names
     * "a chart that zooms" as the third thing in the stack competing for one
     * scroll. Without the declaration the pointer never resolves the plot as
     * the wheel's target and `ChartZoom::wheel` does nothing at all, which is
     * exactly what it did.
     *
     * Per frame rather than once, because the claim is conditional: a chart
     * wanting Ctrl and the wheel must *not* hold the wheel the rest of the
     * time, or it becomes a hole in the middle of a page that scrolls. So it
     * claims only while the modifier is actually down. The tree is rebuilt
     * every frame and hit testing reads the previous one, so the claim lands a
     * frame after the reader presses Ctrl — a modifier is held for far longer
     * than that.
     */
    bool takesWheel = false;
};

/** The rubber band, over everything it covers. Its edges are drawn as well as
 *  its fill: the fill alone is too faint at the width a careful reader picks. */
void appendSweep(std::vector<Shape>& shapes, const Band& band, float width, float height) {
    if (!band.active) return;
    const float left = std::min(band.from, band.to) * width;
    const float right = std::max(band.from, band.to) * width;
    Path box;
    box.moveTo({left, 0.0f});
    box.lineTo({right, 0.0f});
    box.lineTo({right, height});
    box.lineTo({left, height});
    box.close();
    shapes.push_back({std::move(box), Fill{Token::Accent, 0.16f}, 0.0f});
    for (const float x : {left, right}) {
        Path edge;
        edge.moveTo({x, 0.0f});
        edge.lineTo({x, height});
        shapes.push_back({std::move(edge), Fill{Token::Accent}, 1.0f});
    }
}

/**
 * The vertical rule through the mark under the pointer.
 *
 * One line, in one token, in every chart that indexes its samples — which is
 * what lets two of them stacked over the same axis read as a single crosshair
 * running down the pair instead of as two unrelated highlights. A bar chart
 * shades its slot as well, because a category is a *width* and the reader is
 * pointing at all of it; the rule is what says exactly where.
 */
void appendCrosshair(std::vector<Shape>& shapes, float x, float height) {
    Path rule;
    rule.moveTo({x, 0.0f});
    rule.lineTo({x, height});
    shapes.push_back({std::move(rule), Fill{Token::BorderStrong}, 1.0f});
}

/**
 * `series` is what the plot draws, which is a window of the caller's data when
 * there is a view over it — see `Windowed` for the rest.
 */
ChartResult drawLineChart(Ui& ui, const Interaction& input, std::string_view id,
                          const std::vector<Series>& series, const ChartOptions& options,
                          const Windowed& window) {
    ChartResult result;
    // Anything the caller left unset comes from the design, so a dashboard
    // configures its charts in one place.
    const ChartStyle& style = ui.design().chart;
    const int tickCount = options.tickCount.value_or(style.tickCount);
    const float axisWidth = options.axisWidth.value_or(style.axisWidth);
    const bool grid = options.grid.value_or(style.grid);

    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);

    std::size_t samples = 0;
    for (const Series& one : series) samples = std::max(samples, one.values.size());

    // ---- the scale ---------------------------------------------------------
    Scale scale = options.scale;
    if (options.autoScale) {
        double low = 0.0;
        double high = 0.0;
        bool seen = false;
        for (const Series& one : series) {
            for (const double value : one.values) {
                if (!std::isfinite(value)) continue;
                low = seen ? std::min(low, value) : value;
                high = seen ? std::max(high, value) : value;
                seen = true;
            }
        }
        scale = seen ? Scale::nice(std::min(low, 0.0), high, tickCount)
                     : Scale{0.0, 1.0};
    }

    // ---- what the pointer is over ------------------------------------------
    // Turned back into a *sample number*, not a pixel: that is what lets the
    // readout name the right point.
    const ChartTrigger trigger = options.tooltip.trigger.value_or(style.tooltip.trigger);
    // A reader drawing a range is not pointing at a sample, so the chart says
    // nothing is under the pointer until they let go — and because the answer
    // travels through the group, the crosshair, the readouts and the bridge
    // between two charts all fall quiet together rather than one at a time.
    const bool showing = !window.sweep.active && marksTracked(ui, input, id, plotId, trigger);
    if (options.hover && samples > 1 && !plotFrame.empty() && showing) {
        const float across =
            std::clamp((input.pointer().x - plotFrame.x) / plotFrame.width, 0.0f, 1.0f);
        result.hoveredIndex =
            static_cast<int>(std::lround(across * static_cast<float>(samples - 1)));
    }
    if (options.hover && samples > 0) {
        // The group speaks in whole-series samples, so a windowed chart hands
        // over `window.first + local` and takes back one it has to subtract
        // again — and reports nothing at all when the group is pointing at a
        // sample this chart has scrolled off.
        const int mine = result.hoveredIndex >= 0
                             ? result.hoveredIndex + static_cast<int>(window.first)
                             : -1;
        const int shared = linkedIndex(options.link, id, mine, window.first + samples);
        const int local = shared >= 0 ? shared - static_cast<int>(window.first) : -1;
        result.hoveredIndex = local >= 0 && local < static_cast<int>(samples) ? local : -1;
    }

    // The key, and which series it is singling out. Read before the marks are
    // drawn, because it decides what they look like.
    std::vector<LegendEntry> keys;
    for (std::size_t s = 0; s < series.size(); ++s) {
        keys.push_back({series[s].name,
                        series[s].color.value_or(style.palette.empty()
                                                     ? Token::Accent
                                                     : style.palette[s % style.palette.size()])});
    }
    const bool showLegend = options.legend.show && !series.empty();
    const int focused = options.legend.focused ? *options.legend.focused : -1;
    const auto shade = [&](std::size_t s, float alpha) {
        return focused >= 0 && focused != static_cast<int>(s) ? alpha * options.legend.dimAlpha
                                                              : alpha;
    };

    // A column only when there is a key to put under the plot: a chart with one
    // unnamed line should be the same node it always was.
    std::optional<Ui::Scope> outer;
    if (showLegend) {
        Style column;
        column.direction = Direction::Column;
        column.height = options.height + options.legend.height;
        column.gap = 2.0f;
        outer.emplace(ui.begin(column));
        ui.tag(id);
    }

    Style frame;
    frame.direction = Direction::Row;
    frame.gap = 6.0f;
    if (showLegend) {
        frame.grow = 1.0f;
        frame.basis = 0.0f;
    } else {
        frame.height = options.height;
    }
    auto scope = ui.begin(frame);
    if (!showLegend) ui.tag(id);

    // ---- the axis ----------------------------------------------------------
    const std::vector<double> ticks = scale.ticks(tickCount);
    if (axisWidth > 0.0f) {
        Style axis;
        axis.width = axisWidth;
        axis.shrink = 0.0f;
        auto axisScope = ui.begin(axis);
        // Positioned rather than stacked: a label belongs beside its own
        // gridline, and a column would space them evenly instead.
        for (const double at : ticks) {
            if (plotFrame.height <= 0.0f) break;
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), std::string(options.valueFormat).c_str(), at);
            Style slot;
            slot.position = Position::Absolute;
            slot.left = 0.0f;
            slot.top = (1.0f - scale.fraction(at)) * plotFrame.height - 7.0f;
            slot.width = axisWidth;
            slot.justify = Justify::End;
            auto slotScope = ui.begin(slot);
            text(ui, buffer, {.color = Token::TextMuted, .size = 10.0f, .align = TextAlign::End});
            (void)slotScope;
        }
        (void)axisScope;
    }

    // ---- the plot ----------------------------------------------------------
    std::vector<Shape> shapes;
    const float width = plotFrame.width;
    const float height = plotFrame.height;
    const auto xAt = [&](std::size_t index) {
        return samples > 1 ? width * static_cast<float>(index) / static_cast<float>(samples - 1)
                           : width / 2.0f;
    };
    const auto yAt = [&](double value) { return height * (1.0f - scale.fraction(value)); };

    if (width > 0.0f && height > 0.0f) {
        if (grid) {
            for (const double at : ticks) {
                Path line;
                const float y = yAt(at);
                line.moveTo({0.0f, y});
                line.lineTo({width, y});
                shapes.push_back({std::move(line), Fill{Token::Border}, 1.0f});
            }
        }

        for (std::size_t s = 0; s < series.size(); ++s) {
            const Series& one = series[s];
            if (one.values.size() < 2) continue;
            const Token colour = one.color.value_or(
                style.palette.empty() ? Token::Accent : style.palette[s % style.palette.size()]);
            const float fillAlpha = one.fillAlpha.value_or(style.fillAlpha);
            const float thickness = one.thickness.value_or(style.lineThickness);

            // The area first, so the line sits on top of its own shading.
            if (fillAlpha > 0.0f) {
                Path area;
                area.moveTo({xAt(0), height});
                for (std::size_t i = 0; i < one.values.size(); ++i) {
                    area.lineTo({xAt(i), yAt(one.values[i])});
                }
                area.lineTo({xAt(one.values.size() - 1), height});
                area.close();
                shapes.push_back({std::move(area), Fill{colour, shade(s, fillAlpha)}, 0.0f});
            }

            Path line;
            line.moveTo({xAt(0), yAt(one.values[0])});
            for (std::size_t i = 1; i < one.values.size(); ++i) {
                line.lineTo({xAt(i), yAt(one.values[i])});
            }
            shapes.push_back({std::move(line), Fill{colour, shade(s, 1.0f)}, thickness});
        }

        // The crosshair, and a dot on each series at the sample under it.
        if (result.hoveredIndex >= 0) {
            const auto index = static_cast<std::size_t>(result.hoveredIndex);
            const float x = xAt(index);
            appendCrosshair(shapes, x, height);
            for (std::size_t s = 0; s < series.size(); ++s) {
                const Series& one = series[s];
                if (index >= one.values.size()) continue;
                const Token colour = one.color.value_or(
                    style.palette.empty() ? Token::Accent
                                          : style.palette[s % style.palette.size()]);
                const float y = yAt(one.values[index]);
                // A ring drawn as a short thick stroke: the path layer has no
                // circle, and at four pixels nobody can tell.
                Path dot;
                dot.moveTo({x - 0.1f, y});
                dot.lineTo({x + 0.1f, y});
                shapes.push_back({std::move(dot), Fill{colour}, 6.0f});
            }
        }

        // The sweep, last, so it lies over everything it covers — a band that
        // went under the lines would be read as a series rather than as a
        // selection.
        appendSweep(shapes, window.sweep, width, height);
    }

    // The plot is a box holding two things: the marks, and the readout. It has
    // to be a container rather than the drawn node itself, because the readout
    // is positioned absolutely and absolute is measured from the parent — put
    // beside the plot instead of inside it, it would be offset by the axis.
    Style plot;
    plot.grow = 1.0f;
    plot.basis = 0.0f;
    // A crosshair over a plot that can be swept, for the same reason a resize
    // handle shows a resize cursor: the gesture is invisible until it is
    // offered, and nobody drags at a chart to find out.
    if (window.sweep.offered) plot.cursorHint = Cursor::Crosshair;
    if (window.takesWheel) plot.overflow = Overflow::Scroll;
    auto plotScope = ui.begin(plot);
    ui.tag(plotId);

    Style marks;
    marks.position = Position::Absolute;
    marks.left = 0.0f;
    marks.top = 0.0f;
    marks.width = Length::percent(100);
    marks.height = Length::percent(100);
    marks.overflow = Overflow::Hidden;
    ui.draw(marks, std::move(shapes));

    if (result.hoveredIndex >= 0 && trigger != ChartTrigger::None) {
        const auto index = static_cast<std::size_t>(result.hoveredIndex);
        drawSeriesReadout(ui, plotFrame, xAt(index), std::nullopt, window.first + index,
                          window.wholeSeries ? *window.wholeSeries : series,
                          options.categories,
                          options.valueFormat, options.tooltip, style);
    }
    // Closed by hand and in order — the plot is inside the frame, and the frame
    // inside the column the key goes in. Left to their destructors they would
    // all close after it, and the key would be laid out beside the plot rather
    // than under the chart.
    plotScope.close();
    scope.close();

    if (showLegend) drawLegend(ui, input, id, keys, options.legend);
    if (outer) outer->close();

    return result;
}

}  // namespace

ChartResult lineChart(Ui& ui, const Interaction& input, std::string_view id,
                      const std::vector<Series>& series, const ChartOptions& options) {
    return drawLineChart(ui, input, id, series, options, {});
}

void ChartView::normalise(double minSpan) {
    if (to < from) std::swap(from, to);
    const double least = std::clamp(minSpan, 0.0005, 1.0);
    if (to - from < least) {
        const double middle = (from + to) / 2.0;
        from = middle - least / 2.0;
        to = middle + least / 2.0;
    }
    // Slide rather than squash when the window runs off an end: a reader
    // dragging towards the edge means "show me the edge", not "show me less".
    if (from < 0.0) {
        to -= from;
        from = 0.0;
    }
    if (to > 1.0) {
        from -= to - 1.0;
        to = 1.0;
    }
    from = std::max(0.0, from);
    to = std::min(1.0, to);
}

namespace {

/** Applies the wheel and drag gestures to `view`, and says whether either
 *  fired. Shared by every chart that takes a view. */
struct Gestures {
    bool changed = false;
    bool wheelTaken = false;
};

Gestures driveView(const Interaction& input, std::string_view plotId, const Rect& plotFrame,
                   ChartView& view, const ChartZoom& zoom) {
    Gestures out;
    if (plotFrame.empty()) return out;

    // The wheel. Two guards before anything happens, and both matter: the
    // pointer must be over *this* plot and nothing scrollable inside it, and
    // the caller must have asked for wheel zoom at all.
    if (zoom.wheel && input.wheel() != 0.0f && input.wheelTarget() == plotId &&
        (!zoom.wheelModifier || input.modifiers().command())) {
        // Zoom about the pointer, so whatever the reader is looking at stays
        // under the cursor. Zooming about the centre instead makes the point of
        // interest slide away, and they chase it.
        const double at = view.from + view.span() * std::clamp<double>(
                              (input.pointer().x - plotFrame.x) / plotFrame.width, 0.0, 1.0);
        const double factor = input.wheel() > 0.0f ? 0.84 : 1.0 / 0.84;
        const double span = std::clamp(view.span() * factor, zoom.minSpan, 1.0);
        view.from = at - (at - view.from) * (span / view.span());
        view.to = view.from + span;
        view.normalise(zoom.minSpan);
        out.changed = true;
        out.wheelTaken = true;
    }

    // Where the pointer is, as a fraction of the whole series — the units the
    // view and the sweep are both kept in, so neither depends on a pixel.
    const auto atPointer = [&] {
        const double across =
            std::clamp<double>((input.pointer().x - plotFrame.x) / plotFrame.width, 0.0, 1.0);
        return view.from + view.span() * across;
    };

    // ---- sweeping out a range ---------------------------------------------
    // The press anchors it, every frame after it moves the loose end, and the
    // release commits. Nothing is applied to the view in between: rescaling
    // the plot mid-gesture would slide the data out from under the pointer
    // that is aiming at it.
    // Only the plot the press landed on drives the sweep. Every other chart
    // sharing this view draws the band and keeps its hands off the gesture.
    const bool mine = view.sweep.on == plotId;
    if (zoom.drag == ChartDrag::Select) {
        if (input.dragging() == plotId) {
            if (input.pressStarted(plotId)) {
                // Shift means pan, decided once at the press: a modifier read
                // every frame would turn one gesture into two halfway through.
                if (!input.modifiers().shift) {
                    view.sweep = {true, atPointer(), atPointer(), std::string(plotId)};
                }
            } else if (view.sweep.active && mine) {
                view.sweep.to = atPointer();
            }
        } else if (view.sweep.active && mine) {
            const double from = std::min(view.sweep.from, view.sweep.to);
            const double to = std::max(view.sweep.from, view.sweep.to);
            const double least = static_cast<double>(std::max(0.0f, zoom.dragThreshold)) /
                                 plotFrame.width * view.span();
            view.sweep = {};
            if (to - from > least) {
                view.from = from;
                view.to = to;
                view.normalise(zoom.minSpan);
                out.changed = true;
            }
        }
    }

    // ---- panning -----------------------------------------------------------
    // Slides the view by however far the pointer moved, converted back into
    // data space so the data tracks the pointer exactly. In `Select` this is
    // the Shift half of the gesture, which is the drag that started no sweep.
    const bool panning = zoom.drag == ChartDrag::Pan ||
                         (zoom.drag == ChartDrag::Select && !view.sweep.active);
    if (panning && input.dragging() == plotId && !view.whole()) {
        const float moved = input.pointerDelta().x;
        if (moved != 0.0f) {
            const double shift = -static_cast<double>(moved) / plotFrame.width * view.span();
            view.from += shift;
            view.to += shift;
            view.normalise(zoom.minSpan);
            out.changed = true;
        }
    }
    return out;
}

/**
 * What a windowed chart hands down to the plot: where the window starts, and
 * the sweep in the window's own fractions.
 *
 * The sweep is carried in fractions of the whole series and drawn in fractions
 * of the window, and this is the one place the two meet.
 */
Windowed windowedFor(const Interaction& input, const ChartView& view, const ChartZoom& zoom,
                     const Window& window) {
    Windowed out;
    out.first = window.first;
    out.takesWheel = zoom.wheel && (!zoom.wheelModifier || input.modifiers().command());
    out.sweep.offered = zoom.drag == ChartDrag::Select;
    out.sweep.active = view.sweep.active;
    if (out.sweep.active && view.span() > 0.0) {
        out.sweep.from = static_cast<float>((view.sweep.from - view.from) / view.span());
        out.sweep.to = static_cast<float>((view.sweep.to - view.from) / view.span());
    }
    return out;
}

}  // namespace

ChartResult lineChart(Ui& ui, const Interaction& input, std::string_view id,
                      const std::vector<Series>& series, ChartView& view,
                      const ChartOptions& options, const ChartZoom& zoom) {
    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);
    view.normalise(zoom.minSpan);
    const Gestures gestures = driveView(input, plotId, plotFrame, view, zoom);

    // Slicing here rather than inside the chart keeps `lineChart` itself
    // unaware of views: it draws whatever series it is handed, edge to edge.
    std::size_t samples = 0;
    for (const Series& one : series) samples = std::max(samples, one.values.size());
    const Window window = windowOf(samples, view);

    std::vector<Series> sliced;
    sliced.reserve(series.size());
    for (const Series& one : series) {
        Series cut = one;
        cut.values.clear();
        for (std::size_t i = window.first; i < window.last && i < one.values.size(); ++i) {
            cut.values.push_back(one.values[i]);
        }
        sliced.push_back(std::move(cut));
    }

    Windowed windowed = windowedFor(input, view, zoom, window);
    windowed.wholeSeries = &series;
    ChartResult result = drawLineChart(ui, input, id, sliced, options, windowed);
    // Reported in the *whole* series' terms, so a caller can name the sample
    // without knowing the view exists.
    if (result.hoveredIndex >= 0) {
        result.hoveredIndex += static_cast<int>(window.first);
    }
    result.viewChanged = gestures.changed;
    result.wheelTaken = gestures.wheelTaken;
    return result;
}

bool chartToolbar(Ui& ui, const Interaction& input, std::string_view id, ChartView& view,
                  const ChartToolbarOptions& options) {
    bool changed = false;

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 2.0f;
    row.shrink = 0.0f;
    auto scope = ui.begin(row);

    // Zooming about the *middle*, where the wheel zooms about the pointer.
    // There is no pointer in a button press — the reader is looking at the
    // chart, not at where their cursor happens to be resting — and the middle
    // is the only part of the window they can be sure stays.
    const auto scale = [&](double factor) {
        const double middle = (view.from + view.to) / 2.0;
        const double span = std::clamp(view.span() * factor, options.minSpan, 1.0);
        view.from = middle - span / 2.0;
        view.to = middle + span / 2.0;
        view.normalise(options.minSpan);
        changed = true;
    };

    const double step = std::clamp(options.step, 0.05, 0.9);
    const std::string base = std::string(id);
    if (options.zoomIn) {
        button(ui, input, "",
               {.variant = options.variant,
                .leading = Icon::Plus,
                .disabled = view.span() <= options.minSpan,
                .height = options.height,
                .iconSize = options.iconSize,
                .id = base + ".in"});
        if (input.clicked(base + ".in")) scale(1.0 - step);
    }
    if (options.zoomOut) {
        button(ui, input, "",
               {.variant = options.variant,
                .leading = Icon::Minus,
                .disabled = view.whole(),
                .height = options.height,
                .iconSize = options.iconSize,
                .id = base + ".out"});
        if (input.clicked(base + ".out")) scale(1.0 / (1.0 - step));
    }
    if (options.reset) {
        button(ui, input, "",
               {.variant = options.variant,
                .leading = Icon::RotateCcw,
                .disabled = view.whole(),
                .height = options.height,
                .iconSize = options.iconSize,
                .id = base + ".reset"});
        if (input.clicked(base + ".reset")) {
            view.reset();
            changed = true;
        }
    }

    (void)scope;
    return changed;
}

bool chartBrush(Ui& ui, const Interaction& input, std::string_view id,
                const std::vector<Series>& series, ChartView& view,
                const BrushOptions& options) {
    const ChartStyle& style = ui.design().chart;
    const std::string stripId = std::string(id) + ".strip";
    const std::string leftId = std::string(id) + ".from";
    const std::string rightId = std::string(id) + ".to";
    const Rect stripFrame = input.frameOf(stripId);
    bool changed = false;

    view.normalise();

    // ---- the gestures ------------------------------------------------------
    // Handles first: a press that lands on one is a resize, and only a press
    // that missed both can mean anything else.
    if (!stripFrame.empty() && stripFrame.width > 0.0f) {
        const auto atPointer = [&] {
            return std::clamp<double>((input.pointer().x - stripFrame.x) / stripFrame.width, 0.0,
                                      1.0);
        };
        if (input.dragging() == leftId) {
            view.from = atPointer();
            view.normalise();
            changed = true;
        } else if (input.dragging() == rightId) {
            view.to = atPointer();
            view.normalise();
            changed = true;
        } else if (input.dragging() == stripId) {
            if (options.dragSelects && input.pressStarted(stripId)) {
                // A fresh press on the strip starts a new window at that point.
                view.from = atPointer();
                view.to = atPointer();
                changed = true;
            } else if (options.dragSelects) {
                view.to = atPointer();
                changed = true;
            }
            // Not normalised while dragging: a window dragged right to left is
            // still being drawn, and swapping the ends mid-gesture would make
            // the handle jump out from under the pointer. It is squared up when
            // the button comes back up.
        }
        if (changed && input.dragging().empty()) view.normalise();
    }

    // ---- the strip ---------------------------------------------------------
    Style row;
    row.direction = Direction::Row;
    row.height = options.height;
    row.shrink = 0.0f;
    row.gap = 6.0f;
    auto rowScope = ui.begin(row);
    ui.tag(id);

    if (options.axisWidth > 0.0f) {
        Style spacer;
        spacer.width = options.axisWidth;
        spacer.shrink = 0.0f;
        ui.add(spacer);
    }

    std::vector<Shape> shapes;
    const float width = stripFrame.width;
    const float height = stripFrame.height;
    if (width > 0.0f && height > 0.0f) {
        // The whole series, flattened to fit — no axis, no ticks. This is a
        // map, not a chart: the reader is locating themselves on it.
        double low = 0.0;
        double high = 0.0;
        bool seen = false;
        for (const Series& one : series) {
            for (const double value : one.values) {
                if (!std::isfinite(value)) continue;
                low = seen ? std::min(low, value) : value;
                high = seen ? std::max(high, value) : value;
                seen = true;
            }
        }
        const Scale scale = seen ? Scale{std::min(low, 0.0), high} : Scale{0.0, 1.0};

        for (std::size_t s = 0; s < series.size(); ++s) {
            const Series& one = series[s];
            if (one.values.size() < 2) continue;
            const Token colour = one.color.value_or(
                style.palette.empty() ? Token::Accent : style.palette[s % style.palette.size()]);
            const auto span = static_cast<float>(one.values.size() - 1);
            const auto xAt = [&](std::size_t i) {
                return width * static_cast<float>(i) / span;
            };
            const auto yAt = [&](double v) { return height * (1.0f - scale.fraction(v)); };
            Path line;
            line.moveTo({xAt(0), yAt(one.values[0])});
            for (std::size_t i = 1; i < one.values.size(); ++i) {
                line.lineTo({xAt(i), yAt(one.values[i])});
            }
            shapes.push_back({std::move(line), Fill{colour, 0.9f}, 1.0f});
        }

        // Everything outside the window is dimmed, rather than the window being
        // highlighted: the parts not shown are the ones that need explaining.
        const auto edge = [&](double at) { return static_cast<float>(at) * width; };
        const float fromX = edge(std::min(view.from, view.to));
        const float toX = edge(std::max(view.from, view.to));
        for (const Rect box : {Rect{0.0f, 0.0f, fromX, height},
                               Rect{toX, 0.0f, width - toX, height}}) {
            if (box.width <= 0.0f) continue;
            Path shade;
            shade.moveTo({box.x, box.y});
            shade.lineTo({box.right(), box.y});
            shade.lineTo({box.right(), box.bottom()});
            shade.lineTo({box.x, box.bottom()});
            shade.close();
            shapes.push_back({std::move(shade), Fill{Token::Bg, 0.62f}, 0.0f});
        }

        for (const float at : {fromX, toX}) {
            Path rule;
            rule.moveTo({at, 0.0f});
            rule.lineTo({at, height});
            shapes.push_back({std::move(rule), Fill{Token::Accent}, 1.5f});
        }
    }

    // The strip and its handles share a box, because a handle is positioned
    // absolutely and absolute is measured from the *parent's* content box. Sat
    // directly in the row, the handles would be offset by the axis spacer
    // beside them — which is exactly what they were.
    Style box;
    box.grow = 1.0f;
    box.basis = 0.0f;
    auto boxScope = ui.begin(box);

    Style strip;
    strip.width = Length::percent(100);
    strip.height = Length::percent(100);
    strip.position = Position::Absolute;
    strip.left = 0.0f;
    strip.top = 0.0f;
    strip.overflow = Overflow::Hidden;
    strip.cursorHint = Cursor::Text;
    ui.draw(strip, std::move(shapes));
    ui.tag(stripId).cursor(Cursor::Text);

    // The handles, as absolute children of the strip. Wider than the rule they
    // sit on, because a 1.5-pixel target is not a target.
    if (width > 0.0f) {
        const auto place = [&](std::string_view tag, double at) {
            Style grip;
            grip.position = Position::Absolute;
            grip.left = static_cast<float>(at) * width - options.handleWidth / 2.0f;
            grip.top = 0.0f;
            grip.width = options.handleWidth;
            grip.height = Length::percent(100);
            grip.zIndex = 1;
            grip.cursorHint = Cursor::ResizeHorizontal;
            grip.justify = Justify::Center;
            auto gripScope = ui.begin(grip);
            ui.tag(tag).cursor(Cursor::ResizeHorizontal);
            Style bar;
            bar.width = input.isHovered(tag) || input.dragging() == tag ? 4.0f : 2.5f;
            bar.height = Length::percent(56);
            bar.radius = 1.5f;
            bar.margin = Edges::symmetric(static_cast<float>(options.height) * 0.22f, 0.0f);
            bar.background = Fill{Token::Accent};
            ui.add(bar);
            (void)gripScope;
        };
        place(leftId, std::min(view.from, view.to));
        place(rightId, std::max(view.from, view.to));
    }

    (void)boxScope;
    (void)rowScope;
    return changed;
}

namespace {

ChartResult drawBarChart(Ui& ui, const Interaction& input, std::string_view id,
                         const std::vector<Series>& series, const BarChartOptions& options,
                         const Windowed& window) {
    ChartResult result;
    const ChartStyle& style = ui.design().chart;
    const int tickCount = options.tickCount.value_or(style.tickCount);
    const float axisWidth = options.axisWidth.value_or(style.axisWidth);
    const bool grid = options.grid.value_or(style.grid);
    const bool stacked = options.grouping == BarGrouping::Stacked;
    const bool flat = options.horizontal;

    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);

    std::size_t samples = 0;
    for (const Series& one : series) samples = std::max(samples, one.values.size());

    // ---- the scale ---------------------------------------------------------
    // Stacked bars are measured by their total, not by their tallest part, or
    // the top of the pile leaves the plot.
    Scale scale = options.scale;
    if (options.autoScale) {
        double low = 0.0;
        double high = 0.0;
        for (std::size_t i = 0; i < samples; ++i) {
            double positive = 0.0;
            double negative = 0.0;
            for (const Series& one : series) {
                if (i >= one.values.size()) continue;
                const double value = one.values[i];
                if (!std::isfinite(value)) continue;
                if (stacked) {
                    (value >= 0.0 ? positive : negative) += value;
                } else {
                    positive = std::max(positive, value);
                    negative = std::min(negative, value);
                }
            }
            high = std::max(high, positive);
            low = std::min(low, negative);
        }
        scale = Scale::nice(low, high, tickCount);
    }

    // ---- what the pointer is over ------------------------------------------
    // A whole category, not a nearest point: a bar chart's unit of meaning is
    // the slot, and a reader pointing anywhere in it means that one.
    const ChartTrigger trigger = options.tooltip.trigger.value_or(style.tooltip.trigger);
    if (options.hover && samples > 0 && !plotFrame.empty() && !window.sweep.active &&
        marksTracked(ui, input, id, plotId, trigger)) {
        const float along = flat ? (input.pointer().y - plotFrame.y) / plotFrame.height
                                 : (input.pointer().x - plotFrame.x) / plotFrame.width;
        const auto slot = static_cast<int>(std::floor(std::clamp(along, 0.0f, 0.9999f) *
                                                      static_cast<float>(samples)));
        result.hoveredIndex = std::clamp(slot, 0, static_cast<int>(samples) - 1);
    }
    if (options.hover) {
        const int mine = result.hoveredIndex >= 0
                             ? result.hoveredIndex + static_cast<int>(window.first)
                             : -1;
        const int shared = linkedIndex(options.link, id, mine, window.first + samples);
        const int local = shared >= 0 ? shared - static_cast<int>(window.first) : -1;
        result.hoveredIndex = local >= 0 && local < static_cast<int>(samples) ? local : -1;
    }

    const std::vector<double> ticks = scale.ticks(tickCount);
    // The axis down the side holds values when the bars stand up and category
    // names when they lie down; the one along the bottom holds the other.
    const float sideWidth = flat ? options.categoryAxis : axisWidth;
    const float bottomHeight = flat ? 18.0f : options.categoryAxis;

    // Offset into the *caller's* categories: `series` may be a window over their
    // data, and a label read at the window's own index is the wrong label by
    // exactly however far the reader has scrolled.
    const auto categoryLabel = [&](std::size_t i) -> std::string_view {
        const std::size_t at = window.first + i;
        return at < options.categories.size() ? std::string_view(options.categories[at])
                                              : std::string_view{};
    };

    std::vector<LegendEntry> keys;
    for (std::size_t s = 0; s < series.size(); ++s) {
        keys.push_back({series[s].name,
                        series[s].color.value_or(style.palette.empty()
                                                     ? Token::Accent
                                                     : style.palette[s % style.palette.size()])});
    }
    const bool showLegend = options.legend.show && !series.empty();
    const int focused = options.legend.focused ? *options.legend.focused : -1;
    const auto shade = [&](std::size_t s, float alpha) {
        return focused >= 0 && focused != static_cast<int>(s) ? alpha * options.legend.dimAlpha
                                                              : alpha;
    };

    Style outer;
    outer.direction = Direction::Column;
    outer.height = options.height + (showLegend ? options.legend.height : 0.0f);
    outer.gap = 4.0f;
    auto outerScope = ui.begin(outer);
    ui.tag(id);

    {
        Style top;
        top.direction = Direction::Row;
        top.grow = 1.0f;
        top.basis = 0.0f;
        top.gap = 6.0f;
        auto topScope = ui.begin(top);

        if (sideWidth > 0.0f) {
            Style axis;
            axis.width = sideWidth;
            axis.shrink = 0.0f;
            auto axisScope = ui.begin(axis);
            if (plotFrame.height > 0.0f) {
                if (flat) {
                    const float slot = plotFrame.height / static_cast<float>(std::max<std::size_t>(samples, 1));
                    for (std::size_t i = 0; i < samples; ++i) {
                        if (categoryLabel(i).empty()) continue;
                        Style cell;
                        cell.position = Position::Absolute;
                        cell.left = 0.0f;
                        cell.top = (static_cast<float>(i) + 0.5f) * slot - 7.0f;
                        cell.width = sideWidth;
                        cell.justify = Justify::End;
                        auto cellScope = ui.begin(cell);
                        text(ui, categoryLabel(i),
                             {.color = Token::TextMuted, .size = 10.0f,
                              .align = TextAlign::End, .overflow = TextOverflow::Ellipsis});
                        (void)cellScope;
                    }
                } else {
                    for (const double at : ticks) {
                        char buffer[32];
                        std::snprintf(buffer, sizeof(buffer),
                                      std::string(options.valueFormat).c_str(), at);
                        Style slot;
                        slot.position = Position::Absolute;
                        slot.left = 0.0f;
                        // Clamped inside the plot. A label sits half its height above
                // its own gridline, so the bottom tick's used to hang seven
                // pixels below the plot and land on whatever was under it —
                // which, for a price chart with its volume beneath, was the
                // volume's own top label.
                slot.top = std::clamp((1.0f - scale.fraction(at)) * plotFrame.height - 7.0f,
                                      0.0f, std::max(0.0f, plotFrame.height - 14.0f));
                        slot.width = sideWidth;
                        slot.justify = Justify::End;
                        auto slotScope = ui.begin(slot);
                        text(ui, buffer,
                             {.color = Token::TextMuted, .size = 10.0f, .align = TextAlign::End});
                        (void)slotScope;
                    }
                }
            }
            (void)axisScope;
        }

        // ---- the plot ------------------------------------------------------
        std::vector<Shape> shapes;
        const float width = plotFrame.width;
        const float height = plotFrame.height;
        // Along = the category axis, across = the value axis.
        const float along = flat ? height : width;
        const float across = flat ? width : height;
        const auto valueAt = [&](double value) {
            const float f = scale.fraction(value);
            return flat ? across * f : across * (1.0f - f);
        };
        const float baseline = valueAt(std::clamp(0.0, scale.low, scale.high));

        if (width > 0.0f && height > 0.0f && samples > 0) {
            if (grid) {
                for (const double at : ticks) {
                    Path line;
                    const float v = valueAt(at);
                    if (flat) {
                        line.moveTo({v, 0.0f});
                        line.lineTo({v, height});
                    } else {
                        line.moveTo({0.0f, v});
                        line.lineTo({width, v});
                    }
                    shapes.push_back({std::move(line), Fill{Token::Border}, 1.0f});
                }
            }

            // The hovered category, shaded behind its bars so the highlight
            // does not sit on top of the data it is pointing at.
            const float slot = along / static_cast<float>(samples);
            if (result.hoveredIndex >= 0) {
                const float at = static_cast<float>(result.hoveredIndex) * slot;
                Path band;
                const Rect box = flat ? Rect{0.0f, at, width, slot} : Rect{at, 0.0f, slot, height};
                band.moveTo({box.x, box.y});
                band.lineTo({box.right(), box.y});
                band.lineTo({box.right(), box.bottom()});
                band.lineTo({box.x, box.bottom()});
                band.close();
                shapes.push_back({std::move(band), Fill{Token::SurfaceHover, 0.5f}, 0.0f});
                // And the rule down its middle, which is the part that lines up
                // with the chart above or below it.
                if (!flat) appendCrosshair(shapes, at + slot / 2.0f, height);
            }

            const float inner = slot * (1.0f - std::clamp(options.categoryPadding, 0.0f, 0.9f));
            const auto count = static_cast<float>(std::max<std::size_t>(series.size(), 1));
            const float lane =
                stacked ? inner
                        : std::max(1.0f, (inner - options.seriesGap * (count - 1.0f)) / count);

            // Where the last stack for each category ended, so the next series
            // starts on top of it rather than at the baseline.
            std::vector<float> stackTop(samples, baseline);
            std::vector<float> stackBottom(samples, baseline);

            for (std::size_t sIndex = 0; sIndex < series.size(); ++sIndex) {
                const Series& one = series[sIndex];
                const Token colour = one.color.value_or(
                    style.palette.empty() ? Token::Accent
                                          : style.palette[sIndex % style.palette.size()]);

                for (std::size_t i = 0; i < samples && i < one.values.size(); ++i) {
                    const double value = one.values[i];
                    if (!std::isfinite(value)) continue;
                    const float slotStart = static_cast<float>(i) * slot + (slot - inner) / 2.0f;
                    const float laneStart =
                        stacked ? slotStart
                                : slotStart + static_cast<float>(sIndex) *
                                                  (lane + options.seriesGap);

                    float from = baseline;
                    float to = valueAt(value);
                    if (stacked) {
                        const bool up = flat ? to >= baseline : to <= baseline;
                        float& anchor = up ? stackTop[i] : stackBottom[i];
                        const float length = to - baseline;
                        from = anchor;
                        to = anchor + length;
                        anchor = to;
                    }

                    if (options.shape == BarShape::Lollipop) {
                        // A stem and a dot. Drawn as strokes, because a
                        // two-pixel rectangle and a two-pixel line are the same
                        // thing and the line is one contour instead of four.
                        const float centre = laneStart + lane / 2.0f;
                        Path stem;
                        if (flat) {
                            stem.moveTo({from, centre});
                            stem.lineTo({to, centre});
                        } else {
                            stem.moveTo({centre, from});
                            stem.lineTo({centre, to});
                        }
                        shapes.push_back({std::move(stem), Fill{colour, shade(sIndex, 0.55f)},
                                          2.0f});

                        Path dot;
                        const Vec2 head = flat ? Vec2{to, centre} : Vec2{centre, to};
                        dot.moveTo({head.x - 0.1f, head.y});
                        dot.lineTo({head.x + 0.1f, head.y});
                        shapes.push_back(
                            {std::move(dot), Fill{colour, shade(sIndex, 1.0f)},
                             std::min(lane, style.lineThickness * 4.0f + 4.0f)});
                        continue;
                    }

                    const float low = std::min(from, to);
                    const float high = std::max(from, to);
                    const Rect box = flat ? Rect{low, laneStart, high - low, lane}
                                          : Rect{laneStart, low, lane, high - low};
                    const BarSide side = flat ? (to >= from ? BarSide::Right : BarSide::Left)
                                              : (to <= from ? BarSide::Top : BarSide::Bottom);
                    const bool dim =
                        result.hoveredIndex >= 0 && static_cast<std::size_t>(result.hoveredIndex) != i;
                    shapes.push_back({barPath(box, options.radius, side),
                                      Fill{colour, shade(sIndex, dim ? 0.55f : 1.0f)}, 0.0f});
                }
            }
        }

        // A container holding the marks rather than the drawn node itself, so
        // the readout can be positioned inside it — the shape the line chart
        // uses, and for the same reason: absolute is measured from the parent,
        // and a panel put beside the plot would be offset by the axis.
        Style plot;
        plot.grow = 1.0f;
        plot.basis = 0.0f;
        if (window.sweep.offered) plot.cursorHint = Cursor::Crosshair;
        if (window.takesWheel) plot.overflow = Overflow::Scroll;
        auto plotScope = ui.begin(plot);
        ui.tag(plotId);

        Style marks;
        marks.position = Position::Absolute;
        marks.left = 0.0f;
        marks.top = 0.0f;
        marks.width = Length::percent(100);
        marks.height = Length::percent(100);
        marks.overflow = Overflow::Hidden;
        appendSweep(shapes, window.sweep, plotFrame.width, plotFrame.height);
        ui.draw(marks, std::move(shapes));

        if (result.hoveredIndex >= 0 && trigger != ChartTrigger::None) {
            const auto index = static_cast<std::size_t>(result.hoveredIndex);
            const float slot = (flat ? plotFrame.height : plotFrame.width) /
                               static_cast<float>(std::max<std::size_t>(samples, 1));
            const float centre = (static_cast<float>(index) + 0.5f) * slot;
            // A column's slot spans the plot's height, so the panel pins to the
            // top beside it. A horizontal bar's spans the width instead, and
            // there the panel follows the row the pointer is on.
            drawSeriesReadout(ui, plotFrame,
                              flat ? input.pointer().x - plotFrame.x : centre,
                              flat ? std::optional<float>(centre) : std::nullopt,
                              window.first + index,
                              window.wholeSeries ? *window.wholeSeries : series,
                              options.categories, options.valueFormat, options.tooltip, style);
        }
        (void)plotScope;
        (void)topScope;
    }

    // ---- the axis along the bottom ----------------------------------------
    if (bottomHeight > 0.0f && plotFrame.width > 0.0f) {
        Style bottom;
        bottom.direction = Direction::Row;
        bottom.height = bottomHeight;
        bottom.shrink = 0.0f;
        bottom.gap = 6.0f;
        auto bottomScope = ui.begin(bottom);

        if (sideWidth > 0.0f) {
            Style spacer;
            spacer.width = sideWidth;
            spacer.shrink = 0.0f;
            ui.add(spacer);
        }

        Style strip;
        strip.grow = 1.0f;
        strip.basis = 0.0f;
        auto stripScope = ui.begin(strip);
        if (flat) {
            for (const double at : ticks) {
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), std::string(options.valueFormat).c_str(), at);
                Style cell;
                cell.position = Position::Absolute;
                cell.left = scale.fraction(at) * plotFrame.width - 20.0f;
                cell.top = 0.0f;
                cell.width = 40.0f;
                cell.justify = Justify::Center;
                auto cellScope = ui.begin(cell);
                text(ui, buffer,
                     {.color = Token::TextMuted, .size = 10.0f, .align = TextAlign::Center});
                (void)cellScope;
            }
        } else {
            const float slot =
                plotFrame.width / static_cast<float>(std::max<std::size_t>(samples, 1));
            for (std::size_t i = 0; i < samples; ++i) {
                if (categoryLabel(i).empty()) continue;
                // A label may spill into the empty slots on either side of it.
                //
                // Which is what makes a *sparse* axis work: a chart with forty
                // bars and a label every eighth has 21 pixels per slot and a
                // timestamp needs fifty, so every one of them used to ellipsise
                // to "09…". A dense axis has no empty neighbours to borrow and
                // is left exactly as it was.
                std::size_t before = 0;
                while (before < i && categoryLabel(i - before - 1).empty()) ++before;
                std::size_t after = 0;
                while (i + after + 1 < samples && categoryLabel(i + after + 1).empty()) ++after;

                // Centred over its own bar while both sides have room, and
                // pushed off the end it is against when only one does — which
                // is the first and last label of every sparse axis, and the two
                // a reader looks at first.
                const std::size_t both = std::min(before, after);
                float room = slot;
                float left = static_cast<float>(i) * slot;
                Justify justify = Justify::Center;
                TextAlign align = TextAlign::Center;
                if (both > 0) {
                    room = slot * static_cast<float>(both * 2 + 1);
                    left -= (room - slot) / 2.0f;
                } else if (after > 0) {
                    room = slot * static_cast<float>(after + 1);
                    justify = Justify::Start;
                    align = TextAlign::Start;
                } else if (before > 0) {
                    room = slot * static_cast<float>(before + 1);
                    left -= room - slot;
                    justify = Justify::End;
                    align = TextAlign::End;
                }

                Style cell;
                cell.position = Position::Absolute;
                cell.left = left;
                cell.top = 0.0f;
                cell.width = room;
                cell.justify = justify;
                auto cellScope = ui.begin(cell);
                text(ui, categoryLabel(i),
                     {.color = result.hoveredIndex == static_cast<int>(i) ? Token::Text
                                                                         : Token::TextMuted,
                      .size = 10.0f, .align = align,
                      .overflow = TextOverflow::Ellipsis});
                (void)cellScope;
            }
        }
        (void)stripScope;
        (void)bottomScope;
    }

    if (showLegend) drawLegend(ui, input, id, keys, options.legend);

    (void)outerScope;
    return result;
}

}  // namespace

ChartResult barChart(Ui& ui, const Interaction& input, std::string_view id,
                     const std::vector<Series>& series, const BarChartOptions& options) {
    return drawBarChart(ui, input, id, series, options, {});
}

ChartResult barChart(Ui& ui, const Interaction& input, std::string_view id,
                     const std::vector<Series>& series, ChartView& view,
                     const BarChartOptions& options, const ChartZoom& zoom) {
    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);
    view.normalise(zoom.minSpan);
    const Gestures gestures = driveView(input, plotId, plotFrame, view, zoom);

    std::size_t samples = 0;
    for (const Series& one : series) samples = std::max(samples, one.values.size());
    const Window window = windowOf(samples, view);

    std::vector<Series> sliced;
    sliced.reserve(series.size());
    for (const Series& one : series) {
        Series cut = one;
        cut.values.clear();
        for (std::size_t i = window.first; i < window.last && i < one.values.size(); ++i) {
            cut.values.push_back(one.values[i]);
        }
        sliced.push_back(std::move(cut));
    }

    Windowed windowed = windowedFor(input, view, zoom, window);
    windowed.wholeSeries = &series;
    ChartResult result = drawBarChart(ui, input, id, sliced, options, windowed);
    if (result.hoveredIndex >= 0) result.hoveredIndex += static_cast<int>(window.first);
    result.viewChanged = gestures.changed;
    result.wheelTaken = gestures.wheelTaken;
    return result;
}

namespace {

ChartResult drawCandlestickChart(Ui& ui, const Interaction& input, std::string_view id,
                                 const std::vector<Candle>& candles,
                                 const CandlestickOptions& options, const Windowed& window) {
    ChartResult result;
    const ChartStyle& style = ui.design().chart;
    const int tickCount = options.tickCount.value_or(style.tickCount);
    const float axisWidth = options.axisWidth.value_or(style.axisWidth);
    const bool grid = options.grid.value_or(style.grid);

    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);
    const std::size_t samples = candles.size();

    // ---- the scale ---------------------------------------------------------
    // Around the data, not down to zero — see the header for why this one chart
    // breaks that rule. A little padding above and below so the extremes are
    // not drawn on the frame.
    Scale scale = options.scale;
    if (options.autoScale) {
        double low = 0.0;
        double high = 0.0;
        bool seen = false;
        for (const Candle& candle : candles) {
            if (!std::isfinite(candle.top()) || !std::isfinite(candle.bottom())) continue;
            low = seen ? std::min(low, candle.bottom()) : candle.bottom();
            high = seen ? std::max(high, candle.top()) : candle.top();
            seen = true;
        }
        if (!seen) {
            scale = Scale{0.0, 1.0};
        } else {
            const double margin = (high - low) * 0.08;
            scale = Scale::nice(low - margin, high + margin, tickCount);
        }
    }

    const ChartTrigger trigger = options.tooltip.trigger.value_or(style.tooltip.trigger);
    if (options.hover && samples > 0 && !plotFrame.empty() && !window.sweep.active &&
        marksTracked(ui, input, id, plotId, trigger)) {
        const float along = (input.pointer().x - plotFrame.x) / plotFrame.width;
        const auto slot = static_cast<int>(std::floor(std::clamp(along, 0.0f, 0.9999f) *
                                                      static_cast<float>(samples)));
        result.hoveredIndex = std::clamp(slot, 0, static_cast<int>(samples) - 1);
    }
    if (options.hover) {
        const int mine = result.hoveredIndex >= 0
                             ? result.hoveredIndex + static_cast<int>(window.first)
                             : -1;
        const int shared = linkedIndex(options.link, id, mine, window.first + samples);
        const int local = shared >= 0 ? shared - static_cast<int>(window.first) : -1;
        result.hoveredIndex = local >= 0 && local < static_cast<int>(samples) ? local : -1;
    }

    const std::vector<double> ticks = scale.ticks(tickCount);

    Style outer;
    outer.direction = Direction::Column;
    outer.height = options.height;
    outer.gap = 4.0f;
    auto outerScope = ui.begin(outer);
    ui.tag(id);

    {
        Style top;
        top.direction = Direction::Row;
        top.grow = 1.0f;
        top.basis = 0.0f;
        top.gap = 6.0f;
        auto topScope = ui.begin(top);

        if (axisWidth > 0.0f && plotFrame.height > 0.0f) {
            Style axis;
            axis.width = axisWidth;
            axis.shrink = 0.0f;
            auto axisScope = ui.begin(axis);
            for (const double at : ticks) {
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), std::string(options.valueFormat).c_str(), at);
                Style slot;
                slot.position = Position::Absolute;
                slot.left = 0.0f;
                // Clamped inside the plot. A label sits half its height above
                // its own gridline, so the bottom tick's used to hang seven
                // pixels below the plot and land on whatever was under it —
                // which, for a price chart with its volume beneath, was the
                // volume's own top label.
                slot.top = std::clamp((1.0f - scale.fraction(at)) * plotFrame.height - 7.0f,
                                      0.0f, std::max(0.0f, plotFrame.height - 14.0f));
                slot.width = axisWidth;
                slot.justify = Justify::End;
                auto slotScope = ui.begin(slot);
                text(ui, buffer,
                     {.color = Token::TextMuted, .size = 10.0f, .align = TextAlign::End});
                (void)slotScope;
            }
            (void)axisScope;
        }

        std::vector<Shape> shapes;
        const float width = plotFrame.width;
        const float height = plotFrame.height;
        const auto yAt = [&](double value) { return height * (1.0f - scale.fraction(value)); };

        if (width > 0.0f && height > 0.0f && samples > 0) {
            if (grid) {
                for (const double at : ticks) {
                    Path line;
                    const float y = yAt(at);
                    line.moveTo({0.0f, y});
                    line.lineTo({width, y});
                    shapes.push_back({std::move(line), Fill{Token::Border}, 1.0f});
                }
            }

            const float slot = width / static_cast<float>(samples);
            if (result.hoveredIndex >= 0) {
                Path band;
                const float at = static_cast<float>(result.hoveredIndex) * slot;
                band.moveTo({at, 0.0f});
                band.lineTo({at + slot, 0.0f});
                band.lineTo({at + slot, height});
                band.lineTo({at, height});
                band.close();
                shapes.push_back({std::move(band), Fill{Token::SurfaceHover, 0.5f}, 0.0f});
                // The rule down its middle, which is what lines up with the
                // volume chart or the line chart stacked under it.
                appendCrosshair(shapes, at + slot / 2.0f, height);
            }

            float body =
                std::max(1.0f, slot * (1.0f - std::clamp(options.categoryPadding, 0.0f, 0.9f)));
            if (options.maxBodyWidth > 0.0f) body = std::min(body, options.maxBodyWidth);

            for (std::size_t i = 0; i < samples; ++i) {
                const Candle& candle = candles[i];
                if (!std::isfinite(candle.top())) continue;
                const bool up = candle.rising();
                const Token colour =
                    up ? options.rising.value_or(Token::Added)
                       : options.falling.value_or(Token::Removed);
                const float centre = (static_cast<float>(i) + 0.5f) * slot;

                // The wick first, so the body covers the part of it that runs
                // behind — which is what makes a hollow body look right.
                Path wick;
                wick.moveTo({centre, yAt(candle.top())});
                wick.lineTo({centre, yAt(candle.bottom())});
                shapes.push_back({std::move(wick), Fill{colour}, options.lineWidth});

                const float openY = yAt(candle.open);
                const float closeY = yAt(candle.close);
                // A period that opened and closed at the same price still has a
                // body: a doji is a line, not nothing.
                const float bodyTop = std::min(openY, closeY);
                const float bodyHeight = std::max(options.lineWidth, std::abs(closeY - openY));
                const Rect box{centre - body / 2.0f, bodyTop, body, bodyHeight};

                const bool hollow = options.hollowRising && up;
                shapes.push_back({barPath(box, 0.0f, BarSide::Top),
                                  Fill{colour, hollow ? 0.0f : 1.0f},
                                  hollow ? options.lineWidth : 0.0f});
            }
        }

        Style plot;
        plot.grow = 1.0f;
        plot.basis = 0.0f;
        if (window.sweep.offered) plot.cursorHint = Cursor::Crosshair;
        if (window.takesWheel) plot.overflow = Overflow::Scroll;
        auto plotScope = ui.begin(plot);
        ui.tag(plotId);

        Style marks;
        marks.position = Position::Absolute;
        marks.left = 0.0f;
        marks.top = 0.0f;
        marks.width = Length::percent(100);
        marks.height = Length::percent(100);
        marks.overflow = Overflow::Hidden;
        appendSweep(shapes, window.sweep, plotFrame.width, plotFrame.height);
        ui.draw(marks, std::move(shapes));

        if (result.hoveredIndex >= 0 && trigger != ChartTrigger::None) {
            const auto index = static_cast<std::size_t>(result.hoveredIndex);
            const std::vector<Candle>& whole =
                window.wholeCandles ? *window.wholeCandles : candles;
            TooltipContext context;
            context.index = window.first + index;
            context.candles = &whole;

            std::vector<TooltipRow> rows;
            if (options.tooltip.rows) {
                rows = options.tooltip.rows(context);
            } else {
                // The four numbers the period is, in the order the name says
                // them. No swatches: the colour is the candle's direction, and
                // repeating it four times down the panel says nothing.
                const Candle& candle = whole[context.index];
                const std::pair<const char*, double> lines[] = {{"open", candle.open},
                                                                {"high", candle.high},
                                                                {"low", candle.low},
                                                                {"close", candle.close}};
                for (std::size_t line = 0; line < std::size(lines); ++line) {
                    char printed[48];
                    std::snprintf(printed, sizeof(printed),
                                  std::string(options.valueFormat).c_str(), lines[line].second);
                    // Only the close carries a swatch, and it carries the one
                    // the candle is drawn in: the direction is the reason the
                    // reader is looking, and saying it four times says nothing.
                    const std::optional<Token> swatch =
                        line + 1 == std::size(lines)
                            ? std::optional<Token>(candle.rising()
                                                       ? options.rising.value_or(Token::Added)
                                                       : options.falling.value_or(Token::Removed))
                            : std::nullopt;
                    rows.push_back({swatch, lines[line].first, printed});
                }
            }
            const float slot = plotFrame.width / static_cast<float>(samples);
            drawReadoutPanel(ui, plotFrame, (static_cast<float>(index) + 0.5f) * slot,
                             std::nullopt, headingFor(context, options.tooltip, options.categories),
                             rows, options.tooltip, style.tooltip, context);
        }
        (void)plotScope;
        (void)topScope;
    }

    if (options.categoryAxis > 0.0f && plotFrame.width > 0.0f && samples > 0) {
        Style bottom;
        bottom.direction = Direction::Row;
        bottom.height = options.categoryAxis;
        bottom.shrink = 0.0f;
        bottom.gap = 6.0f;
        auto bottomScope = ui.begin(bottom);
        if (axisWidth > 0.0f) {
            Style spacer;
            spacer.width = axisWidth;
            spacer.shrink = 0.0f;
            ui.add(spacer);
        }
        Style strip;
        strip.grow = 1.0f;
        strip.basis = 0.0f;
        auto stripScope = ui.begin(strip);
        const float slot = plotFrame.width / static_cast<float>(samples);
        for (std::size_t i = 0; i < samples; ++i) {
            // The caller's categories, at the caller's index: `candles` may be
            // a window over their data.
            const std::size_t at = window.first + i;
            if (at >= options.categories.size() || options.categories[at].empty()) continue;
            Style cell;
            cell.position = Position::Absolute;
            cell.left = static_cast<float>(i) * slot;
            cell.top = 0.0f;
            cell.width = slot;
            cell.justify = Justify::Center;
            auto cellScope = ui.begin(cell);
            text(ui, options.categories[at],
                 {.color = result.hoveredIndex == static_cast<int>(i) ? Token::Text
                                                                      : Token::TextMuted,
                  .size = 10.0f, .align = TextAlign::Center,
                  .overflow = TextOverflow::Ellipsis});
            (void)cellScope;
        }
        (void)stripScope;
        (void)bottomScope;
    }

    (void)outerScope;
    return result;
}

}  // namespace

ChartResult candlestickChart(Ui& ui, const Interaction& input, std::string_view id,
                             const std::vector<Candle>& candles,
                             const CandlestickOptions& options) {
    return drawCandlestickChart(ui, input, id, candles, options, {});
}

ChartResult candlestickChart(Ui& ui, const Interaction& input, std::string_view id,
                             const std::vector<Candle>& candles, ChartView& view,
                             const CandlestickOptions& options, const ChartZoom& zoom) {
    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);
    view.normalise(zoom.minSpan);
    const Gestures gestures = driveView(input, plotId, plotFrame, view, zoom);

    const Window window = windowOf(candles.size(), view);
    std::vector<Candle> sliced;
    sliced.reserve(window.count());
    for (std::size_t i = window.first; i < window.last && i < candles.size(); ++i) {
        sliced.push_back(candles[i]);
    }

    Windowed windowed = windowedFor(input, view, zoom, window);
    windowed.wholeCandles = &candles;
    ChartResult result = drawCandlestickChart(ui, input, id, sliced, options, windowed);
    if (result.hoveredIndex >= 0) result.hoveredIndex += static_cast<int>(window.first);
    result.viewChanged = gestures.changed;
    result.wheelTaken = gestures.wheelTaken;
    return result;
}

ScatterResult scatterChart(Ui& ui, const Interaction& input, std::string_view id,
                           const std::vector<PointSeries>& series,
                           const ScatterOptions& options) {
    ScatterResult result;
    const ChartStyle& style = ui.design().chart;
    const int tickCount = options.tickCount.value_or(style.tickCount);
    const float axisWidth = options.axisWidth.value_or(style.axisWidth);
    const bool grid = options.grid.value_or(style.grid);

    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);
    const ChartTrigger trigger = options.tooltip.trigger.value_or(style.tooltip.trigger);

    // ---- the scales --------------------------------------------------------
    // Both from the data, and neither forced to zero: a correlation between two
    // measured quantities has no baseline, and dragging one in would push every
    // point into a corner.
    Scale xScale = options.xScale;
    Scale yScale = options.yScale;
    double heaviest = 0.0;
    if (options.autoScale) {
        double xLow = 0.0;
        double xHigh = 0.0;
        double yLow = 0.0;
        double yHigh = 0.0;
        bool seen = false;
        for (const PointSeries& one : series) {
            for (const Point& point : one.points) {
                if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
                xLow = seen ? std::min(xLow, point.x) : point.x;
                xHigh = seen ? std::max(xHigh, point.x) : point.x;
                yLow = seen ? std::min(yLow, point.y) : point.y;
                yHigh = seen ? std::max(yHigh, point.y) : point.y;
                seen = true;
            }
        }
        xScale = seen ? Scale::nice(xLow, xHigh, tickCount) : Scale{0.0, 1.0};
        yScale = seen ? Scale::nice(yLow, yHigh, tickCount) : Scale{0.0, 1.0};
    }
    for (const PointSeries& one : series) {
        for (const Point& point : one.points) {
            if (std::isfinite(point.weight)) heaviest = std::max(heaviest, point.weight);
        }
    }

    const std::vector<double> yTicks = yScale.ticks(tickCount);
    const std::vector<double> xTicks = xScale.ticks(tickCount);

    std::vector<LegendEntry> keys;
    for (std::size_t s = 0; s < series.size(); ++s) {
        keys.push_back({series[s].name,
                        series[s].color.value_or(style.palette.empty()
                                                     ? Token::Accent
                                                     : style.palette[s % style.palette.size()])});
    }
    const bool showLegend = options.legend.show && !series.empty();
    const int focused = options.legend.focused ? *options.legend.focused : -1;

    Style outer;
    outer.direction = Direction::Column;
    outer.height = options.height + (showLegend ? options.legend.height : 0.0f);
    outer.gap = 4.0f;
    auto outerScope = ui.begin(outer);
    ui.tag(id);

    {
        Style top;
        top.direction = Direction::Row;
        top.grow = 1.0f;
        top.basis = 0.0f;
        top.gap = 6.0f;
        auto topScope = ui.begin(top);

        if (axisWidth > 0.0f && plotFrame.height > 0.0f) {
            Style axis;
            axis.width = axisWidth;
            axis.shrink = 0.0f;
            auto axisScope = ui.begin(axis);
            for (const double at : yTicks) {
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), std::string(options.valueFormat).c_str(), at);
                Style slot;
                slot.position = Position::Absolute;
                slot.left = 0.0f;
                slot.top = std::clamp((1.0f - yScale.fraction(at)) * plotFrame.height - 7.0f,
                                      0.0f, std::max(0.0f, plotFrame.height - 14.0f));
                slot.width = axisWidth;
                slot.justify = Justify::End;
                auto slotScope = ui.begin(slot);
                text(ui, buffer,
                     {.color = Token::TextMuted, .size = 10.0f, .align = TextAlign::End});
                (void)slotScope;
            }
            (void)axisScope;
        }

        std::vector<Shape> shapes;
        const float width = plotFrame.width;
        const float height = plotFrame.height;
        const auto xAt = [&](double v) { return width * xScale.fraction(v); };
        const auto yAt = [&](double v) { return height * (1.0f - yScale.fraction(v)); };

        // ---- what the pointer is over --------------------------------------
        // Nearest dot, not nearest column: a scatter has no columns, and two
        // points may share an x while meaning different things.
        if (options.hover && !plotFrame.empty() && width > 0.0f &&
            marksTracked(ui, input, id, plotId, trigger)) {
            const Vec2 at{input.pointer().x - plotFrame.x, input.pointer().y - plotFrame.y};
            float nearest = options.hitRadius * options.hitRadius;
            for (std::size_t s = 0; s < series.size(); ++s) {
                for (std::size_t i = 0; i < series[s].points.size(); ++i) {
                    const Point& point = series[s].points[i];
                    if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
                    const float dx = xAt(point.x) - at.x;
                    const float dy = yAt(point.y) - at.y;
                    const float distance = dx * dx + dy * dy;
                    if (distance > nearest) continue;
                    nearest = distance;
                    result.hoveredSeries = static_cast<int>(s);
                    result.hoveredIndex = static_cast<int>(i);
                }
            }
        }

        if (width > 0.0f && height > 0.0f) {
            if (grid) {
                for (const double at : yTicks) {
                    Path line;
                    line.moveTo({0.0f, yAt(at)});
                    line.lineTo({width, yAt(at)});
                    shapes.push_back({std::move(line), Fill{Token::Border}, 1.0f});
                }
                // Vertical gridlines too, which the indexed charts do not have
                // and this one needs: without them an x value cannot be read
                // off the plot at all.
                for (const double at : xTicks) {
                    Path line;
                    line.moveTo({xAt(at), 0.0f});
                    line.lineTo({xAt(at), height});
                    shapes.push_back({std::move(line), Fill{Token::Border, 0.6f}, 1.0f});
                }
            }

            for (std::size_t s = 0; s < series.size(); ++s) {
                const PointSeries& one = series[s];
                const Token colour = one.color.value_or(
                    style.palette.empty() ? Token::Accent
                                          : style.palette[s % style.palette.size()]);
                for (std::size_t i = 0; i < one.points.size(); ++i) {
                    const Point& point = one.points[i];
                    if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
                    const Vec2 centre{xAt(point.x), yAt(point.y)};

                    float radius = one.radius;
                    if (heaviest > 0.0 && point.weight > 0.0) {
                        // Area-proportional: the square root is the whole point.
                        const auto share = static_cast<float>(std::sqrt(point.weight / heaviest));
                        radius = options.minRadius +
                                 share * (options.maxRadius - options.minRadius);
                    }

                    const bool picked = result.hoveredSeries == static_cast<int>(s) &&
                                        result.hoveredIndex == static_cast<int>(i);
                    const bool dimmed = (result.hoveredIndex >= 0 && !picked) ||
                                        (focused >= 0 && focused != static_cast<int>(s));

                    shapes.push_back({circlePath(centre, radius),
                                      Fill{colour, options.fillAlpha * (dimmed ? 0.4f : 1.0f)},
                                      0.0f});
                    // An outline, so overlapping bubbles stay countable — a
                    // pile of translucent discs with no edges reads as one
                    // blob.
                    shapes.push_back({circlePath(centre, radius),
                                      Fill{colour, dimmed ? 0.5f : 1.0f},
                                      picked ? 2.0f : 1.0f});
                }
            }
        }

        Style plot;
        plot.grow = 1.0f;
        plot.basis = 0.0f;
        auto plotScope = ui.begin(plot);
        ui.tag(plotId);

        Style marks;
        marks.position = Position::Absolute;
        marks.left = 0.0f;
        marks.top = 0.0f;
        marks.width = Length::percent(100);
        marks.height = Length::percent(100);
        marks.overflow = Overflow::Hidden;
        ui.draw(marks, std::move(shapes));

        // ---- the readout ---------------------------------------------------
        // Beside the dot rather than pinned to the top: a point is a place on
        // both axes, and a panel at the top of the plot would leave the reader
        // matching a number to one of forty circles by eye.
        if (result.hoveredSeries >= 0 && result.hoveredIndex >= 0 &&
            trigger != ChartTrigger::None) {
            const auto s = static_cast<std::size_t>(result.hoveredSeries);
            const auto i = static_cast<std::size_t>(result.hoveredIndex);
            const PointSeries& one = series[s];
            const Point& point = one.points[i];
            const Token colour = one.color.value_or(
                style.palette.empty() ? Token::Accent
                                      : style.palette[s % style.palette.size()]);

            TooltipContext context;
            context.index = i;
            context.seriesIndex = s;
            context.points = &series;

            std::vector<TooltipRow> rows;
            if (options.tooltip.rows) {
                rows = options.tooltip.rows(context);
            } else {
                // Both coordinates, because on this chart neither is implied by
                // where the pointer is — and the weight when there is one,
                // which is the whole reason a bubble is the size it is.
                char printed[48];
                std::snprintf(printed, sizeof(printed), std::string(options.xFormat).c_str(),
                              point.x);
                rows.push_back({colour, "x", printed});
                std::snprintf(printed, sizeof(printed), std::string(options.valueFormat).c_str(),
                              point.y);
                rows.push_back({std::nullopt, "y", printed});
                if (heaviest > 0.0 && point.weight > 0.0) {
                    std::snprintf(printed, sizeof(printed),
                                  std::string(options.valueFormat).c_str(), point.weight);
                    rows.push_back({std::nullopt, "weight", printed});
                }
            }

            // The heading names the *series*: a dot has no category, and "#3"
            // says less than "Enterprise" about which cloud it came from.
            std::string heading = options.tooltip.title ? options.tooltip.title(context)
                                                        : std::string(one.name);
            drawReadoutPanel(ui, plotFrame, xAt(point.x), yAt(point.y), heading, rows,
                             options.tooltip, style.tooltip, context);
        }
        (void)plotScope;
        (void)topScope;
    }

    // ---- the x axis --------------------------------------------------------
    if (options.xAxis > 0.0f && plotFrame.width > 0.0f) {
        Style bottom;
        bottom.direction = Direction::Row;
        bottom.height = options.xAxis;
        bottom.shrink = 0.0f;
        bottom.gap = 6.0f;
        auto bottomScope = ui.begin(bottom);
        if (axisWidth > 0.0f) {
            Style spacer;
            spacer.width = axisWidth;
            spacer.shrink = 0.0f;
            ui.add(spacer);
        }
        Style strip;
        strip.grow = 1.0f;
        strip.basis = 0.0f;
        auto stripScope = ui.begin(strip);
        for (const double at : xTicks) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), std::string(options.xFormat).c_str(), at);
            Style cell;
            cell.position = Position::Absolute;
            cell.left = xScale.fraction(at) * plotFrame.width - 24.0f;
            cell.top = 0.0f;
            cell.width = 48.0f;
            cell.justify = Justify::Center;
            auto cellScope = ui.begin(cell);
            text(ui, buffer,
                 {.color = Token::TextMuted, .size = 10.0f, .align = TextAlign::Center});
            (void)cellScope;
        }
        (void)stripScope;
        (void)bottomScope;
    }

    if (showLegend) drawLegend(ui, input, id, keys, options.legend);

    (void)outerScope;
    return result;
}

HeatmapResult heatmap(Ui& ui, const Interaction& input, std::string_view id,
                      const std::vector<std::vector<double>>& values,
                      const HeatmapOptions& options) {
    HeatmapResult result;
    const ChartStyle& style = ui.design().chart;
    const ChartTrigger trigger = options.tooltip.trigger.value_or(style.tooltip.trigger);
    const Token hue = options.color.value_or(
        style.palette.empty() ? Token::Accent : style.palette.front());

    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);

    const std::size_t rowCount = values.size();
    std::size_t columnCount = 0;
    for (const std::vector<double>& row : values) columnCount = std::max(columnCount, row.size());
    if (rowCount == 0 || columnCount == 0) return result;

    // ---- the scale ---------------------------------------------------------
    // From zero, unlike the scatter: a heatmap's colour is read as "how much",
    // and a shade that means 40 in one grid and 4 in another is unreadable
    // without a baseline everyone agrees on.
    Scale scale = options.scale;
    if (options.autoScale) {
        double high = 0.0;
        for (const std::vector<double>& row : values) {
            for (const double value : row) {
                if (std::isfinite(value)) high = std::max(high, value);
            }
        }
        scale = Scale{0.0, high > 0.0 ? high : 1.0};
    }

    const auto shadeOf = [&](double value) {
        float f = std::clamp(scale.fraction(value), 0.0f, 1.0f);
        if (options.steps > 0) {
            const auto steps = static_cast<float>(options.steps);
            // Rounded *up*, so any non-zero value gets at least the first step:
            // a cell with one commit in it must not look like a cell with none.
            f = std::min(1.0f, std::ceil(f * steps) / steps);
        }
        // Never fully transparent when there is something there, and never
        // fully opaque at the bottom of the range.
        return f <= 0.0f ? 0.0f : 0.14f + f * 0.86f;
    };

    Style outer;
    outer.direction = Direction::Column;
    outer.gap = 4.0f;
    auto outerScope = ui.begin(outer);
    ui.tag(id);

    const float cell = options.cellSize > 0.0f
                           ? options.cellSize
                           : (plotFrame.width > 0.0f
                                  ? (plotFrame.width + options.gap) /
                                        static_cast<float>(columnCount) - options.gap
                                  : 0.0f);
    const float pitch = cell + options.gap;

    // ---- the column labels -------------------------------------------------
    if (options.columnLabels > 0.0f && plotFrame.width > 0.0f) {
        Style top;
        top.direction = Direction::Row;
        top.height = options.columnLabels;
        top.shrink = 0.0f;
        top.gap = 6.0f;
        auto topScope = ui.begin(top);
        if (options.rowLabels > 0.0f) {
            Style spacer;
            spacer.width = options.rowLabels;
            spacer.shrink = 0.0f;
            ui.add(spacer);
        }
        Style strip;
        strip.grow = 1.0f;
        strip.basis = 0.0f;
        auto stripScope = ui.begin(strip);
        for (std::size_t c = 0; c < columnCount && c < options.columns.size(); ++c) {
            if (options.columns[c].empty()) continue;
            Style slot;
            slot.position = Position::Absolute;
            slot.left = static_cast<float>(c) * pitch;
            slot.top = 0.0f;
            slot.width = std::max(cell, 24.0f);
            auto slotScope = ui.begin(slot);
            text(ui, options.columns[c],
                 {.color = Token::TextMuted, .size = 10.0f,
                  .overflow = TextOverflow::Ellipsis});
            (void)slotScope;
        }
        (void)stripScope;
        (void)topScope;
    }

    // ---- the grid ----------------------------------------------------------
    {
        Style body;
        body.direction = Direction::Row;
        body.gap = 6.0f;
        auto bodyScope = ui.begin(body);

        if (options.rowLabels > 0.0f) {
            Style side;
            side.width = options.rowLabels;
            side.shrink = 0.0f;
            auto sideScope = ui.begin(side);
            for (std::size_t r = 0; r < rowCount && r < options.rows.size(); ++r) {
                if (options.rows[r].empty()) continue;
                Style slot;
                slot.position = Position::Absolute;
                slot.left = 0.0f;
                slot.top = static_cast<float>(r) * pitch + std::max(0.0f, cell / 2.0f - 7.0f);
                slot.width = options.rowLabels;
                slot.justify = Justify::End;
                auto slotScope = ui.begin(slot);
                text(ui, options.rows[r],
                     {.color = Token::TextMuted, .size = 10.0f, .align = TextAlign::End,
                      .overflow = TextOverflow::Ellipsis});
                (void)slotScope;
            }
            (void)sideScope;
        }

        // What the pointer is over, in grid coordinates.
        if (options.hover && !plotFrame.empty() && pitch > 0.0f &&
            marksTracked(ui, input, id, plotId, trigger)) {
            const float x = input.pointer().x - plotFrame.x;
            const float y = input.pointer().y - plotFrame.y;
            const auto c = static_cast<int>(std::floor(x / pitch));
            const auto r = static_cast<int>(std::floor(y / pitch));
            if (r >= 0 && c >= 0 && static_cast<std::size_t>(r) < rowCount &&
                static_cast<std::size_t>(c) < values[static_cast<std::size_t>(r)].size()) {
                result.hoveredRow = r;
                result.hoveredColumn = c;
                result.hoveredValue = values[static_cast<std::size_t>(r)]
                                            [static_cast<std::size_t>(c)];
            }
        }

        std::vector<Shape> shapes;
        if (cell > 0.0f) {
            for (std::size_t r = 0; r < rowCount; ++r) {
                for (std::size_t c = 0; c < values[r].size(); ++c) {
                    const double value = values[r][c];
                    const Rect box{static_cast<float>(c) * pitch, static_cast<float>(r) * pitch,
                                   cell, cell};
                    const bool picked = result.hoveredRow == static_cast<int>(r) &&
                                        result.hoveredColumn == static_cast<int>(c);

                    // The empty slot underneath every cell, so a grid with gaps
                    // still reads as a grid rather than as scattered marks.
                    // `Border` rather than `BgElevated`: an elevated surface is
                    // a shade away from the panel behind it, which is right for
                    // a raised box and useless for a cell that has to be seen
                    // as present-but-zero.
                    shapes.push_back({barPath(box, options.radius, BarSide::Top),
                                      Fill{Token::Border, 0.55f}, 0.0f});
                    const float shade = std::isfinite(value) ? shadeOf(value) : 0.0f;
                    if (shade > 0.0f) {
                        shapes.push_back({barPath(box, options.radius, BarSide::Top),
                                          Fill{hue, shade}, 0.0f});
                    }
                    if (picked) {
                        shapes.push_back({barPath(box, options.radius, BarSide::Top),
                                          Fill{Token::TextStrong}, 1.5f});
                    }
                }
            }
        }

        Style plot;
        plot.grow = 1.0f;
        plot.basis = 0.0f;
        plot.height = static_cast<float>(rowCount) * pitch - options.gap;
        auto plotScope = ui.begin(plot);
        ui.tag(plotId);

        Style marks;
        marks.position = Position::Absolute;
        marks.left = 0.0f;
        marks.top = 0.0f;
        marks.width = Length::percent(100);
        marks.height = Length::percent(100);
        ui.draw(marks, std::move(shapes));

        // ---- the readout ---------------------------------------------------
        // A grid's labels are along its edges, and on any grid worth drawing
        // they have been ellipsised to fit. The readout is where a cell's row,
        // column and value can actually be read.
        if (result.hoveredRow >= 0 && result.hoveredColumn >= 0 &&
            trigger != ChartTrigger::None) {
            const auto r = static_cast<std::size_t>(result.hoveredRow);
            const auto c = static_cast<std::size_t>(result.hoveredColumn);

            TooltipContext context;
            context.index = c;
            context.seriesIndex = r;
            context.cells = &values;

            std::vector<TooltipRow> rows;
            if (options.tooltip.rows) {
                rows = options.tooltip.rows(context);
            } else {
                char printed[48];
                std::snprintf(printed, sizeof(printed), std::string(options.valueFormat).c_str(),
                              result.hoveredValue);
                rows.push_back({hue,
                                r < options.rows.size() ? options.rows[r] : "row " +
                                                                                std::to_string(r),
                                printed});
            }

            const std::string heading =
                options.tooltip.title ? options.tooltip.title(context)
                                      : (c < options.columns.size() ? options.columns[c]
                                                                    : "#" + std::to_string(c));
            drawReadoutPanel(ui, plotFrame, (static_cast<float>(c) + 0.5f) * pitch,
                             (static_cast<float>(r) + 0.5f) * pitch, heading, rows,
                             options.tooltip, style.tooltip, context);
        }
        (void)plotScope;
        (void)bodyScope;
    }

    (void)outerScope;
    return result;
}

DonutResult donutChart(Ui& ui, const Interaction& input, std::string_view id,
                       const std::vector<Slice>& slices, DonutState& state,
                       const DonutOptions& options) {
    DonutResult result;
    const ChartStyle& style = ui.design().chart;
    const float ringThickness = options.thickness.value_or(style.donutThickness);
    const float padAngle = options.padAngle.value_or(style.donutPadAngle);
    const auto sliceColour = [&](std::size_t index) {
        if (slices[index].color) return *slices[index].color;
        return style.palette.empty() ? Token::Accent : style.palette[index % style.palette.size()];
    };

    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);
    const ChartTrigger trigger = options.tooltip.trigger.value_or(style.tooltip.trigger);

    double total = 0.0;
    for (const Slice& slice : slices) total += std::max(0.0, slice.value);

    // ---- what the pointer is over ------------------------------------------
    // By angle and radius, not by rectangle: a wedge is not a box, and hit
    // testing it as one is what makes a chart's tooltip name the wrong thing.
    if (total > 0.0 && !plotFrame.empty() && marksTracked(ui, input, id, plotId, trigger)) {
        const Vec2 centre{plotFrame.x + plotFrame.width / 2.0f,
                          plotFrame.y + plotFrame.height / 2.0f};
        const float outer = std::min(plotFrame.width, plotFrame.height) / 2.0f;
        const float inner = outer * (1.0f - ringThickness);
        const Vec2 from{input.pointer().x - centre.x, input.pointer().y - centre.y};
        const float distance = std::hypot(from.x, from.y);
        if (distance <= outer && distance >= inner) {
            float angle = std::atan2(from.y, from.x) * 180.0f / 3.14159265f;
            while (angle < options.startAngle) angle += 360.0f;
            double swept = 0.0;
            for (std::size_t i = 0; i < slices.size(); ++i) {
                const double share = std::max(0.0, slices[i].value) / total;
                const float start = options.startAngle + static_cast<float>(swept) * 360.0f;
                const float end = start + static_cast<float>(share) * 360.0f;
                if (angle >= start && angle < end) {
                    result.hoveredIndex = static_cast<int>(i);
                    break;
                }
                swept += share;
            }
        }
    }

    Style row;
    row.direction = Direction::Row;
    row.align = Align::Center;
    row.gap = 16.0f;
    auto scope = ui.begin(row);
    ui.tag(id);

    std::vector<Shape> shapes;
    if (!plotFrame.empty() && total > 0.0) {
        const Vec2 centre{plotFrame.width / 2.0f, plotFrame.height / 2.0f};
        const float radius = std::min(plotFrame.width, plotFrame.height) / 2.0f;
        double swept = 0.0;
        for (std::size_t i = 0; i < slices.size(); ++i) {
            const double share = std::max(0.0, slices[i].value) / total;
            if (share <= 0.0) continue;
            // Hover is a hint and focus is a decision, so they pull the wedge
            // out by different amounts — and both travel rather than jumping,
            // because a wedge that teleports outward reads as a glitch.
            const std::string sliceId = std::string(id) + ".slice." + std::to_string(i);
            const bool lit = result.hoveredIndex == static_cast<int>(i);
            const bool picked = state.focused == static_cast<int>(i);
            const float hover = ui.animate(sliceId, "hover", lit ? 1.0f : 0.0f,
                                           {.duration = 0.12f, .easing = Easing::EaseOut});
            const float focus = ui.animate(sliceId, "focus", picked ? 1.0f : 0.0f,
                                           {.duration = 0.2f, .easing = Easing::BackOut});
            // Everything else steps back so the chosen wedge is the only thing
            // at full strength.
            const float alpha =
                ui.animate(sliceId, "alpha",
                           state.focused < 0 || picked ? 1.0f : options.dimAlpha,
                           {.duration = 0.2f, .easing = Easing::EaseOut});

            const float reach = options.hoverGrow + options.focusGrow;
            const float outer =
                radius - reach + options.hoverGrow * hover + options.focusGrow * focus;
            const float inner = outer * (1.0f - ringThickness);

            const float pad = slices.size() > 1 ? padAngle / 2.0f : 0.0f;
            const float start = options.startAngle + static_cast<float>(swept) * 360.0f + pad;
            const float end =
                options.startAngle + static_cast<float>(swept + share) * 360.0f - pad;
            swept += share;
            if (end <= start) continue;

            // Out along the start edge, round the outside, back down the end
            // edge, and round the inside the other way.
            Path wedge;
            wedge.moveTo(onCircle(centre, inner, start));
            wedge.lineTo(onCircle(centre, outer, start));
            arcTo(wedge, centre, outer, start, end);
            wedge.lineTo(onCircle(centre, inner, end));
            arcTo(wedge, centre, inner, end, start);
            wedge.close();
            shapes.push_back({std::move(wedge), Fill{sliceColour(i), alpha}, 0.0f});
        }
    }

    Style plot;
    plot.width = options.size;
    plot.height = options.size;
    plot.shrink = 0.0f;
    plot.cursorHint = result.hoveredIndex >= 0 ? Cursor::Pointer : Cursor::Default;
    auto plotScope = ui.begin(plot);
    ui.tag(plotId).cursor(plot.cursorHint);

    Style marks;
    marks.position = Position::Absolute;
    marks.left = 0.0f;
    marks.top = 0.0f;
    marks.width = Length::percent(100);
    marks.height = Length::percent(100);
    ui.draw(marks, std::move(shapes));

    // ---- the readout -------------------------------------------------------
    // The legend already names every wedge; what it has no room for is the
    // number behind the share, which is the thing a reader hovers a donut to
    // find out. Placed at the pointer, since a wedge has no single point.
    if (result.hoveredIndex >= 0 && trigger != ChartTrigger::None) {
        const auto index = static_cast<std::size_t>(result.hoveredIndex);
        TooltipContext context;
        context.index = index;
        context.slices = &slices;

        std::vector<TooltipRow> rows;
        if (options.tooltip.rows) {
            rows = options.tooltip.rows(context);
        } else {
            char value[32];
            std::snprintf(value, sizeof(value), "%.0f", slices[index].value);
            rows.push_back({sliceColour(index), "value", value});
            char share[16];
            std::snprintf(share, sizeof(share), "%.1f%%",
                          total > 0.0 ? slices[index].value / total * 100.0 : 0.0);
            rows.push_back({std::nullopt, "share", share});
        }

        const std::string heading = options.tooltip.title
                                        ? options.tooltip.title(context)
                                        : std::string(slices[index].name);
        drawReadoutPanel(ui, plotFrame, input.pointer().x - plotFrame.x,
                         input.pointer().y - plotFrame.y, heading, rows, options.tooltip,
                         style.tooltip, context);
    }
    plotScope.close();

    // Clicking a wedge singles it out; clicking it again, or clicking the hole,
    // puts everything back.
    if (input.clicked(plotId)) {
        state.focused = result.hoveredIndex >= 0 && state.focused != result.hoveredIndex
                            ? result.hoveredIndex
                            : -1;
        result.focusChanged = true;
    }

    if (options.legend) {
        // Bounded and scrolling: forty contributors should not make the legend
        // taller than the ring it belongs to.
        ScrollOptions keys;
        keys.axis = ScrollAxis::Vertical;
        keys.gap = 4.0f;
        // Takes the room left beside the ring: a view that scrolls vertically
        // has its content's width pinned to its own, so it cannot work that
        // width out from what is inside it — it has to be given one.
        keys.grow = 1.0f;
        keys.focusable = false;
        keys.scrollbarWidth = 6.0f;
        // Told, not inferred. A vertical scroll cannot work its own height out
        // from its content — the content is out of the flow — and whether an
        // auto height means "fill" or "fit" depends on the parent's direction,
        // which the view cannot see. The donut *can* count its entries, so it
        // says: as tall as the list, up to the ceiling.
        constexpr float kEntryHeight = 24.0f;
        const float ceiling = isAuto(options.legendMaxHeight) ? options.size
                                                              : options.legendMaxHeight;
        keys.height = std::min(static_cast<float>(slices.size()) * kEntryHeight, ceiling);
        auto keysScope = beginScroll(ui, input, std::string(id) + ".legend", state.legend, keys);
        for (std::size_t i = 0; i < slices.size(); ++i) {
            const std::string entryId = std::string(id) + ".key." + std::to_string(i);
            const bool picked = state.focused == static_cast<int>(i);
            Style entry;
            entry.direction = Direction::Row;
            entry.align = Align::Center;
            entry.gap = 8.0f;
            entry.minHeight = 20.0f;
            entry.shrink = 0.0f;
            entry.padding = Edges::symmetric(0.0f, 4.0f);
            entry.radius = 4.0f;
            if (picked) entry.background = Fill{Token::Accent, 0.18f};
            else if (input.isHovered(entryId)) entry.background = Fill{Token::SurfaceHover};
            // The legend dims with the chart, so the two say the same thing.
            entry.opacity = state.focused < 0 || picked ? 1.0f : 0.5f;
            entry.cursorHint = Cursor::Pointer;
            auto entryScope = ui.begin(entry);
            ui.tag(entryId).cursor(Cursor::Pointer);
            if (input.clicked(entryId)) {
                state.focused = picked ? -1 : static_cast<int>(i);
                result.focusChanged = true;
            }

            Style swatch;
            swatch.width = 10.0f;
            swatch.height = 10.0f;
            swatch.shrink = 0.0f;
            swatch.radius = 2.0f;
            swatch.background = Fill{sliceColour(i)};
            ui.add(swatch);

            char share[16];
            std::snprintf(share, sizeof(share), "%.0f%%",
                          total > 0.0 ? slices[i].value / total * 100.0 : 0.0);
            text(ui, slices[i].name, {.color = Token::Text, .size = 11.0f});
            text(ui, share, {.color = Token::TextMuted, .size = 11.0f});
            (void)entryScope;
        }
        (void)keysScope;
    }
    (void)scope;

    return result;
}

ChartResult radarChart(Ui& ui, const Interaction& input, std::string_view id,
                       const std::vector<Series>& series, const RadarOptions& options) {
    ChartResult result;
    const ChartStyle& style = ui.design().chart;
    const int tickCount = options.tickCount.value_or(style.tickCount);
    const ChartTrigger trigger = options.tooltip.trigger.value_or(style.tooltip.trigger);

    const std::string plotId = std::string(id) + ".plot";
    const Rect plotFrame = input.frameOf(plotId);

    std::size_t axes = 0;
    for (const Series& one : series) axes = std::max(axes, one.values.size());

    // ---- the scale ---------------------------------------------------------
    // Always from zero, whatever the data says: a radar is read as an area, and
    // an area measured from a floating baseline is the same lie a truncated bar
    // tells — told about every axis at once.
    Scale scale = options.scale;
    if (options.autoScale) {
        double high = 0.0;
        for (const Series& one : series) {
            for (const double value : one.values) {
                if (std::isfinite(value)) high = std::max(high, value);
            }
        }
        scale = high > 0.0 ? Scale::nice(0.0, high, tickCount) : Scale{0.0, 1.0};
    }

    // Twelve o'clock, going clockwise — where every reader starts, and the
    // direction they read a clock, a compass and a pie in.
    const auto angleOf = [&](std::size_t axis) {
        return -90.0f + 360.0f * static_cast<float>(axis) / static_cast<float>(axes);
    };

    const Vec2 centre{plotFrame.width / 2.0f, plotFrame.height / 2.0f};
    const float outerRadius =
        std::max(0.0f, std::min(plotFrame.width, plotFrame.height) / 2.0f - options.labelRoom);
    const auto vertexAt = [&](std::size_t axis, double value) {
        return onCircle(centre, outerRadius * std::clamp(scale.fraction(value), 0.0f, 1.0f),
                        angleOf(axis));
    };

    // ---- what the pointer is over ------------------------------------------
    // By angle, not by distance to a vertex: a reader pointing anywhere along a
    // spoke means that axis, which is the rule the bars use for a category and
    // is what lets a radar be pointed at near the centre where every vertex is
    // on top of every other.
    if (options.hover && axes > 0 && outerRadius > 0.0f && !plotFrame.empty() &&
        marksTracked(ui, input, id, plotId, trigger)) {
        const Vec2 from{input.pointer().x - plotFrame.x - centre.x,
                        input.pointer().y - plotFrame.y - centre.y};
        if (std::hypot(from.x, from.y) <= outerRadius + options.labelRoom / 2.0f) {
            const float step = 360.0f / static_cast<float>(axes);
            float angle = std::atan2(from.y, from.x) * 180.0f / kPi + 90.0f;
            while (angle < 0.0f) angle += 360.0f;
            const auto nearest = static_cast<std::size_t>(std::llround(angle / step));
            result.hoveredIndex = static_cast<int>(nearest % axes);
        }
    }

    std::vector<LegendEntry> keys;
    for (std::size_t s = 0; s < series.size(); ++s) {
        keys.push_back({series[s].name,
                        series[s].color.value_or(style.palette.empty()
                                                     ? Token::Accent
                                                     : style.palette[s % style.palette.size()])});
    }
    const bool showLegend = options.legend.show && series.size() > 1;
    const int focused = options.legend.focused ? *options.legend.focused : -1;
    const auto shade = [&](std::size_t s, float alpha) {
        return focused >= 0 && focused != static_cast<int>(s) ? alpha * options.legend.dimAlpha
                                                              : alpha;
    };

    std::vector<Shape> shapes;
    if (axes > 0 && outerRadius > 0.0f) {
        const std::vector<double> ticks = scale.ticks(tickCount);

        // ---- the web -------------------------------------------------------
        // Outermost first, so the shaded bands of a polygon grid stack the
        // right way: each ring is a whole polygon, and an inner one drawn
        // second covers the middle of the one before it.
        if (options.grid != RadarGrid::None) {
            for (std::size_t ring = ticks.size(); ring-- > 0;) {
                const float fraction = std::clamp(scale.fraction(ticks[ring]), 0.0f, 1.0f);
                if (fraction <= 0.0f) continue;
                Path polygon;
                for (std::size_t axis = 0; axis < axes; ++axis) {
                    const Vec2 point = onCircle(centre, outerRadius * fraction, angleOf(axis));
                    if (axis == 0) polygon.moveTo(point);
                    else polygon.lineTo(point);
                }
                polygon.close();
                if (options.grid == RadarGrid::Polygon) {
                    // Alternating, and both steps faint: the bands are there to
                    // be counted out of the corner of the eye, not read.
                    shapes.push_back({std::move(polygon),
                                      Fill{Token::Border, ring % 2 == 0 ? 0.5f : 0.28f}, 0.0f});
                } else {
                    shapes.push_back({std::move(polygon), Fill{Token::Border}, 1.0f});
                }
            }

            // The spokes, over the rings either way: they are what a value is
            // traced along, and a polygon fill would otherwise bury them.
            for (std::size_t axis = 0; axis < axes; ++axis) {
                const bool lit = result.hoveredIndex == static_cast<int>(axis);
                Path spoke;
                spoke.moveTo(centre);
                spoke.lineTo(onCircle(centre, outerRadius, angleOf(axis)));
                shapes.push_back({std::move(spoke),
                                  Fill{lit ? Token::BorderStrong : Token::Border,
                                       options.grid == RadarGrid::Polygon && !lit ? 0.6f : 1.0f},
                                  1.0f});
            }
        }

        // ---- the series ----------------------------------------------------
        for (std::size_t s = 0; s < series.size(); ++s) {
            const Series& one = series[s];
            if (one.values.empty()) continue;
            const Token colour = one.color.value_or(
                style.palette.empty() ? Token::Accent : style.palette[s % style.palette.size()]);
            const float fillAlpha = one.fillAlpha.value_or(options.fillAlpha);
            const float thickness = one.thickness.value_or(style.lineThickness);

            Path polygon;
            for (std::size_t axis = 0; axis < axes; ++axis) {
                // A series shorter than the widest sits at the centre on the
                // axes it has nothing to say about, rather than closing early
                // and drawing a shape it does not mean.
                const double value = axis < one.values.size() && std::isfinite(one.values[axis])
                                         ? one.values[axis]
                                         : scale.low;
                const Vec2 point = vertexAt(axis, value);
                if (axis == 0) polygon.moveTo(point);
                else polygon.lineTo(point);
            }
            polygon.close();

            if (fillAlpha > 0.0f) {
                Path filled = polygon;
                shapes.push_back({std::move(filled), Fill{colour, shade(s, fillAlpha)}, 0.0f});
            }
            shapes.push_back({std::move(polygon), Fill{colour, shade(s, 1.0f)}, thickness});

            if (options.markerRadius > 0.0f) {
                for (std::size_t axis = 0; axis < axes && axis < one.values.size(); ++axis) {
                    if (!std::isfinite(one.values[axis])) continue;
                    const bool lit = result.hoveredIndex == static_cast<int>(axis);
                    shapes.push_back({circlePath(vertexAt(axis, one.values[axis]),
                                                 options.markerRadius * (lit ? 1.6f : 1.0f)),
                                      Fill{colour, shade(s, 1.0f)}, 0.0f});
                }
            }
        }
    }

    std::optional<Ui::Scope> outer;
    if (showLegend) {
        Style column;
        column.direction = Direction::Column;
        column.align = Align::Center;
        column.gap = 2.0f;
        column.shrink = 0.0f;
        outer.emplace(ui.begin(column));
    }

    Style plot;
    plot.width = options.size;
    plot.height = options.size;
    plot.shrink = 0.0f;
    auto plotScope = ui.begin(plot);
    ui.tag(plotId);

    Style marks;
    marks.position = Position::Absolute;
    marks.left = 0.0f;
    marks.top = 0.0f;
    marks.width = Length::percent(100);
    marks.height = Length::percent(100);
    ui.draw(marks, std::move(shapes));

    // ---- the names ---------------------------------------------------------
    // Centred on a point pushed out past the last ring, in a box wide enough to
    // hold a word: an axis label placed at the vertex itself sits on top of the
    // data, and one aligned by quadrant needs six cases to get the corners
    // right for the two it actually improves.
    if (options.labelRoom > 0.0f && axes > 0 && outerRadius > 0.0f) {
        constexpr float kLabelWidth = 76.0f;
        for (std::size_t axis = 0; axis < axes && axis < options.categories.size(); ++axis) {
            if (options.categories[axis].empty()) continue;
            const Vec2 at = onCircle(centre, outerRadius + 16.0f, angleOf(axis));
            Style slot;
            slot.position = Position::Absolute;
            slot.left = at.x - kLabelWidth / 2.0f;
            slot.top = at.y - 7.0f;
            slot.width = kLabelWidth;
            slot.justify = Justify::Center;
            auto slotScope = ui.begin(slot);
            ui.ignoresPointer();
            text(ui, options.categories[axis],
                 {.color = result.hoveredIndex == static_cast<int>(axis) ? Token::Text
                                                                         : Token::TextMuted,
                  .size = 10.5f,
                  .align = TextAlign::Center,
                  .overflow = TextOverflow::Ellipsis});
            (void)slotScope;
        }
    }

    // ---- the readout -------------------------------------------------------
    // Every series at the axis under the pointer, which is what makes the shapes
    // comparable: the whole reason two profiles are drawn on one web is to be
    // read against each other, one axis at a time.
    if (result.hoveredIndex >= 0 && trigger != ChartTrigger::None && !plotFrame.empty()) {
        const auto axis = static_cast<std::size_t>(result.hoveredIndex);
        const Vec2 at = onCircle(centre, outerRadius, angleOf(axis));
        drawSeriesReadout(ui, plotFrame, at.x, at.y, axis, series, options.categories,
                          options.valueFormat, options.tooltip, style);
    }
    plotScope.close();

    if (showLegend) drawLegend(ui, input, id, keys, options.legend);
    if (outer) outer->close();

    return result;
}

}  // namespace gbui
