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
#include "gbui/widgets/button.hpp"
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

struct PointSeries;
struct Candle;
struct Slice;

/** What a readout says about one mark.
 *
 * Handed to `ChartTooltip::rows`, whose job is to turn it into lines. Split
 * this way so a caller replacing the *content* does not also have to lay out a
 * panel, and one replacing the *look* does not have to reformat numbers.
 *
 * One struct for every chart rather than one per kind, because the alternative
 * is a `ChartTooltip` per kind and five copies of the callbacks with it. What
 * a chart cannot fill it leaves null, and which fields those are is decided by
 * which chart is asking — a `rows` callback is written for a call site, not for
 * charts in general. */
struct TooltipContext {
    /**
     * The mark under the pointer: the sample number for a line or a bar, the
     * point within its series for a scatter, the candle, the heatmap cell's
     * column, the slice.
     *
     * In the whole series, not in the view — a zoomed line chart still names
     * the sample the reader would find in the data.
     */
    std::size_t index = 0;
    /** Which series it came from, where `index` cannot name it alone: a
     *  scatter's dots and a heatmap's rows. Zero everywhere else. */
    std::size_t seriesIndex = 0;
    /** Whichever collection the chart was drawn from; the rest are null. */
    const std::vector<Series>* series = nullptr;
    const std::vector<PointSeries>* points = nullptr;
    const std::vector<Candle>* candles = nullptr;
    const std::vector<Slice>* slices = nullptr;
    /** A heatmap's cells, row-major, as it was handed them. */
    const std::vector<std::vector<double>>* cells = nullptr;
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

/**
 * The mark a group of charts is pointing at together.
 *
 * Two charts stacked over one axis — a price and its volume, a load and its
 * frequency — are one instrument, and a reader pointing at a moment in either
 * means that moment in both. Left unlinked they are two instruments that happen
 * to be adjacent: the crosshair lands on one, the other says nothing, and the
 * reader has to hold a position on screen in their head while they look at it.
 *
 * Shared by *index*, not by pixel, which is the only thing that survives two
 * charts of different widths, one with an axis and one without. That does mean
 * a group only makes sense over charts indexing the same samples — `lineChart`,
 * `barChart` and `candlestickChart` — and nowhere else: a donut's third wedge
 * and a scatter's third dot have nothing to do with each other.
 *
 * Held by the caller, like `ChartView` and for the same reason. Whichever chart
 * the pointer is actually over writes it and names itself in `source`; the
 * others read it and draw their own readout at that index. A chart built before
 * the one being pointed at follows a frame late, which at sixty frames a second
 * is not a thing a reader can see.
 */
struct ChartLink {
    /** The sample the group is showing, or -1 for none. */
    int index = -1;
    /** The id of the chart the pointer is actually over. A chart compares it
     *  with its own to tell "I am being hovered" from "I am following". */
    std::string source{};
};

/**
 * The key under a chart, and the focus a click on it gives.
 *
 * Shown by default, which is the one defensible default: a chart with two
 * series and no key is a chart whose colours mean nothing until the reader
 * finds the sentence that explains them. It is skipped anyway when no series
 * carries a name, so a single unnamed line costs nothing.
 *
 * Clicking an entry singles that series out and dims the rest — the same
 * gesture the donut's legend already has, and what a reader tries when six
 * lines have become a thicket. It needs somewhere to remember which one, so a
 * null `focused` leaves the legend a key and nothing more.
 */
struct ChartLegend {
    bool show = true;
    /** The series singled out, or -1. The caller's, like every other piece of
     *  state here: `int focused = -1;` beside the chart's data is the whole of
     *  what it takes. */
    int* focused = nullptr;
    /** What the others fade to while one is singled out. */
    float dimAlpha = 0.22f;
    float height = 24.0f;
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
    /** Ties this chart's readout to the other charts sharing the same
     *  `ChartLink`, so pointing at a sample in one points at it in all of
     *  them. Null leaves the chart on its own. */
    ChartLink* link = nullptr;
    /** The key drawn under the chart, and the focus a click on it gives. */
    ChartLegend legend{};
    /**
     * What the chart is of — "Revenue by month".
     *
     * A chart is the one thing on a screen a reader gets nothing from: the
     * shapes carry all of it, and the numbers behind them are the caller's. A
     * name and a `Figure` role are the least that makes it navigable, and they
     * are not the whole answer — see the note in `docs/reference/accessibility`.
     */
    std::string_view name{};
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

