// Charts: a scale, its ticks, and a line or area drawn from data.
#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/scroll.hpp"

namespace gbui {

/**
 * A linear mapping from a range of values onto a range of pixels, and — the
 * part that actually takes the work — a set of ticks at *round* numbers.
 *
 * Dividing a range by five gives ticks at 0, 3.7, 7.4… which is arithmetically
 * correct and unreadable. A reader wants 0, 2, 4, 6, 8, so the step is snapped
 * to 1, 2, 5 or 10 times a power of ten and the ends are widened to sit on it.
 */
struct Scale {
    double low = 0.0;
    double high = 1.0;

    /** Widens the range so both ends land on a round step. */
    static Scale nice(double low, double high, int targetTicks = 5);

    /** The round values between the ends, inclusive. */
    std::vector<double> ticks(int targetTicks = 5) const;

    /** 0 at `low`, 1 at `high`. */
    float fraction(double value) const {
        const double span = high - low;
        return span > 0.0 ? static_cast<float>((value - low) / span) : 0.0f;
    }
};

/** One line, with its own colour. */
struct Series {
    std::string_view name{};
    std::vector<double> values{};
    /** Unset cycles the design's chart palette by position, so three series get
     *  three distinguishable colours without anyone choosing them. */
    std::optional<Token> color{};
    /** Shades the area under the line. Unset takes the design's. */
    std::optional<float> fillAlpha{};
    /** Unset takes the design's line weight. */
    std::optional<float> thickness{};
};

/** One line of a chart's readout. */
struct TooltipRow {
    /** Empty draws no swatch — for a total, or a note. */
    std::optional<Token> color{};
    std::string label{};
    std::string value{};
};

/** What a readout says about one sample.
 *
 * Handed to `ChartTooltip::rows`, whose job is to turn it into lines. Split
 * this way so a caller replacing the *content* does not also have to lay out a
 * panel, and one replacing the *look* does not have to reformat numbers. */
struct TooltipContext {
    /** Sample number in the whole series, not in the view. */
    std::size_t index = 0;
    /** The series drawn, in the order they were given. */
    const std::vector<Series>* series = nullptr;
};

/**
 * A chart's readout: when it appears, what it says, and how it looks.
 *
 * Every field falls back to `Design::chart.tooltip`, so an application sets the
 * look once and each call site only says what is different — usually nothing,
 * or just the numbers.
 */
struct ChartTooltip {
    std::optional<ChartTrigger> trigger{};
    /**
     * Builds the rows for a sample. Unset lists one row per series: its name,
     * its colour and its value through `valueFormat`.
     *
     * A callback rather than a format string because the interesting readouts
     * are not reformattings of the number — they are a percentage of a total, a
     * delta from the previous sample, the commit that sample came from. None of
     * those are expressible as `%.1f`.
     */
    std::function<std::vector<TooltipRow>(const TooltipContext&)> rows{};
    /** The heading. Unset uses the category name, or the sample number. */
    std::function<std::string(const TooltipContext&)> title{};
    /** Draws the whole panel, replacing everything above. For a caller that
     *  wants a sparkline or an image in there. */
    std::function<void(Ui&, const TooltipContext&)> body{};
    /** Overrides for the look; unset takes the design's. */
    std::optional<Token> background{};
    std::optional<float> radius{};
    std::optional<bool> swatches{};
};

struct ChartOptions {
    /** Fixed bounds. Left as they are, the chart takes them from the data —
     *  which is what a live readout wants and what a comparison does not. */
    Scale scale{0.0, 0.0};
    bool autoScale = true;
    /** Each of these falls back to `Design::chart` when it is not set, so a
     *  dashboard configures its charts once instead of per call site. */
    std::optional<int> tickCount{};
    /** Room for the value labels down the left. Zero draws none. */
    std::optional<float> axisWidth{};
    float height = 120.0f;
    std::optional<bool> grid{};
    /** A crosshair and a readout following the pointer. */
    bool hover = true;
    /** Printed beside a hovered value; "%.0f" style, as `std::snprintf` takes. */
    std::string_view valueFormat = "%.0f";
    /** Names for the samples, used as the readout's heading. */
    std::vector<std::string> categories{};
    ChartTooltip tooltip{};
};

/**
 * Which slice of the data a chart is showing, as fractions of the whole.
 *
 * Fractions rather than indices so the same view survives the series growing:
 * a live chart appending a sample every second would otherwise creep, because
 * "samples 40 to 90" means something different each time it is asked.
 */
struct ChartView {
    double from = 0.0;
    double to = 1.0;

