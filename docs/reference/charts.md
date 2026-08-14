# charts

`#include "gbui/widgets/chart.hpp"`

A chart here is **one node plus its labels**, not a widget per mark: the geometry
is built and handed to [`Ui::draw`](/reference/scene#ui), which takes a rectangle
and a list of shapes in the node's own coordinates. That is the same vector layer
the icons ride on, and it is why a commit graph, a sparkline and a donut are one
leaf each.

Hit testing is on the **data**, not on pixels. Every chart reports an *index*, so
a caller names the commit or the contributor rather than the nearest rectangle.

Everything unset falls back to `Design::chart`, so an application configures its
charts once instead of per call site — line weight, tick count, axis width, donut
thickness, the series palette and the tooltip's look. The palette is *tokens*, so
a chart re-themes with everything else.

## Scale

```cpp
struct Scale {
    double low, high;

    static Scale nice(double low, double high, int targetTicks = 5);
    std::vector<double> ticks(int targetTicks = 5) const;
    float fraction(double value) const;      // 0 at low, 1 at high
};
```

`nice` snaps the step to 1, 2, 5 or 10 times a power of ten and widens the ends
onto it. Dividing a range by five gives ticks at 0, 3.7, 7.4… which is
arithmetically correct and unreadable; a reader wants 0, 2, 4, 6, 8. That is the
whole of the difference between a chart that is read and one that is decoded.

## Series and readouts

```cpp
struct Series {
    std::string_view name;
    std::vector<double> values;
    std::optional<Token> color{};        // unset cycles the design's palette
    std::optional<float> fillAlpha{};    // shading under the line
    std::optional<float> thickness{};
};
```

```cpp
struct ChartTooltip {
    std::optional<ChartTrigger> trigger{};                     // Hover | Click | None
    std::function<std::vector<TooltipRow>(const TooltipContext&)> rows{};
    std::function<std::string(const TooltipContext&)> title{};
    std::function<void(Ui&, const TooltipContext&)> body{};     // replaces the panel
    std::optional<Token> background{};
    std::optional<float> radius{};
    std::optional<bool> swatches{};
};
```

`rows` is a callback rather than a format string because the readouts worth
having are not reformattings of the number — a percentage of a total, a delta
from the previous sample, the commit that sample came from. None of those are
expressible as `%.1f`. Content and look are split so that a caller replacing one
does not have to reimplement the other.

`ChartTrigger::Click` keeps the readout up until the next click elsewhere, which
is what a reader on a touchpad or a touchscreen needs.

**Every chart here has a readout**, and the same one: the panel, its placement
and its flipping away from an edge are written once, and what differs per chart
is only which mark the pointer found and what there is to say about it. Each
options struct carries a `tooltip`, so it is customised — or turned off with
`ChartTrigger::None` — in the same place whatever is being drawn. `None` still
reports `hoveredIndex` and still highlights the mark: it means "no panel", not
"tell me nothing", which is what a caller drawing its own readout elsewhere on
the page needs.

## Line and area

```cpp
ChartResult lineChart(ui, input, id, const std::vector<Series>&, const ChartOptions& = {});
ChartResult lineChart(ui, input, id, const std::vector<Series>&, ChartView&,
                      const ChartOptions& = {}, const ChartZoom& = {});
```

```cpp
struct ChartOptions {
    Scale scale{0.0, 0.0};                  // fixed bounds; autoScale takes them from the data
    bool autoScale = true;
    std::optional<int> tickCount{};
    std::optional<float> axisWidth{};       // room for the value labels; 0 draws none
    float height = 120.0f;
    std::optional<bool> grid{};
    bool hover = true;
    std::string_view valueFormat = "%.0f";
    std::vector<std::string> categories{};
    ChartTooltip tooltip{};
};
```

An area chart is a line with `fillAlpha`. The pointer's x is turned back into a
*sample number*, which is what lets the readout name the right point.

### Zoom, pan and the brush

```cpp
struct ChartView {
    double from = 0.0, to = 1.0;
    struct Sweep { bool active; double from, to; } sweep{};   // a drag in progress
    void normalise(double minSpan = 0.01);
};

enum class ChartDrag { None, Select, Pan };
struct ChartZoom { bool wheel = false; bool wheelModifier = true;
                   ChartDrag drag = ChartDrag::Select; float dragThreshold = 4.0f;
                   double minSpan = 0.02; };

bool chartBrush(ui, input, id, const std::vector<Series>&, ChartView&, const BrushOptions& = {});
```

The view is **fractions of the whole**, not indices, so a live series appending
samples does not make the window creep. It is the caller's, which is what lets
two charts share one — panning a line chart pans the bars under it.

**Dragging inside the plot sweeps out a range and zooms to it**, in either
direction, with a rubber band drawn over the marks while the button is down.
That is the gesture a reader tries first: the range they want is one they can
already see, and pointing straight at it beats steering a window into place from
somewhere else. Holding Shift pans instead, so the second gesture is still on
the chart. A press that travels less than `dragThreshold` is a click and changes
nothing — a reader asking for the readout moves a pixel or two, and zooming
their chart to a sliver because of it would be a trap.

The anchor lives in `ChartView::sweep` rather than inside the chart, for the
same reason the view does: the chart remembers nothing between frames. Nothing
outside the gesture reads it.

`ChartZoom::wheel` is off by default and this is the important default: a chart
lives inside a page that scrolls, and one that takes the wheel whenever the
pointer is over it steals every scroll aimed past it. Even switched on it wants
Ctrl unless `wheelModifier` is turned off too — the same bargain embedded maps
settle on. Either way the wheel zooms **about the pointer**, so whatever the
reader is looking at stays under the cursor; zooming about the centre instead
makes the point of interest slide away and they chase it.

How a chart claims the wheel is worth knowing, because it is the same mechanism
a nested scroll view uses: a node takes the wheel by declaring
`Overflow::Scroll`, and the pointer resolves the *innermost* one under it. A
chart wanting Ctrl claims it only while Ctrl is actually down — claiming it
permanently would leave a hole in the middle of the page, where the reader rolls
past the chart and nothing moves. The claim lands a frame after the key goes
down, which is nothing next to how long a modifier is held.

### The toolbar

```cpp
bool chartToolbar(ui, input, id, ChartView&, const ChartToolbarOptions& = {});
```

Zoom in, zoom out, and put it back — the buttons ApexCharts puts in the corner
of a chart, as a row of ordinary ones over the caller's own view. It is a
component rather than an option on the chart because it has no idea which chart
it belongs to: one row can drive a whole group the same way one view already
zooms a price and its volume together, and a caller who wants it somewhere other
than the top right simply puts it there.

The buttons zoom about the **middle**, where the wheel zooms about the pointer.
There is no pointer in a button press — the reader is looking at the chart, not
at where their cursor is resting — and the middle is the only part of the window
they can be sure keeps. Reset is disabled while the view is already whole: a
button that does nothing is worse than one that is not there.

`normalise` slides a window that runs off an end rather than squashing it.
Clamping each end independently turns a pan towards the start of the data into a
silent zoom out, and the reader's window grows every time they reach the edge.

`chartBrush` draws the whole series with the current view as a window over it,
dimming what is *outside* — the part the reader cannot see is the part that
needs explaining. That context, not the zooming, is what a brush earns its keep
with, and it is why the strip is optional: the plot zooms on its own.

### Charts that work together

```cpp
struct ChartLink { int index = -1; std::string source; float x = 0.0f; };
// ChartOptions, BarChartOptions and CandlestickOptions each carry:
ChartLink* link = nullptr;
```

This is ApexCharts' `chart.group`, split in two. There, one group string syncs
the crosshair, the tooltip, the zoom and the pan together; here the crosshair
and the readout travel on a `ChartLink` and the window travels on a shared
`ChartView`, because they are separable and worth separating — two charts over
the same samples but different scales want one crosshair and their own windows,
and a chart and its brush want the opposite.

Two charts stacked over one axis — a price and its volume, a load and its
frequency — are one instrument, and a reader pointing at a moment in either
means that moment in both. Give them the same `ChartLink` and they behave like
one: whichever chart the pointer is over publishes the sample and names itself
in `source`, and the others draw their own crosshair and their own readout at
that index.

```cpp
ChartLink crosshair;   // the caller's, like a ChartView
candlestickChart(ui, input, "price", candles, {.categories = times, .link = &crosshair});
barChart(ui, input, "volume", volume, {.categories = times, .link = &crosshair});
```

Shared by **index, not by pixel**, which is the only thing that survives two
charts of different widths, one with an axis and one without. That is also the
limit of it: a group only means something over charts indexing the same
samples, so `lineChart`, `barChart` and `candlestickChart` carry the field and
nothing else does. A donut's third wedge and a scatter's third dot have nothing
to do with each other.

A chart built *before* the one being pointed at follows one frame late, which at
sixty frames a second is not something a reader can see. A windowed chart
publishes in whole-series samples and says nothing when the group points at a
sample it has scrolled past.

**The crosshair is one line.** Every chart that indexes its samples draws the
same 1px rule in the same token through the mark, so two of them stacked read as
a single crosshair rather than as two highlights that agree. A bar or a candle
shades its whole slot as well, because a category is a *width* and the reader is
pointing at all of it; that is ApexCharts' `crosshairs.width: 'barWidth'`, and
the rule down the middle is its `width: 1`. Both are drawn behind the marks, as
`crosshairs.position: 'back'`.

### Two panes of one chart

A price over its volume is not two charts. It is one instrument drawn in two
panes, and it becomes one by three ordinary things rather than by a component
for it:

```cpp
ChartLink crosshair;   // one mark
ChartView view;        // one window
Style stack;
stack.direction = Direction::Column;
stack.gap = 0.0f;      // one plot area
auto panes = ui.scope(stack);
candlestickChart(ui, input, "price", candles, view, {.link = &crosshair, ...});
barChart(ui, input, "volume", volume, view, {.link = &crosshair, ...});
```

**The gap is the whole trick, and it has to be zero.** A component paints inside
the box it was given and stops at its edge, so any space between the panes is
space the crosshair cannot cross and a swept range cannot fill — and a selection
that breaks in the middle reads as two selections. With the panes touching, each
draws to its own edge and the two edges are the same line: the crosshair runs
through, the band runs through, and the only thing between them is the lower
pane's own top gridline, which is the pane separator every trading screen has.

Nothing else is needed. There is no bridging to do, no overlay to draw and no
geometry for the container to work out, which is why there is no `chartStack`
component here — it would own a column with a gap of zero and nothing else.

### Zooming a group

`barChart` and `candlestickChart` take the same `ChartView` overload the line
chart does:

```cpp
ChartResult barChart(ui, input, id, const std::vector<Series>&, ChartView&,
                     const BarChartOptions& = {}, const ChartZoom& = {});
ChartResult candlestickChart(ui, input, id, const std::vector<Candle>&, ChartView&,
                             const CandlestickOptions& = {}, const ChartZoom& = {});
```

Hand two charts the same view and they are looking at one window: sweep a range
on either and both zoom, shift-drag either and both pan. Nothing coordinates
them — there is only one window, and they are both drawing it.

The sweep in progress records which plot the press landed on. That matters the
moment a view is shared and not before: the second chart is handed a gesture it
did not start, and without a name on it, "a sweep is running and the pointer is
not on me" reads as "the reader let go" — which committed the zoom on the frame
the drag began. Every chart sharing the view still *draws* the band, so the
selection is visible across the group while it is being made.

### The key

```cpp
struct ChartLegend {
    bool show = true;
    int* focused = nullptr;      // click to single a series out
    float dimAlpha = 0.22f;
    float height = 24.0f;
};
// ChartOptions, BarChartOptions, ScatterOptions and RadarOptions each carry one.
```

Shown by default, which is the one defensible default: a chart with two series
and no key is a chart whose colours mean nothing until the reader finds the
sentence that explains them. It is skipped when no series carries a name, so a
single unnamed line costs nothing and keeps the node it always had.

Clicking an entry singles that series out and dims the rest — the gesture the
donut's legend already had, and the one a reader tries when six lines have
become a thicket. Clicking it again puts everything back. That needs somewhere
to remember which one, so `focused` is a pointer to the caller's own `int`: null
leaves the legend a key and nothing more, and the entries then do not light up
under the pointer, because a key that does should be promising something.

## Bars, columns and lollipops

```cpp
ChartResult barChart(ui, input, id, const std::vector<Series>&, const BarChartOptions& = {});
```

One function rather than four, because they differ only in how a value becomes a
rectangle: `horizontal` transposes the axes, `grouping` (`Grouped` | `Stacked`)
changes where each bar starts, and `shape` (`Bar` | `Lollipop`) draws a stem
instead of a body. Four functions would have meant four copies of the scale, the
axis, the grid and the hit testing.

A lollipop is the same information with most of the ink removed, which is the
point: with many categories and small differences between them, a row of wide
filled bars is mostly a solid block and the differences live in the top few
pixels.

Two decisions worth keeping:

- **The hit target is the slot, not the bar.** A reader pointing anywhere in a
  category means that category, including the empty space above a short bar.
- **Only the growing end is rounded.** Rounding the pair at the baseline lifts
  the bar off its own axis, which is a small lie a chart cannot afford.

## Scatter and bubbles

```cpp
ScatterResult scatterChart(ui, input, id, const std::vector<PointSeries>&,
                           const ScatterOptions& = {});
```

The first chart here with a real **x scale**. Every other one indexes its samples
along the bottom — sample 0 at the left, sample n at the right, evenly spaced
whatever their x values are — which is right for a series over time and wrong for
a correlation, where the question is where the points sit against each other on
*both* axes.

A point's `weight` becomes the dot's **area**, not its radius: mapping a value to
the radius makes a doubled value look four times bigger, which is the single most
common way a bubble chart lies.

Hit testing is by distance to the nearest dot rather than by column, because a
scatter has no columns — two points may share an x and mean different things.
`hitRadius` is how near the pointer has to get.

## Heatmap

```cpp
HeatmapResult heatmap(ui, input, id, const std::vector<std::vector<double>>& values,
                      const HeatmapOptions& = {});
```

`values` is row-major and may be ragged — a short row simply has fewer cells,
which is what a calendar's last week is.

One token at varying strength rather than a rainbow: hue carries no order, so a
multi-hue scale sends the reader back to the legend for every cell, while "more
of the same colour" needs no legend at all. The shade is quantised into `steps`
(five by default) for the same reason — cells are compared by matching them, and
two cells three percent apart on a continuous ramp are the same colour at a
glance. Empty slots are drawn, because a grid with holes in it should still read
as a grid.

## Candlesticks

```cpp
ChartResult candlestickChart(ui, input, id, const std::vector<Candle>&,
                             const CandlestickOptions& = {});

struct Candle { double open, high, low, close; bool rising() const; };
```

The one chart here whose scale deliberately does **not** reach zero. Every other
chart widens to include the baseline, because a bar that does not start at zero
misstates the ratio between two bars; a price has no such baseline, and forcing
one on a stock trading between 180 and 190 squeezes the year into the top five
percent of the plot.

`rising`/`falling` default to the theme's added and removed colours, which are
already the two a reader of this application associates with up and down.
`hollowRising` is the other common convention: on a dense chart the hollow
bodies recede and the falling ones stand out.

`Candle::top()` and `bottom()` guard against a feed that has `high` and `low` the
wrong way round, which is common enough to be worth absorbing here.

## Donut

```cpp
DonutResult donutChart(ui, input, id, const std::vector<Slice>&, DonutState&,
                       const DonutOptions& = {});
```

A donut, or a pie when `thickness` is 1. Each wedge is two arcs and two lines,
and an arc is cubics — nothing about it is a special case in the painter, which
is the proof that the canvas node carries more than polylines.

`startAngle` is -90 by default, which puts the first wedge at twelve o'clock
where every reader expects a donut to begin. The hovered wedge grows outward, a
*focused* one — clicked, and remembered in `DonutState` — pulls out further and
dims the rest. The legend scrolls once it passes `legendMaxHeight`, so a chart
with forty contributors does not grow taller than the ring beside it.

Hit testing is by angle and radius, so the result names the contributor rather
than the nearest bounding box.

## Radar

```cpp
ChartResult radarChart(ui, input, id, const std::vector<Series>&, const RadarOptions& = {});

enum class RadarGrid { Web, Polygon, None };
```

One value per axis, per series, on spokes around a common centre, starting at
twelve o'clock and going clockwise. The chart for a **profile** — a handful of
measures that belong together and are compared as a shape rather than read off
one at a time. It falls apart past about eight axes, where the polygon stops
being a shape and becomes a scribble.

The three shapes it comes in are one function and two options:

```cpp
radarChart(ui, input, "basic", {one}, {.categories = axes});                      // basic
radarChart(ui, input, "both", {one, other}, {.categories = axes});                // multiple series
radarChart(ui, input, "filled", {one}, {.categories = axes,                       // polygon fill
                                        .grid = RadarGrid::Polygon});
```

`RadarGrid::Web` draws the rings and spokes as hairlines — the mesh a value is
traced along, which is what two series being compared need. `Polygon` shades the
bands between the rings in alternating steps and draws no ring lines at all,
which reads as a target rather than as graph paper and suits one series being
looked at.

The scale always starts at **zero**, whatever the data says. A radar is read as
an area, and an area from a floating baseline is the same lie a truncated bar
tells, told about every axis at once — two profiles a hair apart become one
enormous and one tiny. Unlike a line, a radar is **filled** by default: a line is
read along and shading buries the one underneath, while an unfilled radar is a
wire outline the eye has to close for itself.

Hit testing is by *angle*, so a reader pointing anywhere along a spoke means that
axis — the rule the bars use for a category, and the only one that works near the
centre where every vertex sits on top of every other. The readout lists every
series at that axis, which is the whole reason two profiles share one web.

## Still to build

Range bars; a **time scale** and a **band scale**, since every chart but the
scatter indexes its samples rather than placing them; axis titles; and a shared
legend component. Each is geometry over the same primitive rather than
new machinery, which is what makes "more chart types" a matter of work rather
than of design.