    /**
     * The range being swept out right now, while the button is still down.
     *
     * Held by the caller like the view itself, and for the same reason: the
     * chart remembers nothing between frames, so the one thing a rubber band
     * needs — where the press landed — has to live somewhere that does. In
     * fractions of the whole series, the same units as `from` and `to`, so it
     * still means the same thing after the pointer has left the plot.
     *
     * Nothing outside the gesture reads it, and it is empty the rest of the
     * time. It is separate from `from`/`to` on purpose: the view cannot follow
     * the sweep as it is drawn, because rescaling the plot under the pointer
     * would move the very data the reader is aiming at.
     */
    struct Sweep {
        bool active = false;
        double from = 0.0;
        double to = 0.0;
        /**
         * The plot the press landed on.
         *
         * Needed the moment a view is shared, which is the point of sharing
         * one: the second chart is handed a sweep it did not start, and without
         * a name on it that chart reads "a sweep is running and the pointer is
         * not on me" as "the reader let go" and commits the gesture out from
         * under the chart still drawing it.
         */
        std::string on{};
    } sweep{};

    bool whole() const { return from <= 0.0 && to >= 1.0; }
    double span() const { return to - from; }
    void reset() {
        from = 0.0;
        to = 1.0;
        sweep = {};
    }
    /** Clamped to [0,1], at least `minSpan` wide, and the right way round. */
    void normalise(double minSpan = 0.01);
};

/** What dragging inside the plot does. */
enum class ChartDrag {
    /** Nothing: a press is only ever a click, for the readout. */
    None,
    /**
     * Sweeps out a range and zooms to it when the button comes up.
     *
     * The default, and what a reader tries first — the range they want is one
     * they can already see, and pointing straight at it is fewer moves than
     * steering a window into place from somewhere else. Drawn in either
     * direction, because about half of them start from the right-hand end.
     *
     * Holding Shift pans instead, so the second gesture is still on the plot
     * rather than only on a strip underneath it.
     */
    Select,
    /**
     * Slides the view under the pointer.
     *
     * Worth choosing when the chart is a timeline the reader mostly travels
     * along rather than one they cut ranges out of — and it does nothing at
     * all until something is zoomed in, since a whole view has nowhere to go.
     */
    Pan,
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
    /** What a drag inside the plot means. */
    ChartDrag drag = ChartDrag::Select;
    /** How far a sweep has to travel, in pixels, before it counts as one.
     *
     *  Under this it is a click — which is what a reader asking for the readout
     *  does, and zooming their chart to a two-pixel slice because their hand
     *  moved would be a trap rather than a feature. */
    float dragThreshold = 4.0f;
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

struct ChartToolbarOptions {
    /** Which of the three buttons the strip carries. All of them by default;
     *  drop the ones a chart has no room for. */
    bool zoomIn = true;
    /** As above. */
    bool zoomOut = true;
    /** Puts the whole series back. Disabled while it already is whole, because
     *  a button that does nothing is worse than one that is not there — the
     *  reader presses it and learns nothing about why. */
    bool reset = true;
    /**
     * How much of the window a press takes off, as a fraction of it.
     *
     * 0.4 rather than a half: two presses should not lose 75% of the data, and
     * a reader who wants that has the sweep, which says exactly what to keep.
     */
    double step = 0.4;
    /** The narrowest the view may get, as a fraction of the whole. The same
     *  floor `BrushOptions` uses, so the two controls agree about how far in a
     *  reader is allowed to go. */
    double minSpan = 0.02;
    /**
     * How the buttons are drawn.
     *
     * `Secondary` by default, which gives them a surface and an outline: three
     * bare glyphs floating in a card header read as marks rather than as
     * controls, and a reader has to try one to find out it can be pressed.
     * `Ghost` is the bare version, for a toolbar that sits on top of the plot
     * where a surface would be in the way.
     */
    ButtonVariant variant = ButtonVariant::Secondary;
    /** The buttons are square, so this is their size. */
    float height = 30.0f;
    /** Bigger than a button with a label would use: the glyph is the whole
     *  button here, and at fourteen pixels in a thirty-pixel square it reads as
     *  a mark stranded in the middle of a box. */
    float iconSize = 17.0f;
};

/**
 * The buttons ApexCharts puts in the corner of a chart: zoom in, zoom out, and
 * put it back.
 *
 * A row of ordinary buttons over the caller's own `ChartView`, and nothing
 * else — which is why it is a component rather than an option on the chart. It
 * has no idea which chart it belongs to, so one row can drive a group of them
 * the same way one view already zooms a price and its volume together, and a
 * caller who wants it somewhere other than the top right corner simply puts it
 * there.
 *
 * The gestures are the primary way in and this is the secondary one, for the
 * same reason a map has both: a sweep says *which range*, and these say *a bit
 * more* — and only one of the two can be pressed by someone who has already
 * lost the thread of where they are.
 *
 * Returns true on any frame the view moved.
 */
bool chartToolbar(Ui& ui, const Interaction& input, std::string_view id, ChartView& view,
                  const ChartToolbarOptions& options = {});

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
 * zooming — a drag inside the plot does that, and so does the wheel — but the
 * *context*: once zoomed in, a reader has no way to tell where they are or how
 * much they cannot see. The strip answers both without them having to zoom back
 * out, which is also why it is optional.
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
    /** How several series share a category: side by side, or on top of each
     *  other. */
    BarGrouping grouping = BarGrouping::Grouped;
    /** A filled rectangle from the baseline, or a stem with a dot at the
     *  value. */
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
    ChartTooltip tooltip{};
    /** Ties this chart's readout to the other charts sharing the same
     *  `ChartLink`, so pointing at a sample in one points at it in all of
     *  them. Null leaves the chart on its own. */
    ChartLink* link = nullptr;
    /** The key drawn under the chart, and the focus a click on it gives. */
    ChartLegend legend{};
    /**
     * What the chart is of — "Revenue by month".
     *
     * A chart is the one thing on a screen a reader gets nothing from: the
     * shapes carry all of it, and the numbers behind them are the caller's. A
     * name and a `Figure` role are the least that makes it navigable, and they
     * are not the whole answer — see the note in `docs/reference/accessibility`.
     */
    std::string_view name{};
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

/**
 * The same bars, showing only `view` of the categories and letting the reader
 * move it.
 *
 * The view is the caller's, like everything else here, and sharing one is the
 * point: a volume chart handed the same `ChartView` as the price chart above it
 * zooms when that one is swept, because they are looking at the same window of
 * the same data rather than each keeping their own idea of it.
 */
ChartResult barChart(Ui& ui, const Interaction& input, std::string_view id,
                     const std::vector<Series>& series, ChartView& view,
                     const BarChartOptions& options = {}, const ChartZoom& zoom = {});

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
    /** As above, for the other one. */
    Scale yScale{0.0, 0.0};
    bool autoScale = true;
    std::optional<int> tickCount{};
    std::optional<float> axisWidth{};
    float height = 220.0f;
    std::optional<bool> grid{};
    bool hover = true;
    std::string_view valueFormat = "%.0f";
    /** The same, for the x value. Two formats because the axes are two
     *  different quantities — seconds against dollars. */
    std::string_view xFormat = "%.0f";
    /** Room under the plot for the x labels. Zero draws none. */
    float xAxis = 20.0f;
    /** The radii the weights are spread across, when there are weights. */
    float minRadius = 5.0f;
    float maxRadius = 26.0f;
    /** How solid a dot is. Under one on purpose: overlapping points then read
     *  as denser rather than hiding each other, and density is most of the
     *  information in a crowded scatter. */
    float fillAlpha = 0.6f;
    /** How near the pointer has to be, in pixels, to pick a dot. */
    float hitRadius = 14.0f;
    ChartTooltip tooltip{};
    ChartLegend legend{};
    /**
     * What the chart is of — "Revenue by month".
     *
     * A chart is the one thing on a screen a reader gets nothing from: the
     * shapes carry all of it, and the numbers behind them are the caller's. A
     * name and a `Figure` role are the least that makes it navigable, and they
     * are not the whole answer — see the note in `docs/reference/accessibility`.
     */
    std::string_view name{};
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
    /** As above, along the top. */
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
    /** The same, along the top. */
    float columnLabels = 16.0f;
    bool hover = true;
    std::string_view valueFormat = "%.0f";
    ChartTooltip tooltip{};
    /**
     * What the chart is of — "Revenue by month".
     *
     * A chart is the one thing on a screen a reader gets nothing from: the
     * shapes carry all of it, and the numbers behind them are the caller's. A
     * name and a `Figure` role are the least that makes it navigable, and they
     * are not the whole answer — see the note in `docs/reference/accessibility`.
     */
    std::string_view name{};
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
    /**
     * A ceiling on the body, in pixels, whatever the slot allows.
     *
     * A candle is a *mark*, not a bar: zooming in should make it taller and put
     * more air around it, not fatten it. Without the ceiling a chart zoomed to
     * a dozen periods draws them as forty-pixel bricks, which is the width of a
     * bar chart's column and reads as one. The default barely bites at a full
     * view — forty-odd candles across a card land near it anyway — and does all
     * its work once the reader has swept a range.
     */
    float maxBodyWidth = 14.0f;
    std::vector<std::string> categories{};
    /** Room for the category names. Zero draws none. */
    float categoryAxis = 22.0f;
    /** Unset takes the theme's added/removed colours, which are already the
     *  two a reader of this application associates with up and down. */
    std::optional<Token> rising{};
    /** As above, for the other direction. */
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
    ChartTooltip tooltip{};
    /** Ties this chart's readout to the other charts sharing the same
     *  `ChartLink`, so pointing at a sample in one points at it in all of
     *  them. Null leaves the chart on its own. */
    ChartLink* link = nullptr;
    /**
     * What the chart is of — "Revenue by month".
     *
     * A chart is the one thing on a screen a reader gets nothing from: the
     * shapes carry all of it, and the numbers behind them are the caller's. A
     * name and a `Figure` role are the least that makes it navigable, and they
     * are not the whole answer — see the note in `docs/reference/accessibility`.
     */
    std::string_view name{};
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

/** The same candles, showing only `view` of them — the overload a price chart
 *  and its volume share, so sweeping either zooms both. */
ChartResult candlestickChart(Ui& ui, const Interaction& input, std::string_view id,
                             const std::vector<Candle>& candles, ChartView& view,
                             const CandlestickOptions& options = {},
                             const ChartZoom& zoom = {});

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
    /** The readout beside the hovered wedge. A donut's legend already names
     *  every slice, so this one is about the *number* — which the legend has
     *  no room for once there are more than a handful. */
    ChartTooltip tooltip{};
    /**
     * What the chart is of — "Revenue by month".
     *
     * A chart is the one thing on a screen a reader gets nothing from: the
     * shapes carry all of it, and the numbers behind them are the caller's. A
     * name and a `Figure` role are the least that makes it navigable, and they
     * are not the whole answer — see the note in `docs/reference/accessibility`.
     */
    std::string_view name{};
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

// ---------------------------------------------------------------------------
// Radar
// ---------------------------------------------------------------------------

/** How a radar's web is drawn behind the data. */
enum class RadarGrid {
    /** Rings and spokes as hairlines: the mesh a reader traces a value along. */
    Web,
    /**
     * The bands between the rings shaded in alternating steps, and no ring
     * lines at all — ApexCharts calls this a polygon fill.
     *
     * Reads as a target rather than as graph paper, which suits a chart being
     * *looked* at more than one being measured off: the shape of the series is
     * what carries, and a mesh behind it competes.
     */
    Polygon,
    None,
};

struct RadarOptions {
    /**
     * Fixed bounds. Left alone the chart takes them from the data — and always
     * from zero, whatever the data says.
     *
     * A radar is read as an *area*, and area from a non-zero baseline is the
     * same lie a truncated bar tells, told about five axes at once: two
     * profiles a hair apart become one enormous and one tiny.
     */
    Scale scale{0.0, 0.0};
    bool autoScale = true;
    std::optional<int> tickCount{};
    /** One name per axis, drawn outside its spoke. Fewer than there are values
     *  leaves the rest unlabelled rather than renumbering them. */
    std::vector<std::string> categories{};
    /** The whole chart's box, labels included. */
    float size = 240.0f;
    /** What is drawn behind the data: a mesh, shaded bands, or nothing. */
    RadarGrid grid = RadarGrid::Web;
    /** Room between the outermost ring and the edge, for the names. Zero draws
     *  none and gives the web the whole box. */
    float labelRoom = 52.0f;
    /**
     * Shading inside a series' polygon, when the series carries no `fillAlpha`
     * of its own.
     *
     * Filled by default, where a line chart is not. A line is read along, and
     * shading under two of them buries the lower one; a radar is read as a
     * shape, and an unfilled one is a wire outline the eye has to close for
     * itself.
     *
     * Lower than the 0.2 the web charting libraries settle on, and deliberately:
     * the painter composites in linear light, where the same number covers
     * roughly twice as much as it does in a browser blending sRGB directly.
     */
    float fillAlpha = 0.1f;
    /** A dot at each vertex. Zero draws none. */
    float markerRadius = 3.0f;
    bool hover = true;
    /** Printed in the readout; "%.0f" style, as `std::snprintf` takes. */
    std::string_view valueFormat = "%.0f";
    ChartTooltip tooltip{};
    ChartLegend legend{};
    /**
     * What the chart is of — "Revenue by month".
     *
     * A chart is the one thing on a screen a reader gets nothing from: the
     * shapes carry all of it, and the numbers behind them are the caller's. A
     * name and a `Figure` role are the least that makes it navigable, and they
     * are not the whole answer — see the note in `docs/reference/accessibility`.
     */
    std::string_view name{};
};

/**
 * One value per axis, per series, on spokes around a common centre.
 *
 * The chart for a *profile* — a handful of measures that belong together and
 * are compared as a shape rather than read off one at a time: a skills
 * assessment, a wine's tasting notes, one machine's five utilisation figures
 * against another's. Everything it is good at falls apart past about eight
 * axes, where the polygon stops being a shape and becomes a scribble.
 *
 * `series` is the same `Series` every other chart here takes; each value is a
 * spoke, in order, starting at twelve o'clock and going clockwise. Hit testing
 * is by *angle*, so a reader pointing anywhere along a spoke means that axis —
 * the same rule the bars use for a category, for the same reason.
 */
ChartResult radarChart(Ui& ui, const Interaction& input, std::string_view id,
                       const std::vector<Series>& series, const RadarOptions& options = {});

}  // namespace gbui
