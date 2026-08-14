// Voltway — a grid control desk: what the network is drawing, what is
// generating it, and what the next hour of it costs.
//
// The screen that makes the case for the candlestick chart, which is in the
// toolkit for markets and is here for the one every energy control room
// watches: a day-ahead price curve where the *range* inside each hour matters
// as much as where it settled. It is also the one chart whose scale must not
// reach zero — see the note in chart.hpp.

#include <array>
#include <string>
#include <vector>

#include "gbui/widgets/chart.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/containers.hpp"
#include "gbui/widgets/controls.hpp"

#include "kit.hpp"
#include "registry.hpp"

namespace gbui::demos {
namespace {

struct Feeder {
    std::string_view code;
    std::string_view name;
    double current;  ///< amps
    double voltage;  ///< kilovolts
    double loading;  ///< percent of rating
    std::string_view breaker;
    kit::Tone tone;
};

struct Interconnector {
    std::string_view name;
    double flow;  ///< megawatts, signed: positive is import
    double capacity;
};

class Grid final : public Demo {
public:
    NodeId build(Frame& frame) override;

private:
    void balance(Ui& ui, const Interaction& input);
    void prices(Ui& ui, const Interaction& input);
    void mix(Ui& ui, const Interaction& input);
    void feeders(Ui& ui, const Interaction& input);
    void ties(Ui& ui);

    kit::Rolling demand_{56, 2'840.0, 3'620.0, 1.4f, 0.5f};
    kit::Rolling generation_{56, 2'800.0, 3'680.0, 4.9f, 0.5f};
    kit::Rolling frequency_{56, 49.94, 50.06, 7.3f, 0.3f};
    kit::Rolling reserve_{56, 210.0, 480.0, 3.1f, 0.9f};
    kit::Rolling powerFactor_{56, 0.94, 0.995, 8.4f, 1.1f};

    ChartView view_{};
    DonutState mix_{};
    TableState feederTable_{};
    ScrollState page_{};
    std::size_t horizon_ = 0;

    // Day-ahead settlements. Fixed data: a market that repriced itself every
    // second would be telling the reader something untrue about markets.
    std::vector<Candle> spot_ = {
        {58.2, 61.0, 56.4, 60.1}, {60.1, 63.8, 59.7, 63.0}, {63.0, 71.2, 62.4, 69.9},
        {69.9, 74.0, 66.1, 67.2}, {67.2, 68.9, 61.0, 62.4}, {62.4, 64.1, 55.8, 57.0},
        {57.0, 59.3, 51.2, 52.6}, {52.6, 54.0, 44.9, 46.3}, {46.3, 49.8, 45.1, 49.2},
        {49.2, 58.6, 48.7, 57.9}, {57.9, 66.4, 57.1, 65.8}, {65.8, 82.3, 64.9, 80.7},
        {80.7, 94.1, 78.2, 88.4}, {88.4, 91.0, 76.5, 78.1}, {78.1, 79.9, 70.3, 71.6},
        {71.6, 73.2, 64.8, 66.0},
    };

    std::array<Feeder, 7> feeders_ = {{
        {"F-11", "Riverside industrial", 412.0, 33.1, 68.0, "CLOSED", kit::Tone::Ok},
        {"F-12", "Riverside residential", 288.0, 33.0, 47.0, "CLOSED", kit::Tone::Ok},
        {"F-21", "Northgate", 596.0, 32.8, 92.0, "CLOSED", kit::Tone::Alarm},
        {"F-22", "Northgate spur", 0.0, 32.9, 0.0, "OPEN", kit::Tone::Neutral},
        {"F-31", "Docks traction", 501.0, 33.2, 81.0, "CLOSED", kit::Tone::Warn},
        {"F-41", "Airport", 344.0, 33.1, 56.0, "CLOSED", kit::Tone::Ok},
        {"F-42", "Data park", 478.0, 33.0, 77.0, "CLOSED", kit::Tone::Warn},
    }};

