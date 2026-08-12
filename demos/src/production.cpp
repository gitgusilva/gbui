// Kaizen — the line monitor that hangs over a shop floor.
//
// OEE is one number made of three, and the whole screen exists to let a
// supervisor see which of the three is losing them the shift: availability
// (the line stopped), performance (it ran slow) or quality (it made scrap).
// So the four dials come first, the hour-by-hour state bars say *when*, the
// Pareto says *why*, and the defect grid says *where*.
//
// The two charts here are the ones a dashboard rarely reaches for and a
// factory always does: a stacked bar over time, and a heatmap that turns a
// table of numbers nobody reads into a shape anybody can.

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

struct StationRow {
    std::string_view code;
    std::string_view name;
    std::string_view state;
    double cycle;   ///< seconds
    double target;  ///< seconds
    int produced;
    int scrap;
    kit::Tone tone;
};

class Production final : public Demo {
public:
    NodeId build(Frame& frame) override;

private:
    void stateBars(Ui& ui, const Interaction& input);
    void pareto(Ui& ui, const Interaction& input);
    void defects(Ui& ui, const Interaction& input);
    void stations(Ui& ui, const Interaction& input);

    kit::Rolling rate_{40, 780.0, 1'020.0, 2.1f, 0.5f};
    kit::Rolling oeeTrend_{40, 62.0, 88.0, 5.8f, 1.0f};

    ScrollState page_{};
    TableState stationTable_{};
    std::size_t shift_ = 1;
    bool andon_ = false;

    // Minutes per hour in each state, eight hours of a shift. Stacked, so the
    // three series always sum to sixty and the bars are the same height —
    // which is what makes "how much of that hour was down" readable.
    std::vector<double> running_ = {58, 60, 47, 60, 52, 60, 41, 55};
    std::vector<double> idle_ = {2, 0, 6, 0, 3, 0, 9, 4};
    std::vector<double> down_ = {0, 0, 7, 0, 5, 0, 10, 1};

    // Downtime causes, biggest first — a Pareto is only a Pareto when sorted.
    std::vector<double> causes_ = {41.0, 26.0, 18.0, 11.0, 8.0, 5.0, 3.0};

    // Defects by station and hour. Ragged is allowed; this one is not.
    std::vector<std::vector<double>> defectGrid_ = {
        {1, 0, 4, 0, 2, 1, 6, 2}, {0, 0, 1, 0, 0, 0, 2, 0}, {3, 2, 9, 1, 4, 2, 12, 5},
        {0, 1, 2, 0, 1, 0, 3, 1}, {2, 1, 5, 2, 3, 1, 7, 3}, {0, 0, 0, 0, 1, 0, 1, 0},
    };

    std::array<StationRow, 6> stations_ = {{
        {"OP-10", "Frame load", "RUNNING", 41.2, 42.0, 1'184, 16, kit::Tone::Ok},
        {"OP-20", "Weld cell A", "RUNNING", 43.9, 42.0, 1'171, 3, kit::Tone::Warn},
        {"OP-30", "Weld cell B", "DOWN · fixture", 0.0, 42.0, 1'042, 38, kit::Tone::Alarm},
        {"OP-40", "Sealant", "RUNNING", 40.8, 42.0, 1'168, 8, kit::Tone::Ok},
        {"OP-50", "Inspection", "RUNNING", 39.6, 42.0, 1'160, 21, kit::Tone::Warn},
        {"OP-60", "Pack out", "STARVED", 44.5, 42.0, 1'149, 2, kit::Tone::Warn},
    }};
};

constexpr std::array<std::string_view, 3> kShifts = {"A · 06-14", "B · 14-22", "C · 22-06"};

void Production::stateBars(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(ui, {.title = "LINE STATE BY HOUR",
                                    .note = "minutes · stacked",
                                    .grow = 1.4f,
                                    .minWidth = 320.0f});
    const std::vector<Series> series = {
        {.name = "Running", .values = running_, .color = Token::Added},
        {.name = "Idle", .values = idle_, .color = Token::Modified},
        {.name = "Down", .values = down_, .color = Token::Removed},
    };
    barChart(ui, input, "production.state", series,
             {.height = 178.0f,
              .valueFormat = "%.0f min",
              .grouping = BarGrouping::Stacked,
              .categoryPadding = 0.34f,
              .categories = {"06", "07", "08", "09", "10", "11", "12", "13"},
              .categoryAxis = 18.0f});
}

void Production::pareto(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(
        ui, {.title = "DOWNTIME PARETO", .note = "minutes lost", .grow = 1.0f, .minWidth = 260.0f});
    const std::vector<Series> series = {
        {.name = "Minutes", .values = causes_, .color = Token::Removed}};
    barChart(ui, input, "production.pareto", series,
             {.height = 178.0f,
              .valueFormat = "%.0f min",
              // Horizontal, because the cause names are sentences and a
              // vertical axis has nowhere to put them. Lollipops rather than
              // bars: with seven categories close in value, a row of wide
              // filled bars is a solid block and the differences live in the
              // last few pixels.
              .horizontal = true,
              .shape = BarShape::Lollipop,
              .categories = {"Fixture fault", "Tool change", "Material starve", "Weld reject",
                             "Operator break", "Program load", "Sensor fault"},
              .categoryAxis = 104.0f});
}

void Production::defects(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(ui, {.title = "DEFECTS BY STATION AND HOUR",
                                    .note = "hover a cell",
                                    .grow = 1.0f,
                                    .minWidth = 300.0f});
    heatmap(ui, input, "production.defects", defectGrid_,
            {.rows = {"OP-10", "OP-20", "OP-30", "OP-40", "OP-50", "OP-60"},
             .columns = {"06", "07", "08", "09", "10", "11", "12", "13"},
             .steps = 5,
             .color = Token::Removed,
             // Without a cell size the grid shares out the width, and eight
             // wide columns make six rows taller than the card they sit in.
             .cellSize = 30.0f,
             .rowLabels = 44.0f,
             .valueFormat = "%.0f"});
}

void Production::stations(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(ui, {.title = "STATIONS",
                                    .note = "cycle against takt",
                                    .gap = 0.0f,
                                    .grow = 1.0f,
                                    .height = 288.0f,
                                    .minWidth = 380.0f});

    const std::vector<Column> columns = {
        {.title = "Station", .width = 1.6f},
        {.title = "State", .sizing = ColumnSize::FitContent, .fitSample = "DOWN · fixture"},
        {.title = "Cycle",
         .sizing = ColumnSize::FitContent,
         .fitSample = "00.0 sMM",
         .align = TextAlign::End},
        {.title = "Takt", .width = 1.2f},
        {.title = "Made",
         .sizing = ColumnSize::FitContent,
         .fitSample = "9 999MM",
         .align = TextAlign::End},
        {.title = "Scrap",
         .sizing = ColumnSize::FitContent,
         .fitSample = "999MM",
         .align = TextAlign::End},
    };

    table(ui, input, "production.stations", columns, stations_.size(), stationTable_,
          [&](Ui& cellUi, std::size_t row, std::size_t column) {
              const StationRow& station = stations_[row];
              switch (column) {
                  case 0:
                      kit::statusDot(cellUi, station.tone);
                      kit::hspace(cellUi, 8.0f);
                      text(cellUi, station.code,
                           {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.0f});
                      kit::hspace(cellUi, 8.0f);
                      text(
                          cellUi, station.name,
                          {.color = Token::TextStrong, .weight = FontWeight::Medium, .grow = 1.0f});
                      break;
                  case 1:
                      text(cellUi, station.state,
                           {.color = kit::toneToken(station.tone), .size = 11.0f, .grow = 1.0f});
                      break;
                  case 2:
                      text(cellUi, kit::format("%.1f s", station.cycle),
                           {.color = station.cycle > station.target ? Token::Removed : Token::Text,
                            .role = FontRole::Mono,
                            .align = TextAlign::End,
                            .grow = 1.0f});
                      break;
                  case 3:
                      // The bar in the cell is the point of the column: a number
                      // beside a number needs arithmetic, and a bar against a
                      // line needs a glance.
                      kit::meter(cellUi, {.label = "",
                                          .value = station.cycle,
                                          .maximum = station.target * 1.3,
                                          .tone = station.cycle > station.target ? kit::Tone::Alarm
                                                                                 : kit::Tone::Ok,
                                          .showValue = false,
                                          .height = 6.0f,
                                          .grow = 1.0f});
                      break;
                  case 4:
                      text(cellUi, kit::format("%.0f", static_cast<double>(station.produced)),
                           {.color = Token::Text,
                            .role = FontRole::Mono,
                            .align = TextAlign::End,
                            .grow = 1.0f});
                      break;
                  default:
                      text(cellUi, kit::format("%.0f", static_cast<double>(station.scrap)),
                           {.color = station.scrap > 20 ? Token::Removed : Token::TextMuted,
                            .role = FontRole::Mono,
                            .align = TextAlign::End,
                            .grow = 1.0f});
                      break;
              }
          },
          {.rowHeight = 34.0f, .rowLines = true});
}

NodeId Production::build(Frame& frame) {
    Ui& ui = frame.ui;
    const Interaction& input = frame.input;

    rate_.advance(frame.time);
    oeeTrend_.advance(frame.time);

    const double availability = 91.4;
    const double performance = 88.2;
    const double quality = 97.6;
    const double oee = availability * performance * quality / 10'000.0;

    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.begin(window);

    {
        auto header = kit::beginHeader(ui, Icon::Package, "Kaizen MES · Line 4",
                                       "Assembly · SKU 88-4120 Hinge LH · takt 42.0 s");
        spacer(ui);
        {
            Style strip;
            strip.width = 300.0f;
            strip.shrink = 0.0f;
            auto stripScope = ui.begin(strip);
            std::vector<TabItem> items;
            for (std::string_view label : kShifts) items.push_back({.label = label});
            if (const auto chosen = tabs(ui, input, "production.shift", items, shift_,
                                         {.thickness = 30.0f, .rule = false})) {
                shift_ = *chosen;
            }
        }
        {
            Style toggle;
            toggle.direction = Direction::Row;
            toggle.align = Align::Center;
            toggle.gap = 8.0f;
            toggle.shrink = 0.0f;
            auto toggleScope = ui.begin(toggle);
            text(ui, "ANDON", {.color = Token::TextMuted, .size = 10.5f});
            if (switchToggle(ui, input, "production.andon", andon_)) andon_ = !andon_;
        }
        button(ui, input, "CALL SUPPORT",
               {.variant = andon_ ? ButtonVariant::Danger : ButtonVariant::Secondary,
                .leading = Icon::CircleAlert,
                .id = "production.support"});
    }
    kit::rule(ui, Direction::Column);

    {
        Style body;
        body.direction = Direction::Column;
        body.grow = 1.0f;
        body.basis = 0.0f;
        auto bodyScope = ui.begin(body);
        auto page = beginScroll(ui, input, "production.page", page_,
                                {.padding = Edges::all(12.0f), .gap = 12.0f});

        {
            // The four dials and the counters beside them: the whole shift in
            // one row, and everything below it is the explanation.
            auto card = kit::beginCard(ui, {.title = "OVERALL EQUIPMENT EFFECTIVENESS",
                                            .note = "shift to date · target 85 %",
                                            .direction = Direction::Row,
                                            .gap = 10.0f});
            const auto dial = [&](double value, std::string_view label, kit::Tone tone,
                                  float size) {
                Style cell;
                cell.direction = Direction::Column;
                cell.align = Align::Center;
                cell.justify = Justify::Center;
                cell.gap = 4.0f;
                cell.shrink = 0.0f;
                auto cellScope = ui.begin(cell);
                kit::gauge(ui, {.value = value,
                                .label = label,
                                .unit = "%",
                                .tone = tone,
                                .size = size,
                                .thickness = size * 0.09f,
                                .valueFormat = "%.1f"});
            };
            dial(oee, "OEE", kit::toneBelow(oee, 85.0, 75.0), 148.0f);
            dial(availability, "AVAILABILITY", kit::toneBelow(availability, 92.0, 85.0), 112.0f);
            dial(performance, "PERFORMANCE", kit::toneBelow(performance, 92.0, 85.0), 112.0f);
            dial(quality, "QUALITY", kit::toneBelow(quality, 99.0, 97.0), 112.0f);

            Style grid;
            grid.direction = Direction::Row;
            grid.wrap = true;
            grid.gap = 10.0f;
            grid.crossGap = 10.0f;
            grid.grow = 1.0f;
            grid.basis = 0.0f;
            grid.minWidth = 0.0f;
            grid.align = Align::Center;
            auto gridScope = ui.begin(grid);
            kit::statTile(ui, {.label = "RATE",
                               .value = kit::format("%.0f", rate_.latest()),
                               .unit = "pcs/h",
                               .tone = kit::toneBelow(rate_.latest(), 860.0, 800.0),
                               .history = &rate_.values(),
                               .valueSize = 22.0f});
            kit::statTile(ui, {.label = "GOOD PARTS",
                               .value = "6 874",
                               .unit = "of 7 200",
                               .tone = kit::Tone::Ok,
                               .valueSize = 22.0f});
            kit::statTile(ui, {.label = "SCRAP",
                               .value = "88",
                               .unit = "1.26%",
                               .tone = kit::Tone::Warn,
                               .valueSize = 22.0f});
            kit::statTile(ui, {.label = "DOWNTIME",
                               .value = kit::duration(23.0),
                               .tone = kit::Tone::Alarm,
                               .history = &oeeTrend_.values(),
                               .valueSize = 22.0f});
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.height = 250.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.begin(row);
            stateBars(ui, input);
            pareto(ui, input);
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.begin(row);
            stations(ui, input);
            defects(ui, input);
        }
    }

    kit::rule(ui, Direction::Column);
    {
        auto bar = kit::beginStatusBar(ui);
        kit::statusItem(ui, Icon::Terminal, "OPC UA · 6 stations connected", kit::Tone::Ok);
        kit::statusItem(ui, Icon::ClockFading, kShifts[shift_]);
        kit::statusItem(ui, Icon::Package, "order 4471-B · 7 200 pcs");
        spacer(ui);
        kit::statusItem(ui, Icon::CircleAlert, "OP-30 down 11 min", kit::Tone::Alarm);
        text(ui, "KAIZEN MES 3.1", {.color = Token::TextMuted, .size = 10.5f});
    }
    return root.id();
}

}  // namespace

DemoInfo productionDemo() {
    return {.id = "production",
            .title = "Kaizen Line Monitor",
            .sector = "Manufacturing · MES / OEE",
            .summary =
                "A shop-floor line monitor: OEE broken into its three factors, hourly "
                "state bars, a downtime Pareto and a defect heatmap.",
            .highlights = {"Stacked bars", "Horizontal lollipops", "Defect heatmap",
                           "Meters inside table cells"},
            .design = {1400.0f, 860.0f},
            .palette = Palette::Dark,
            .create = [] { return std::unique_ptr<Demo>(new Production()); }};
}

}  // namespace gbui::demos
