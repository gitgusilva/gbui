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
struct ChartView { double from = 0.0, to = 1.0; void normalise(double minSpan = 0.01); };
struct ChartZoom { bool wheel = false; bool wheelModifier = true; bool drag = true;
                   double minSpan = 0.02; };

bool chartBrush(ui, input, id, const std::vector<Series>&, ChartView&, const BrushOptions& = {});
```

The view is **fractions of the whole**, not indices, so a live series appending
samples does not make the window creep. It is the caller's, which is what lets
two charts share one — panning a line chart pans the bars under it.

`ChartZoom::wheel` is off by default and this is the important default: a chart
lives inside a page that scrolls, and one that takes the wheel whenever the
pointer is over it steals every scroll aimed past it. Even switched on it wants
Ctrl unless `wheelModifier` is turned off too — the same bargain embedded maps
settle on.

`normalise` slides a window that runs off an end rather than squashing it.
Clamping each end independently turns a pan towards the start of the data into a
silent zoom out, and the reader's window grows every time they reach the edge.

`chartBrush` draws the whole series with the current view as a window over it,
dimming what is *outside* — the part the reader cannot see is the part that
needs explaining. That context, not the zooming, is what a brush earns its keep
with.

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

## Still to build

Radar and range bars; a **time scale** and a **band scale**, since every chart
but the scatter indexes its samples rather than placing them; axis titles; and a
shared legend component. Each is geometry over the same primitive rather than
new machinery, which is what makes "more chart types" a matter of work rather
than of design.