    std::array<Interconnector, 4> ties_ = {{
        {"North tie · 400 kV", 620.0, 1'000.0},
        {"Coastal tie · 275 kV", -285.0, 700.0},
        {"Sub-sea HVDC", 410.0, 500.0},
        {"Municipal CHP", -96.0, 200.0},
    }};
};

constexpr std::array<std::string_view, 3> kHorizons = {"LIVE", "DAY", "WEEK"};

void Grid::balance(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "DEMAND AND GENERATION",
                                    .note = "MW · 30 s resolution",
                                    .grow = 1.5f,
                                    .minWidth = 340.0f});
    const std::vector<Series> series = {
        {.name = "Demand", .values = demand_.values(), .color = Token::Graph2},
        {.name = "Generation",
         .values = generation_.values(),
         .color = Token::Graph3,
         .fillAlpha = 0.0f},
    };
    // The unit belongs in the card's note, not in every axis label: "4000 MW"
    // needs an axis wide enough to hold it, and the axis is where the room
    // comes from.
    // A fixed window rather than the data's own, and deliberately *not*
    // anchored at zero. A bar that does not start at zero misstates a ratio; a
    // line of a continuous quantity has no such duty, and a demand curve that
    // never leaves 2 800–3 700 MW spends four fifths of an axis rooted at zero
    // saying nothing.
    lineChart(ui, input, "grid.balance", series, view_,
              {.scale = {2'500.0, 4'000.0},
               .autoScale = false,
               .axisWidth = 52.0f,
               .height = 190.0f,
               .valueFormat = "%.0f"},
              {.wheel = true, .drag = ChartDrag::Select});

    {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 16.0f;
        row.shrink = 0.0f;
        auto rowScope = ui.scope(row);
        const double imbalance = generation_.latest() - demand_.latest();
        text(ui, "SYSTEM IMBALANCE", {.color = Token::TextMuted, .size = 10.0f});
        text(ui, kit::format("%+.0f MW", imbalance),
             {.color =
                  kit::toneToken(std::fabs(imbalance) > 120.0 ? kit::Tone::Alarm : kit::Tone::Ok),
              .weight = FontWeight::SemiBold,
              .role = FontRole::Mono,
              .size = 13.0f});
        spacer(ui);
        kit::pill(ui, std::fabs(imbalance) > 120.0 ? "BALANCING ACTION" : "WITHIN TOLERANCE",
                  {.tone = std::fabs(imbalance) > 120.0 ? kit::Tone::Alarm : kit::Tone::Ok});
    }
}

void Grid::prices(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "DAY-AHEAD SETTLEMENT",
                                    .note = "EUR/MWh · hourly",
                                    .grow = 1.0f,
                                    .minWidth = 300.0f});
    candlestickChart(ui, input, "grid.spot", spot_,
                     {.height = 190.0f,
                      .valueFormat = "%.1f",
                      .categories = {"08", "09", "10", "11", "12", "13", "14", "15", "16", "17",
                                     "18", "19", "20", "21", "22", "23"},
                      .categoryAxis = 18.0f});
}

void Grid::mix(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "GENERATION MIX", .width = 306.0f});
    const std::vector<Slice> slices = {
        {.name = "Wind", .value = 1'180.0, .color = Token::Graph6},
        {.name = "Nuclear", .value = 940.0, .color = Token::Graph4},
        {.name = "Gas CCGT", .value = 720.0, .color = Token::Graph2},
        {.name = "Solar", .value = 410.0, .color = Token::Graph7},
        {.name = "Hydro", .value = 260.0, .color = Token::Graph1},
        {.name = "Import", .value = 170.0, .color = Token::Graph5},
    };
    donutChart(ui, input, "grid.mix", slices, mix_, {.size = 150.0f, .legendMaxHeight = 160.0f});
}

