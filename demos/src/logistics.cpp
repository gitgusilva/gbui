// Portway — a warehouse and fleet control tower.
//
// The last screen in the set, and the one that stays light: a logistics desk
// is looked at in a lit office by someone who also has a phone in their hand,
// so it declares `Palette::Follow` and reads well either way.
//
// It carries the scatter, which is the one chart here with a real **x scale** —
// every other chart in the set spaces its samples evenly along the bottom. A
// delivery's lateness against its distance is exactly the question that needs
// two continuous axes and a third value in the size of the dot.

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

struct Vehicle {
    std::string_view plate;
    std::string_view driver;
    std::string_view route;
    std::string_view status;
    int stopsDone;
    int stopsTotal;
    double etaMinutes;  ///< signed: negative is ahead of schedule
    double fuel;        ///< percent
    kit::Tone tone;
};

struct Dock {
    std::string_view bay;
    std::string_view carrier;
    std::string_view window;
    std::string_view state;
    kit::Tone tone;
};

class Logistics final : public Demo {
public:
    NodeId build(Frame& frame) override;

private:
    void punctuality(Ui& ui, const Interaction& input);
    void zones(Ui& ui);
    void docks(Ui& ui, const Interaction& input);
    void fleet(Ui& ui, const Interaction& input);

    kit::Rolling onTime_{44, 88.0, 97.5, 2.4f, 1.0f};
    kit::Rolling throughput_{44, 220.0, 410.0, 6.1f, 0.6f};
    kit::Rolling dwell_{44, 22.0, 61.0, 9.2f, 0.8f};
    kit::Rolling backlog_{44, 120.0, 640.0, 3.8f, 0.7f};

    ScrollState page_{};
    ScrollState dockScroll_{};
    TableState fleetTable_{};
    std::size_t view_ = 0;

    // Deliveries: x is distance in kilometres, y is minutes late, and the
    // weight is the number of pallets — so a big dot far right and high up is
    // the one that cost the most.
    std::vector<Point> onSchedule_ = {
        {12, -4, 6},  {28, 2, 14},  {45, 1, 9}, {63, -2, 22}, {19, 3, 4},  {88, 6, 31},
        {104, 4, 18}, {57, -6, 11}, {33, 0, 7}, {71, 5, 26},  {96, 2, 12}, {41, -3, 16},
    };
    std::vector<Point> late_ = {
        {120, 38, 28}, {142, 52, 19}, {88, 44, 8},   {166, 71, 34},
        {134, 29, 12}, {178, 96, 41}, {151, 61, 23},
    };

    std::array<Dock, 7> docks_ = {{
        {"D-01", "Norlin Freight", "08:00 – 08:45", "UNLOADING", kit::Tone::Info},
        {"D-02", "Kessler Transit", "08:30 – 09:15", "ARRIVED", kit::Tone::Ok},
        {"D-03", "Portway own", "09:00 – 09:30", "SCHEDULED", kit::Tone::Neutral},
        {"D-04", "Vale Cargo", "07:45 – 08:30", "DETENTION 41 min", kit::Tone::Alarm},
        {"D-05", "Norlin Freight", "09:15 – 10:00", "SCHEDULED", kit::Tone::Neutral},
        {"D-06", "Anselm Cold", "08:15 – 09:00", "UNLOADING", kit::Tone::Info},
        {"D-07", "—", "—", "FREE", kit::Tone::Neutral},
    }};

    std::array<Vehicle, 8> fleet_ = {{
        {"TR-1140", "A. Moreau", "R-12 city north", "IN TRANSIT", 8, 14, -6.0, 62.0, kit::Tone::Ok},
        {"TR-1142", "J. Okafor", "R-04 industrial", "IN TRANSIT", 11, 16, 3.0, 48.0, kit::Tone::Ok},
        {"TR-1155", "S. Bianchi", "R-21 coastal", "DELAYED", 5, 18, 41.0, 27.0, kit::Tone::Alarm},
        {"TR-1160", "M. Haddad", "R-09 ring", "IN TRANSIT", 13, 15, 0.0, 71.0, kit::Tone::Ok},
        {"TR-1171", "K. Lindqvist", "R-33 rural", "LOADING", 0, 12, 0.0, 96.0, kit::Tone::Neutral},
        {"TR-1180", "P. Novák", "R-07 city south", "IN TRANSIT", 9, 13, 12.0, 55.0,
         kit::Tone::Warn},
        {"TR-1183", "D. Ferreira", "R-15 airport", "AT CUSTOMER", 14, 14, -2.0, 39.0,
         kit::Tone::Ok},
        {"TR-1190", "L. Nakamura", "R-28 depot run", "MAINTENANCE", 0, 0, 0.0, 18.0,
         kit::Tone::Warn},
    }};
};

constexpr std::array<std::string_view, 3> kViews = {"TODAY", "WEEK", "MONTH"};

