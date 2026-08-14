// The nine charts. Each one is `Ui::draw` underneath — geometry handed to a
// vector node — rather than a widget per mark, which is why a donut's wedges
// and a candlestick's wicks need nothing special from the painter.
//
// The data is fixed rather than simulated: a chart that repaints itself with
// different numbers every second demonstrates the animation, not the chart.
#include "catalog.hpp"

namespace gbui::demos::catalog {

void addChartExamples(std::vector<Example>& out) {
    out.push_back({"lineChart", [](Ui& ui, const Interaction& input, State& state) {
                       const std::vector<Series> series = {
                           {.name = "Booked", .values = state.series},
                           {.name = "Forecast", .values = state.other, .fillAlpha = 0.0f}};
                       lineChart(ui, input, "catalog.line", series, {.height = 160.0f});
                   }});

    out.push_back({"chartBrush", [](Ui& ui, const Interaction& input, State& state) {
                       const std::vector<Series> series = {
                           {.name = "Booked", .values = state.series}};
                       // The strip earns its place not by zooming — a drag
                       // inside the plot does that — but by saying where in the
                       // whole series the view currently is.
                       lineChart(ui, input, "catalog.brushed", series, state.view,
                                 {.height = 130.0f});
                       chartBrush(ui, input, "catalog.brush", series, state.view);
                   }});

    out.push_back({"chartToolbar", [](Ui& ui, const Interaction& input, State& state) {
                       const std::vector<Series> series = {
                           {.name = "Booked", .values = state.series}};
                       Style row;
                       row.direction = Direction::Row;
                       row.align = Align::Center;
                       row.gap = 8.0f;
                       row.shrink = 0.0f;
                       {
                           auto scope = ui.scope(row);
                           text(ui, "the same view the gestures move",
                                {.color = Token::TextMuted, .size = 11.0f, .grow = 1.0f});
                           chartToolbar(ui, input, "catalog.zoom", state.view);
                       }
                       lineChart(ui, input, "catalog.zoomed", series, state.view,
                                 {.height = 150.0f},
                                 {.wheel = true, .wheelModifier = false});
                   }});

    out.push_back({"barChart", [](Ui& ui, const Interaction& input, State&) {
                       const std::vector<std::string> hours = {"06", "07", "08", "09", "10"};
                       const std::vector<Series> series = {
                           {.name = "Running", .values = {58, 60, 47, 60, 52}},
                           {.name = "Down", .values = {2, 0, 13, 0, 8}}};
                       barChart(ui, input, "catalog.bars", series,
                                {.height = 130.0f,
                                 .valueFormat = "%.0f min",
                                 .grouping = BarGrouping::Stacked,
                                 .categories = hours});

                       // The same function, and the two options that make it a
                       // different chart: laid on its side because the names
                       // are sentences, and drawn as lollipops because seven
                       // categories close in value are a solid block as bars.
                       barChart(ui, input, "catalog.pareto", {series.back()},
                                {.height = 130.0f,
                                 .valueFormat = "%.0f min",
                                 .horizontal = true,
                                 .shape = BarShape::Lollipop,
                                 .categories = hours,
                                 .categoryAxis = 28.0f});
                   }});

    out.push_back({"donutChart", [](Ui& ui, const Interaction& input, State& state) {
                       const std::vector<Slice> slices = {{.name = "Direct", .value = 38.0},
                                                          {.name = "Partner", .value = 24.0},
                                                          {.name = "Self-serve", .value = 21.0},
                                                          {.name = "Referral", .value = 17.0}};
                       donutChart(ui, input, "catalog.donut", slices, state.donut);
                   }});

    out.push_back({"heatmap", [](Ui& ui, const Interaction& input, State&) {
                       const std::vector<std::vector<double>> grid = {
                           {1, 0, 4, 0, 2}, {0, 0, 1, 0, 0}, {3, 2, 9, 1, 4}, {0, 1, 2, 0, 1}};
                       // One token at varying strength rather than a rainbow:
                       // hue carries no order, so a reader would have to
                       // consult a legend for every cell.
                       heatmap(ui, input, "catalog.heatmap", grid,
                               {.rows = {"OP-10", "OP-20", "OP-30", "OP-40"},
                                .columns = {"06", "07", "08", "09", "10"},
                                .cellSize = 30.0f});
                   }});

    out.push_back({"scatterChart", [](Ui& ui, const Interaction& input, State&) {
                       // The only chart here with a real *x* scale: every other
                       // one spaces its samples evenly along the bottom, which
                       // is right for a series over time and wrong for a
                       // correlation.
                       const std::vector<PointSeries> series = {
                           {.name = "On time",
                            .points = {{12, -4, 6}, {45, 1, 9}, {63, -2, 22}, {88, 6, 31}},
                            .color = Token::Added},
                           {.name = "Late",
                            .points = {{120, 38, 28}, {142, 52, 19}, {166, 71, 34}},
                            .color = Token::Removed}};
                       scatterChart(ui, input, "catalog.scatter", series, {.height = 180.0f});
                   }});

    out.push_back({"candlestickChart", [](Ui& ui, const Interaction& input, State&) {
                       const std::vector<Candle> candles = {
                           {58.2, 61.0, 56.4, 60.1}, {60.1, 63.8, 59.7, 63.0},
                           {63.0, 71.2, 62.4, 69.9}, {69.9, 74.0, 66.1, 67.2},
                           {67.2, 68.9, 61.0, 62.4}, {62.4, 64.1, 55.8, 57.0}};
                       // The one chart whose scale deliberately does not reach
                       // zero: a price has no baseline, and forcing one hides
                       // the only thing it was drawn to show.
                       candlestickChart(ui, input, "catalog.candles", candles,
                                        {.height = 170.0f,
                                         .valueFormat = "%.1f",
                                         .categories = {"08", "09", "10", "11", "12", "13"}});
                   }});

    out.push_back({"radarChart", [](Ui& ui, const Interaction& input, State&) {
                       // Two of them, because the interesting thing about a
                       // radar is what it is *for*: two profiles on one web are
                       // read against each other, and one on its own is read as
                       // a shape. The grids differ for the same reason — a mesh
                       // to measure two series along, a target behind one.
                       const std::vector<std::string> axes = {"Speed",  "Memory", "Startup",
                                                              "Binary", "API",    "Docs"};
                       Style row;
                       row.direction = Direction::Row;
                       row.align = Align::Center;
                       row.gap = 4.0f;
                       auto scope = ui.scope(row);

                       const std::vector<Series> both = {
                           {.name = "gbui", .values = {92, 88, 95, 90, 74, 68}},
                           {.name = "Electron", .values = {41, 22, 35, 12, 88, 92}}};
                       radarChart(ui, input, "catalog.radar", both,
                                  {.categories = axes, .size = 250.0f});

                       radarChart(ui, input, "catalog.radar.filled", {both.front()},
                                  {.categories = axes,
                                   .size = 250.0f,
                                   .grid = RadarGrid::Polygon});
                       (void)scope;
                   }});
}

}  // namespace gbui::demos::catalog