void Grid::feeders(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "FEEDERS · SUBSTATION MARSH LANE",
                                    .note = "33 kV busbar A",
                                    .gap = 0.0f,
                                    .grow = 1.0f,
                                    .height = 282.0f,
                                    .minWidth = 420.0f});

    const std::vector<Column> columns = {
        {.title = "Feeder", .width = 1.8f},
        {.title = "Breaker", .sizing = ColumnSize::FitContent, .fitSample = "CLOSED"},
        {.title = "Current",
         .sizing = ColumnSize::FitContent,
         .fitSample = "999 A",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End},
        {.title = "Voltage",
         .sizing = ColumnSize::FitContent,
         .fitSample = "99.9 kV",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End},
        {.title = "Loading", .width = 1.4f},
    };

    table(ui, input, "grid.feeders", columns, feeders_.size(), feederTable_,
          [&](Ui& cellUi, std::size_t row, std::size_t column) {
              const Feeder& feeder = feeders_[row];
              switch (column) {
                  case 0:
                      kit::statusDot(cellUi, feeder.tone);
                      kit::hspace(cellUi, 8.0f);
                      text(cellUi, feeder.code,
                           {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.0f});
                      kit::hspace(cellUi, 8.0f);
                      text(
                          cellUi, feeder.name,
                          {.color = Token::TextStrong, .weight = FontWeight::Medium, .grow = 1.0f});
                      break;
                  case 1:
                      badge(cellUi, feeder.breaker,
                            {.background = Token::BgOverlay,
                             .foreground =
                                 feeder.breaker == "OPEN" ? Token::Modified : Token::TextMuted});
                      break;
                  case 2:
                      text(cellUi, kit::format("%.0f A", feeder.current),
                           {.color = Token::Text,
                            .role = FontRole::Mono,
                            .align = TextAlign::End,
                            .grow = 1.0f});
                      break;
                  case 3:
                      text(cellUi, kit::format("%.1f kV", feeder.voltage),
                           {.color = Token::TextMuted,
                            .role = FontRole::Mono,
                            .align = TextAlign::End,
                            .grow = 1.0f});
                      break;
                  default:
                      kit::meter(cellUi, {.label = "",
                                          .value = feeder.loading,
                                          .unit = "%",
                                          .tone = kit::toneFor(feeder.loading, 75.0, 90.0),
                                          .height = 6.0f,
                                          .grow = 1.0f,
                                          .valueFormat = "%.0f"});
                      break;
              }
          },
          {.rowHeight = 32.0f, .rowLines = true});
}

void Grid::ties(Ui& ui) {
    auto card = kit::card(
        ui,
        {.title = "INTERCONNECTORS", .note = "positive is import", .gap = 12.0f, .width = 250.0f});
    for (const Interconnector& tie : ties_) {
        const double share = std::fabs(tie.flow) / tie.capacity * 100.0;
        kit::meter(ui, {.label = tie.name,
                        .value = share,
                        .unit = "%",
                        .tone = tie.flow >= 0.0 ? kit::Tone::Info : kit::Tone::Warn,
                        .valueFormat = "%.0f"});
        text(ui, kit::format("%+.0f MW", tie.flow),
             {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.5f});
    }
}