void Logistics::punctuality(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "DELIVERY PUNCTUALITY",
                                    .note = "minutes late against distance · dot size is pallets",
                                    .grow = 1.5f,
                                    .minWidth = 340.0f});
    const std::vector<PointSeries> series = {
        {.name = "Within window", .points = onSchedule_, .color = Token::Added},
        {.name = "Missed window", .points = late_, .color = Token::Removed},
    };
    scatterChart(ui, input, "logistics.punctuality", series,
                 {.height = 244.0f,
                  .valueFormat = "%.0f min",
                  .xFormat = "%.0f km",
                  .minRadius = 4.0f,
                  .maxRadius = 18.0f});
}

void Logistics::zones(Ui& ui) {
    auto card = kit::card(ui, {.title = "PICK ZONE OCCUPANCY",
                                    .note = "of rack capacity",
                                    .gap = 12.0f,
                                    .width = 268.0f});

    struct Zone {
        std::string_view name;
        double occupancy;
    };
    static constexpr std::array<Zone, 6> zones = {{
        {"A · fast movers", 91.0},
        {"B · ambient", 74.0},
        {"C · cold chain", 58.0},
        {"D · bulk", 83.0},
        {"E · returns", 34.0},
        {"F · hazmat", 22.0},
    }};

    for (const Zone& zone : zones) {
        kit::meter(ui, {.label = zone.name,
                        .value = zone.occupancy,
                        .unit = "%",
                        .tone = kit::toneFor(zone.occupancy, 80.0, 90.0),
                        .valueFormat = "%.0f"});
    }
}

void Logistics::docks(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "DOCK SCHEDULE",
                                    .note = "next 2 hours",
                                    .gap = 0.0f,
                                    .grow = 1.0f,
                                    .height = 292.0f,
                                    .minWidth = 300.0f});

    auto list = scrollArea(ui, input, "logistics.docks", dockScroll_, {.gap = 1.0f});
    for (const Dock& dock : docks_) {
        const std::string tag = "logistics.dock." + std::string(dock.bay);
        auto row =
            kit::entry(ui, {.id = tag, .hovered = input.isHovered(tag), .height = 36.0f});
        kit::statusDot(ui, dock.tone);
        text(ui, dock.bay,
             {.color = Token::TextStrong,
              .weight = FontWeight::Medium,
              .role = FontRole::Mono,
              .size = 11.5f});
        {
            Style column;
            column.direction = Direction::Column;
            column.gap = 1.0f;
            column.grow = 1.0f;
            column.minWidth = 0.0f;
            auto columnScope = ui.scope(column);
            text(ui, dock.carrier, {.color = Token::Text, .size = 11.5f});
            text(ui, dock.window,
                 {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.0f});
        }
        kit::pill(ui, dock.state, {.tone = dock.tone, .size = 9.5f});
    }
}

void Logistics::fleet(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "FLEET",
                                    .note = "8 vehicles · 1 delayed",
                                    .gap = 0.0f,
                                    .grow = 1.0f,
                                    .height = 292.0f,
                                    .minWidth = 460.0f});

    const std::vector<Column> columns = {
        {.title = "Vehicle",
         .sizing = ColumnSize::FitContent,
         .fitSample = "TR-9999",
         .fitStyle = {.role = FontRole::Mono}},
        {.title = "Driver", .width = 1.2f},
        {.title = "Route", .width = 1.4f},
        {.title = "Status", .sizing = ColumnSize::FitContent, .fitSample = "MAINTENANCE"},
        {.title = "Stops", .width = 1.0f},
        {.title = "ETA",
         .sizing = ColumnSize::FitContent,
         .fitSample = "+99 min",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End},
        {.title = "Fuel", .width = 0.9f},
    };

    table(ui, input, "logistics.fleet", columns, fleet_.size(), fleetTable_,
          [&](Ui& cellUi, std::size_t row, std::size_t column) {
              const Vehicle& vehicle = fleet_[row];
              switch (column) {
                  case 0:
                      kit::statusDot(cellUi, vehicle.tone);
                      kit::hspace(cellUi, 8.0f);
                      text(cellUi, vehicle.plate,
                           {.color = Token::TextStrong,
                            .weight = FontWeight::Medium,
                            .role = FontRole::Mono,
                            .size = 11.5f,
                            .grow = 1.0f});
                      break;
                  case 1:
                      text(cellUi, vehicle.driver, {.color = Token::Text, .grow = 1.0f});
                      break;
                  case 2:
                      text(cellUi, vehicle.route,
                           {.color = Token::TextMuted, .size = 11.5f, .grow = 1.0f});
                      break;
                  case 3:
                      text(cellUi, vehicle.status,
                           {.color = kit::toneToken(vehicle.tone), .size = 11.0f, .grow = 1.0f});
                      break;
                  case 4:
                      kit::meter(cellUi, {.label = "",
                                          .value = static_cast<double>(vehicle.stopsDone),
                                          .maximum = vehicle.stopsTotal > 0
                                                         ? static_cast<double>(vehicle.stopsTotal)
                                                         : 1.0,
                                          .tone = kit::Tone::Info,
                                          .showValue = false,
                                          .height = 6.0f,
                                          .grow = 1.0f});
                      break;
                  case 5:
                      text(cellUi,
                           vehicle.status == "MAINTENANCE"
                               ? std::string("—")
                               : kit::format("%+.0f min", vehicle.etaMinutes),
                           {.color = vehicle.etaMinutes > 10.0  ? Token::Removed
                                     : vehicle.etaMinutes < 0.0 ? Token::Added
                                                                : Token::Text,
                            .role = FontRole::Mono,
                            .align = TextAlign::End,
                            .grow = 1.0f});
                      break;
                  default:
                      kit::meter(cellUi, {.label = "",
                                          .value = vehicle.fuel,
                                          .tone = kit::toneBelow(vehicle.fuel, 30.0, 20.0),
                                          .showValue = false,
                                          .height = 6.0f,
                                          .grow = 1.0f});
                      break;
              }
          },
          {.rowHeight = 34.0f, .zebra = true});
}