    bool whole() const { return from <= 0.0 && to >= 1.0; }
    double span() const { return to - from; }
    void reset() {
        from = 0.0;
        to = 1.0;
    }
    /** Clamped to [0,1], at least `minSpan` wide, and the right way round. */
    void normalise(double minSpan = 0.01);
};

/** How a chart responds to the wheel and to dragging. */
struct ChartZoom {
    /**
     * Off by default, and this is the important default in the whole struct.
     *
     * A chart lives inside a page that scrolls. A chart that takes the wheel
     * whenever the pointer is over it steals every scroll aimed past it, and
     * the reader has to route around a hole in the middle of the page. So the
     * wheel is only taken when a caller asks for it, and even then only with
     * `wheelModifier` held unless that is turned off too.
     */
    bool wheel = false;
    /**
     * Require Ctrl for wheel zoom, leaving a plain wheel to scroll the page.
     *
     * The same bargain embedded maps settle on, for the same reason: the reader
     * scrolls past a chart far more often than they zoom one.
     */
    bool wheelModifier = true;
    /** Dragging inside the plot slides the view. */
    bool drag = true;
    /** The narrowest the view may get, as a fraction of the whole. */
    double minSpan = 0.02;
};

struct ChartResult {
    /** The sample under the pointer, or -1. Data-space, not pixels: a caller
     *  can name the commit rather than the nearest rectangle. */
    int hoveredIndex = -1;
    /** The view moved this frame — zoomed, panned or brushed. */
    bool viewChanged = false;
    /** The wheel was taken for zooming, so nothing outside should also act on
     *  it. Only ever true when `ChartZoom::wheel` asked for it. */
    bool wheelTaken = false;
};

/**
 * Draws one or more series over a shared scale.
 *
 * The geometry is built here and handed to `Ui::draw`, so the whole chart is
 * one node plus its labels rather than a widget per mark. Hit testing is on the
 * *index* — the pointer's x is turned back into a sample number — which is what
 * lets a tooltip name the right point instead of the nearest pixel.
 */
ChartResult lineChart(Ui& ui, const Interaction& input, std::string_view id,
                      const std::vector<Series>& series, const ChartOptions& options = {});

/**
 * The same chart, showing only `view` of the data and letting the reader move
 * it.
 *
 * The view is the caller's, like every other piece of state here — which is
 * what lets two charts share one, so panning a line chart pans the bars under
 * it.
 */
ChartResult lineChart(Ui& ui, const Interaction& input, std::string_view id,
                      const std::vector<Series>& series, ChartView& view,
                      const ChartOptions& options = {}, const ChartZoom& zoom = {});

struct BrushOptions {
    float height = 46.0f;
    /** Room down the left, so the strip lines up with the chart above it. */
    float axisWidth = 0.0f;
    /** Handle width, and the smallest grabbable window. */
    float handleWidth = 8.0f;
    /** Dragging the strip's empty part draws a new window instead of moving
     *  the existing one — which is how a reader selects a range they can see
     *  but are not in. */
    bool dragSelects = true;
};

/**
 * A strip of the whole series with the current view as a window over it.
 *
 * The part an ApexCharts-style brush actually earns its keep with is not the
 * zooming — the wheel does that — but the *context*: once zoomed in, a reader
 * has no way to tell where they are or how much they cannot see. The strip
 * answers both without them having to zoom back out.
 *
 * Returns true on any frame the view moved.
 */
bool chartBrush(Ui& ui, const Interaction& input, std::string_view id,
                const std::vector<Series>& series, ChartView& view,
                const BrushOptions& options = {});

// ---------------------------------------------------------------------------
// Bars
// ---------------------------------------------------------------------------

/** How several series share a category. */
enum class BarGrouping {
    /** Side by side inside the slot, which compares series against each other. */
    Grouped,
    /** Piled up, which compares each category's *total* and shows the parts. */
    Stacked,
};

/** What one value is drawn as. */
enum class BarShape {
    /** A filled rectangle from the baseline. */
    Bar,
    /**
     * A hairline stem with a dot at the value.
     *
     * The same information as a bar with most of the ink removed, which is the
     * point: when there are many categories and the differences between them
     * are small, a row of wide filled bars is mostly a solid block, and the
     * differences live in the top few pixels. A lollipop puts the reader's eye
     * on exactly those pixels.
     */
    Lollipop,
};

struct BarChartOptions {
    /** Fixed bounds. Left as they are, the chart takes them from the data. */
    Scale scale{0.0, 0.0};
    bool autoScale = true;
    /** Each of these falls back to `Design::chart` when unset. */
    std::optional<int> tickCount{};
    /** Room for the value labels. Zero draws none. */
    std::optional<float> axisWidth{};
    float height = 160.0f;
    std::optional<bool> grid{};
    bool hover = true;
    /** `std::snprintf` style, for the value labels and readouts. */
    std::string_view valueFormat = "%.0f";