NodeId Grid::build(Frame& frame) {
    Ui& ui = frame.ui;
    const Interaction& input = frame.input;

    demand_.advance(frame.time);
    generation_.advance(frame.time);
    frequency_.advance(frame.time);
    reserve_.advance(frame.time);
    powerFactor_.advance(frame.time);

    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.scope(window);

    {
        auto header = kit::header(ui, Icon::Terminal, "Voltway Grid Operations",
                                       "Region North-East · control area NE-2 · UTC+1");
        spacer(ui);
        {
            Style strip;
            strip.width = 200.0f;
            strip.shrink = 0.0f;
            auto stripScope = ui.scope(strip);
            std::vector<TabItem> items;
            for (std::string_view label : kHorizons) items.push_back({.label = label});
            if (const auto chosen = tabs(ui, input, "grid.horizon", items, horizon_,
                                         {.thickness = 30.0f, .rule = false})) {
                horizon_ = *chosen;
            }
        }
        kit::pill(ui, "N-1 SECURE", {.tone = kit::Tone::Ok, .solid = true});
        button(ui, input, "DISPATCH",
               {.variant = ButtonVariant::Primary, .leading = Icon::Upload, .id = "grid.dispatch"});
    }
    kit::rule(ui, Direction::Column);

    {
        Style body;
        body.direction = Direction::Column;
        body.grow = 1.0f;
        body.basis = 0.0f;
        auto bodyScope = ui.scope(body);
        auto page = scrollArea(ui, input, "grid.page", page_,
                               {.padding = Edges::all(12.0f), .gap = 12.0f});

        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.scope(row);
            kit::statTile(ui, {.label = "SYSTEM DEMAND",
                               .value = kit::compact(demand_.latest()),
                               .unit = "MW",
                               .trend = kit::signedPercent(demand_.trend()),
                               .trendTone = kit::Tone::Neutral,
                               .tone = kit::Tone::Info,
                               .history = &demand_.values()});
            kit::statTile(ui,
                          {.label = "FREQUENCY",
                           .value = kit::format("%.3f", frequency_.latest()),
                           .unit = "Hz",
                           // Frequency is the one reading on this screen
                           // that is bad in *both* directions, so its tone
                           // comes from the distance to fifty rather than
                           // from a threshold.
                           .tone = kit::toneFor(std::fabs(frequency_.latest() - 50.0), 0.03, 0.05),
                           .history = &frequency_.values()});
            kit::statTile(ui, {.label = "SPINNING RESERVE",
                               .value = kit::format("%.0f", reserve_.latest()),
                               .unit = "MW",
                               .tone = kit::toneBelow(reserve_.latest(), 300.0, 240.0),
                               .history = &reserve_.values()});
            kit::statTile(ui, {.label = "POWER FACTOR",
                               .value = kit::format("%.3f", powerFactor_.latest()),
                               .tone = kit::toneBelow(powerFactor_.latest(), 0.96, 0.95),
                               .history = &powerFactor_.values()});
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.height = 288.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.scope(row);
            balance(ui, input);
            prices(ui, input);
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.scope(row);
            feeders(ui, input);
            mix(ui, input);
            ties(ui);
        }
    }

    kit::rule(ui, Direction::Column);
    {
        auto bar = kit::statusBar(ui);
        kit::statusItem(ui, Icon::RefreshCw, "SCADA telemetry 2 s", kit::Tone::Ok);
        kit::statusItem(ui, Icon::ClockFading, "state estimator converged 00:04 ago",
                        kit::Tone::Ok);
        kit::statusItem(ui, Icon::Archive, "PMU archive 30 d");
        spacer(ui);
        kit::statusItem(ui, Icon::CircleAlert, "F-21 at 92% rating", kit::Tone::Alarm);
        text(ui, "VOLTWAY EMS 11.2", {.color = Token::TextMuted, .size = 10.5f});
    }
    return root.id();
}

}  // namespace

DemoInfo gridDemo() {
    return {.id = "grid",
            .title = "Voltway Grid Operations",
            .sector = "Energy · Transmission / EMS",
            .summary =
                "A grid control desk: demand against generation, a day-ahead candlestick "
                "settlement, the generation mix and a live feeder table.",
            .highlights = {"Candlesticks", "Signed readings", "Feeder loading meters",
                           "Generation donut"},
            .tryThis =
                "Hover a candle in the day-ahead settlement, then click a wedge of the "
                "generation donut to single it out.",
            .design = {1440.0f, 840.0f},
            .palette = Palette::Dark,
            .create = [] { return std::unique_ptr<Demo>(new Grid()); }};
}

}  // namespace gbui::demos