NodeId Logistics::build(Frame& frame) {
    Ui& ui = frame.ui;
    const Interaction& input = frame.input;

    onTime_.advance(frame.time);
    throughput_.advance(frame.time);
    dwell_.advance(frame.time);
    backlog_.advance(frame.time);

    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.scope(window);

    {
        auto header = kit::header(ui, Icon::Package, "Portway Control Tower",
                                       "DC Rotterdam-Zuid · 42 bays · 168 vehicles");
        spacer(ui);
        {
            Style strip;
            strip.width = 220.0f;
            strip.shrink = 0.0f;
            auto stripScope = ui.scope(strip);
            std::vector<TabItem> items;
            for (std::string_view label : kViews) items.push_back({.label = label});
            if (const auto chosen = tabs(ui, input, "logistics.view", items, view_,
                                         {.thickness = 30.0f, .rule = false})) {
                view_ = *chosen;
            }
        }
        button(
            ui, input, "MANIFEST",
            {.variant = ButtonVariant::Ghost, .leading = Icon::File, .id = "logistics.manifest"});
        button(ui, input, "DISPATCH RUN",
               {.variant = ButtonVariant::Primary,
                .leading = Icon::Upload,
                .id = "logistics.dispatch"});
    }
    kit::rule(ui, Direction::Column);

    {
        Style body;
        body.direction = Direction::Column;
        body.grow = 1.0f;
        body.basis = 0.0f;
        auto bodyScope = ui.scope(body);
        auto page = scrollArea(ui, input, "logistics.page", page_,
                               {.padding = Edges::all(12.0f), .gap = 12.0f});

        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.scope(row);
            kit::statTile(ui,
                          {.label = "ON-TIME DELIVERY",
                           .value = kit::format("%.1f%%", onTime_.latest()),
                           .trend = kit::signedPercent(onTime_.trend()),
                           .trendTone = onTime_.trend() >= 0.0 ? kit::Tone::Ok : kit::Tone::Alarm,
                           .tone = kit::toneBelow(onTime_.latest(), 94.0, 90.0),
                           .history = &onTime_.values()});
            kit::statTile(ui, {.label = "THROUGHPUT",
                               .value = kit::format("%.0f", throughput_.latest()),
                               .unit = "pallets/h",
                               .tone = kit::Tone::Info,
                               .history = &throughput_.values()});
            kit::statTile(ui, {.label = "AVERAGE DOCK DWELL",
                               .value = kit::format("%.0f", dwell_.latest()),
                               .unit = "min",
                               .tone = kit::toneFor(dwell_.latest(), 45.0, 55.0),
                               .history = &dwell_.values()});
            kit::statTile(ui, {.label = "OPEN ORDER LINES",
                               .value = kit::compact(backlog_.latest()),
                               .tone = kit::toneFor(backlog_.latest(), 450.0, 560.0),
                               .history = &backlog_.values()});
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.height = 300.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.scope(row);
            punctuality(ui, input);
            zones(ui);
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.scope(row);
            fleet(ui, input);
            docks(ui, input);
        }
    }

    kit::rule(ui, Direction::Column);
    {
        auto bar = kit::statusBar(ui);
        kit::statusItem(ui, Icon::RefreshCw, "telematics live · 168 of 168", kit::Tone::Ok);
        kit::statusItem(ui, Icon::Folder, "WMS Blue Yonder · TMS Portway");
        spacer(ui);
        kit::statusItem(ui, Icon::CircleAlert, "D-04 in detention 41 min", kit::Tone::Alarm);
        text(ui, "PORTWAY TOWER 5.6", {.color = Token::TextMuted, .size = 10.5f});
    }
    return root.id();
}

}  // namespace

DemoInfo logisticsDemo() {
    return {.id = "logistics",
            .title = "Portway Control Tower",
            .sector = "Logistics · WMS / TMS",
            .summary =
                "A warehouse and fleet desk: a bubble scatter of lateness against "
                "distance, zone occupancy meters, the dock schedule and the fleet.",
            .highlights = {"Bubble scatter", "Two continuous axes", "Dock schedule", "Fleet table"},
            .tryThis =
                "Hover a bubble out on the right of the scatter: far from the depot, well "
                "past its window, and a big load.",
            .design = {1400.0f, 812.0f},
            .palette = Palette::Follow,
            .create = [] { return std::unique_ptr<Demo>(new Logistics()); }};
}

}  // namespace gbui::demos