    /** Bars run left to right from a category axis down the side, instead of
     *  bottom to top from one along the bottom. Long category names are the
     *  usual reason: horizontal gives them room to be read. */
    bool horizontal = false;
    BarGrouping grouping = BarGrouping::Grouped;
    BarShape shape = BarShape::Bar;
    /** Share of each category's slot left as gap, 0 to 0.9. */
    float categoryPadding = 0.28f;
    /** Between the bars of different series in one category, in pixels. */
    float seriesGap = 2.0f;
    float radius = 2.0f;
    /** Names along the category axis. Fewer than there are values leaves the
     *  rest unlabelled rather than renumbering them. */
    std::vector<std::string> categories{};
    /** Room for the category names. Zero draws none. */
    float categoryAxis = 22.0f;
};

/**
 * Bars, columns or lollipops over a shared scale.
 *
 * One function rather than four because they differ only in how a value is
 * turned into a rectangle: transposing the axes gives a bar chart from a column
 * chart, stacking changes where each bar starts, and a lollipop is a bar drawn
 * as a stem. Splitting them would mean four copies of the scale, the axis, the
 * grid and the hit testing.
 */
ChartResult barChart(Ui& ui, const Interaction& input, std::string_view id,
                     const std::vector<Series>& series, const BarChartOptions& options = {});

// ---------------------------------------------------------------------------
// Points
// ---------------------------------------------------------------------------

/** One point on two continuous scales. */
struct Point {
    double x = 0.0;
    double y = 0.0;
    /**
     * A third value, drawn as the dot's *area*.
     *
     * Zero leaves every dot the same size, which is a scatter. Area rather than
     * radius because a reader compares the ink: mapping the value to the radius
     * makes a doubled value look four times bigger, and every bubble chart that
     * gets this wrong overstates its large values.
     */
    double weight = 0.0;
};

struct PointSeries {
    std::string_view name{};
    std::vector<Point> points{};
    std::optional<Token> color{};
    /** Dot radius for a series with no weights. */
    float radius = 4.0f;
};

struct ScatterOptions {
    /** Fixed bounds for each axis. Left alone, both come from the data. */
    Scale xScale{0.0, 0.0};
    Scale yScale{0.0, 0.0};
    bool autoScale = true;
    std::optional<int> tickCount{};
    std::optional<float> axisWidth{};
    float height = 220.0f;
    std::optional<bool> grid{};
    bool hover = true;
    std::string_view valueFormat = "%.0f";
    std::string_view xFormat = "%.0f";
    /** Room under the plot for the x labels. Zero draws none. */
    float xAxis = 20.0f;
    /** The radii the weights are spread across, when there are weights. */
    float minRadius = 5.0f;
    float maxRadius = 26.0f;
    float fillAlpha = 0.6f;
    /** How near the pointer has to be, in pixels, to pick a dot. */
    float hitRadius = 14.0f;
    ChartTooltip tooltip{};
};

struct ScatterResult {
    /** Which series the dot under the pointer belongs to, or -1. */
    int hoveredSeries = -1;
    /** Its index within that series, or -1. */
    int hoveredIndex = -1;
};

/**
 * Points on two continuous scales — a scatter, or a bubble chart when the
 * points carry weights.
 *
 * The first chart here with a real **x scale**. Every other one indexes its
 * samples along the bottom: sample 0 at the left, sample n at the right, evenly
 * spaced whatever their x values are. That is right for a series over time and
 * wrong for a correlation, where the whole question is where the points sit
 * against each other on both axes.
 *
 * Hit testing is by distance to the nearest dot rather than by column, because
 * a scatter has no columns — two points can share an x and mean different
 * things.
 */
ScatterResult scatterChart(Ui& ui, const Interaction& input, std::string_view id,
                           const std::vector<PointSeries>& series,
                           const ScatterOptions& options = {});

// ---------------------------------------------------------------------------
// Heatmap
// ---------------------------------------------------------------------------

struct HeatmapOptions {
    /** Names down the side and along the top. Fewer than there are rows or
     *  columns leaves the rest unlabelled rather than renumbering them. */
    std::vector<std::string> rows{};
    std::vector<std::string> columns{};
    Scale scale{0.0, 0.0};
    bool autoScale = true;
    /**
     * Quantise the colour into this many steps; zero is a continuous ramp.
     *
     * Five by default, and steps rather than a ramp because a reader compares
     * cells by *matching* them, not by judging absolute lightness. Two cells
     * one step apart are visibly different; two cells 3% apart on a continuous
     * ramp are the same colour to anyone reading a grid at a glance.
     */
    int steps = 5;
    /** The hue the scale runs to. Unset takes the design's first chart colour,
     *  so the grid re-themes with everything else. */
    std::optional<Token> color{};
    /** Zero lets the cells share out the width. */
    float cellSize = 0.0f;
    float gap = 3.0f;
    float radius = 2.0f;
    /** Room for the labels. Zero draws none. */
    float rowLabels = 34.0f;
    float columnLabels = 16.0f;
    bool hover = true;
    std::string_view valueFormat = "%.0f";
};

struct HeatmapResult {
    int hoveredRow = -1;
    int hoveredColumn = -1;
    /** The value under the pointer, when there is one. */
    double hoveredValue = 0.0;
};

/**
 * A grid of cells shaded by value.
 *
 * `values` is row-major and may be ragged — a short row simply has fewer cells,
 * which is what a calendar's last week is.
 *
 * The colour is one token at varying strength rather than a rainbow. A
 * multi-hue scale looks richer and reads worse: hue carries no order, so a
 * reader has to consult the legend for every cell, while "more of the same
 * colour" needs no legend at all.
 */
HeatmapResult heatmap(Ui& ui, const Interaction& input, std::string_view id,
                      const std::vector<std::vector<double>>& values,
                      const HeatmapOptions& options = {});

// ---------------------------------------------------------------------------
// Candlesticks
// ---------------------------------------------------------------------------

/** One period: where it opened, how far it ranged, and where it closed. */
struct Candle {
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;

    bool rising() const { return close >= open; }
    /** Guards against a series that has `high` and `low` the wrong way round,
     *  which is common enough in real feeds to be worth absorbing here. */
    double top() const { return std::max({open, high, low, close}); }
    double bottom() const { return std::min({open, high, low, close}); }
};

struct CandlestickOptions {
    /** Fixed bounds. Left as they are, the chart takes them from the data. */
    Scale scale{0.0, 0.0};
    bool autoScale = true;
    /** Each of these falls back to `Design::chart` when unset. */
    std::optional<int> tickCount{};
    /** Room for the value labels. Zero draws none. */
    std::optional<float> axisWidth{};
    float height = 160.0f;
    std::optional<bool> grid{};
    bool hover = true;
    /** `std::snprintf` style, for the value labels and readouts. */
    std::string_view valueFormat = "%.0f";

    /** Share of each period's slot left as gap, 0 to 0.9. */
    float categoryPadding = 0.3f;
    std::vector<std::string> categories{};
    float categoryAxis = 22.0f;
    /** Unset takes the theme's added/removed colours, which are already the
     *  two a reader of this application associates with up and down. */
    std::optional<Token> rising{};
    std::optional<Token> falling{};
    /**
     * Draw rising candles as an outline instead of a solid body.
     *
     * The other common convention, and worth having: on a dense chart the
     * hollow bodies recede and the falling ones stand out, which is what a
     * reader watching for drops actually wants.
     */
    bool hollowRising = false;
    /** Width of the wick, and of a hollow body's outline. */
    float lineWidth = 1.0f;
};

/**
 * Open, high, low and close per period.
 *
 * The one chart here whose scale deliberately does **not** reach zero. Every
 * other chart in this file widens its range to include the baseline, because a
 * bar that does not start at zero misstates the ratio between two bars. A price
 * has no such baseline — forcing one on a stock that trades between 180 and 190
 * squeezes the entire year into the top five percent of the plot and hides the
 * only thing the chart was drawn to show.
 */
ChartResult candlestickChart(Ui& ui, const Interaction& input, std::string_view id,
                             const std::vector<Candle>& candles,
                             const CandlestickOptions& options = {});

/** One wedge of a donut: a value and the colour it is drawn in. */
struct Slice {
    std::string_view name{};
    double value = 0.0;
    /** Unset cycles the design's chart palette, as a series does. */
    std::optional<Token> color{};
};

/** What a donut remembers: which wedge was singled out, and where its legend is
 *  scrolled to. Owned by the application, like every other piece of state. */
struct DonutState {
    /** The wedge the reader clicked, or -1 for none. */
    int focused = -1;
    ScrollState legend{};
};

struct DonutOptions {
    float size = 150.0f;
    /** How thick the ring is, as a share of the radius. 1 is a full pie.
     *  Unset takes the design's. */
    std::optional<float> thickness{};
    /** A gap between wedges, in degrees, so neighbouring colours do not merge. */
    std::optional<float> padAngle{};
    /** Where the first wedge starts. -90 puts it at twelve o'clock, which is
     *  where every reader expects a donut to begin. */
    float startAngle = -90.0f;
    /** The hovered wedge grows outward by this much. */
    float hoverGrow = 4.0f;
    /** How much further a *focused* wedge pulls out of the ring. */
    float focusGrow = 10.0f;
    /** What the wedges that were not singled out fade to. */
    float dimAlpha = 0.3f;
    bool legend = true;
    /** A ceiling on the legend before it scrolls, so a chart with forty
     *  contributors does not grow taller than the ring beside it. `kAuto`
     *  matches the donut. */
    float legendMaxHeight = kAuto;
};

struct DonutResult {
    /** The wedge under the pointer, or -1 — by *index*, so a caller names the
     *  contributor rather than the nearest pixel. */
    int hoveredIndex = -1;
    /** A wedge was clicked this frame; `state.focused` already reflects it. */
    bool focusChanged = false;
};

/**
 * A donut, or a pie when `thickness` is 1.
 *
 * Proof that the canvas node carries more than polylines: each wedge is two
 * arcs and two lines, and an arc is cubics. Nothing here is a special case in
 * the painter — it is the same vector layer the icons ride on.
 */
DonutResult donutChart(Ui& ui, const Interaction& input, std::string_view id,
                       const std::vector<Slice>& slices, DonutState& state,
                       const DonutOptions& options = {});

}  // namespace gbui
