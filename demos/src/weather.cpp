// Aurora — a forecaster's desk.
//
// The screen a meteorological service puts on a wall: one station's current
// observation large enough to read from across the room, the model runs beside
// it, and the network it belongs to underneath.
//
// It is here because a weather desk is the honest test of a *readout*. Nothing
// on it is clickable in the way a dashboard is — the work is making eleven
// numbers, a week of forecasts and a rainfall chart legible at a glance, which
// is a typography and spacing problem rather than an interaction one.

#include <array>
#include <string>
#include <vector>

#include "gbui/widgets/chart.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/containers.hpp"

#include "kit.hpp"
#include "registry.hpp"

namespace gbui::demos {
namespace {

struct Station {
    std::string_view code;
    std::string_view name;
    double temperature;
    double wind;
    std::string_view condition;
    kit::Tone status;
};

struct Day {
    std::string_view name;
    std::string_view condition;
    double high;
    double low;
    int rainChance;
};

class Weather final : public Demo {
public:
    NodeId build(Frame& frame) override;

private:
    void current(Ui& ui);
    void observations(Ui& ui);
    void trends(Ui& ui, const Interaction& input);
    void forecast(Ui& ui, const Interaction& input);
    void network(Ui& ui, const Interaction& input);

    kit::Rolling temperature_{36, 11.0, 21.0, 3.3f, 0.9f};
    kit::Rolling dewPoint_{36, 6.0, 14.0, 7.1f, 0.9f};
    kit::Rolling wind_{36, 8.0, 46.0, 2.2f, 0.7f};
    kit::Rolling pressure_{36, 1002.0, 1021.0, 6.4f, 1.4f};
    kit::Rolling humidity_{36, 48.0, 92.0, 8.8f, 1.0f};

    ChartView view_{};
    ScrollState page_{};
    ScrollState stationScroll_{};
    std::size_t station_ = 0;
    std::size_t model_ = 0;

    std::vector<Station> stations_ = {
        {"SBGL", "Rio de Janeiro / Galeão", 27.4, 18.0, "Scattered cloud", kit::Tone::Ok},
        {"EGLL", "London Heathrow", 14.2, 31.0, "Light rain", kit::Tone::Warn},
        {"KJFK", "New York / Kennedy", 9.8, 24.0, "Overcast", kit::Tone::Ok},
        {"LFPG", "Paris Charles de Gaulle", 12.6, 12.0, "Mist", kit::Tone::Warn},
        {"EDDF", "Frankfurt am Main", 10.1, 9.0, "Broken cloud", kit::Tone::Ok},
        {"LEMD", "Madrid Barajas", 19.7, 15.0, "Clear", kit::Tone::Ok},
        {"BIKF", "Keflavík", 2.3, 74.0, "Gale · blowing snow", kit::Tone::Alarm},
        {"ENGM", "Oslo Gardermoen", 4.6, 21.0, "Snow showers", kit::Tone::Warn},
    };

    std::array<Day, 7> week_ = {{
        {"TODAY", "Light rain", 15.0, 9.0, 78},
        {"TUE", "Showers", 14.0, 8.0, 65},
        {"WED", "Overcast", 13.0, 7.0, 30},
        {"THU", "Broken cloud", 16.0, 8.0, 15},
        {"FRI", "Clear", 19.0, 10.0, 5},
        {"SAT", "Clear", 21.0, 12.0, 5},
        {"SUN", "Scattered cloud", 18.0, 11.0, 20},
    }};

    // Rainfall over the last twelve hours, in millimetres. Fixed rather than
    // simulated: rain that changes every second reads as a bug, not as weather.
    std::vector<double> rainfall_ = {0.0, 0.0, 0.2, 1.4, 3.8, 6.1, 4.2, 2.6, 0.9, 0.1, 0.0, 0.0};
};

constexpr std::array<std::string_view, 3> kModels = {"ECMWF", "GFS", "ICON"};

void Weather::current(Ui& ui) {
    const Station& here = stations_[station_];

    auto card = kit::beginCard(
        ui, {.title = "CURRENT OBSERVATION", .note = "12:50 UTC", .gap = 14.0f, .width = 300.0f});

    {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 10.0f;
        auto rowScope = ui.begin(row);
        {
            // The reading refuses to shrink. Everything else in this row can
            // give up width; a temperature elided to "1…" is worse than no
            // temperature, because it looks like a value.
            Style reading;
            reading.direction = Direction::Row;
            reading.align = Align::Center;
            reading.gap = 6.0f;
            reading.shrink = 0.0f;
            auto readingScope = ui.begin(reading);
            text(ui, kit::format("%.1f", temperature_.latest()),
                 {.color = Token::TextStrong, .weight = FontWeight::Bold, .size = 50.0f});
            Style unit;
            unit.direction = Direction::Column;
            unit.gap = 2.0f;
            unit.shrink = 0.0f;
            auto unitScope = ui.begin(unit);
            text(ui, "°C", {.color = Token::TextMuted, .size = 16.0f});
            text(ui, kit::format("feels %.0f°", temperature_.latest() - 2.4),
                 {.color = Token::TextMuted, .size = 11.0f});
        }
        spacer(ui);
        kit::gauge(ui, {.value = humidity_.latest(),
                        .label = "HUMIDITY",
                        .unit = "%",
                        .tone = kit::Tone::Info,
                        .size = 84.0f,
                        .thickness = 7.0f});
    }

    text(ui, here.condition, {.color = Token::Text, .weight = FontWeight::Medium, .size = 14.0f});

    kit::rule(ui, Direction::Column);

    {
        Style grid;
        grid.direction = Direction::Row;
        grid.wrap = true;
        grid.gap = 14.0f;
        grid.crossGap = 12.0f;
        auto gridScope = ui.begin(grid);

        const auto cell = [&](std::string_view label, const std::string& value) {
            Style item;
            item.width = 108.0f;
            item.shrink = 0.0f;
            auto itemScope = ui.begin(item);
            kit::field(ui, label, value, Token::TextStrong, FontRole::Mono);
        };
        cell("WIND", kit::format("%.0f km/h", wind_.latest()));
        cell("GUST", kit::format("%.0f km/h", wind_.latest() * 1.45));
        cell("PRESSURE", kit::format("%.0f hPa", pressure_.latest()));
        cell("DEW POINT", kit::format("%.1f °C", dewPoint_.latest()));
        cell("VISIBILITY", "9 km");
        cell("CEILING", "1 200 ft");
    }
}

void Weather::observations(Ui& ui) {
    Style column;
    column.direction = Direction::Column;
    column.gap = 12.0f;
    column.shrink = 0.0f;
    column.minWidth = 0.0f;
    auto scope = ui.begin(column);

    {
        Style row;
        row.direction = Direction::Row;
        row.gap = 12.0f;
        row.shrink = 0.0f;
        auto rowScope = ui.begin(row);
        kit::statTile(ui, {.label = "PRESSURE",
                           .value = kit::format("%.0f", pressure_.latest()),
                           .unit = "hPa",
                           .trend = kit::signedPercent(pressure_.trend()),
                           .trendTone = pressure_.trend() >= 0.0 ? kit::Tone::Ok : kit::Tone::Warn,
                           .tone = kit::Tone::Info,
                           .history = &pressure_.values(),
                           .valueSize = 22.0f});
        kit::statTile(ui, {.label = "WIND",
                           .value = kit::format("%.0f", wind_.latest()),
                           .unit = "km/h",
                           .tone = kit::toneFor(wind_.latest(), 30.0, 42.0),
                           .history = &wind_.values(),
                           .valueSize = 22.0f});
        kit::statTile(ui, {.label = "HUMIDITY",
                           .value = kit::format("%.0f", humidity_.latest()),
                           .unit = "%",
                           .tone = kit::Tone::Neutral,
                           .history = &humidity_.values(),
                           .valueSize = 22.0f});
    }
}

void Weather::trends(Ui& ui, const Interaction& input) {
    {
        auto card = kit::beginCard(ui, {.title = "TEMPERATURE AND DEW POINT",
                                        .note = "last 6 hours · 10 min steps",
                                        .grow = 1.0f,
                                        .minWidth = 320.0f});
        const std::vector<Series> series = {
            {.name = "Temperature", .values = temperature_.values(), .color = Token::Graph2},
            {.name = "Dew point", .values = dewPoint_.values(), .color = Token::Graph6},
        };
        lineChart(ui, input, "weather.temperature", series, view_,
                  {.height = 168.0f, .valueFormat = "%.1f°"}, {.drag = true});
    }
    {
        auto card =
            kit::beginCard(ui, {.title = "RAINFALL", .note = "mm per hour", .width = 300.0f});
        const std::vector<Series> series = {
            {.name = "Rainfall", .values = rainfall_, .color = Token::Graph6}};
        barChart(
            ui, input, "weather.rainfall", series,
            {.height = 168.0f,
             .valueFormat = "%.1f mm",
             .categories = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"},
             .categoryAxis = 18.0f});
    }
}

void Weather::forecast(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(
        ui, {.title = "SEVEN DAY", .note = kModels[model_], .gap = 2.0f, .width = 288.0f});

    for (std::size_t i = 0; i < week_.size(); ++i) {
        const Day& day = week_[i];
        const std::string tag = "weather.day." + std::string(day.name);
        auto row = kit::beginEntry(
            ui, {.id = tag, .hovered = input.isHovered(tag), .height = 32.0f, .gap = 8.0f});
        text(ui, day.name,
             {.color = i == 0 ? Token::TextStrong : Token::Text,
              .weight = i == 0 ? FontWeight::SemiBold : FontWeight::Regular,
              .size = 11.5f,
              .grow = 0.0f});
        {
            Style fixed;
            fixed.width = 34.0f;
            fixed.shrink = 0.0f;
            ui.add(fixed);
        }
        text(ui, day.condition, {.color = Token::TextMuted, .size = 11.0f, .grow = 1.0f});
        text(ui, kit::format("%.0f%%", static_cast<double>(day.rainChance)),
             {.color = day.rainChance >= 60 ? Token::Accent : Token::TextMuted,
              .role = FontRole::Mono,
              .size = 11.0f});
        text(ui, kit::format("%.0f°", day.high),
             {.color = Token::TextStrong,
              .weight = FontWeight::Medium,
              .role = FontRole::Mono,
              .size = 12.0f});
        text(ui, kit::format("%.0f°", day.low),
             {.color = Token::TextMuted, .role = FontRole::Mono, .size = 12.0f});
    }
}

void Weather::network(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(ui, {.title = "STATION NETWORK",
                                    .note = "8 reporting · 1 alert",
                                    .gap = 0.0f,
                                    .grow = 1.0f,
                                    .height = 264.0f});

    auto list = beginScroll(ui, input, "weather.stations", stationScroll_, {.gap = 1.0f});
    for (std::size_t i = 0; i < stations_.size(); ++i) {
        const Station& entry = stations_[i];
        const std::string tag = "weather.station." + std::string(entry.code);
        {
            auto row = kit::beginEntry(ui, {.id = tag,
                                            .selected = i == station_,
                                            .hovered = input.isHovered(tag),
                                            .height = 30.0f});
            kit::statusDot(ui, entry.status);
            text(ui, entry.code,
                 {.color = Token::TextStrong,
                  .weight = FontWeight::Medium,
                  .role = FontRole::Mono,
                  .size = 11.5f});
            text(ui, entry.name, {.color = Token::Text, .size = 11.5f, .grow = 1.0f});
            text(ui, entry.condition, {.color = Token::TextMuted, .size = 11.0f});
            text(ui, kit::format("%.1f °C", entry.temperature),
                 {.color = Token::TextStrong, .role = FontRole::Mono, .size = 11.5f});
            text(ui, kit::format("%.0f km/h", entry.wind),
                 {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.0f});
        }
        if (input.clicked(tag)) station_ = i;
    }
}

NodeId Weather::build(Frame& frame) {
    Ui& ui = frame.ui;
    const Interaction& input = frame.input;

    temperature_.advance(frame.time);
    dewPoint_.advance(frame.time);
    wind_.advance(frame.time);
    pressure_.advance(frame.time);
    humidity_.advance(frame.time);

    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.begin(window);

    {
        const Station& here = stations_[station_];
        auto header = kit::beginHeader(ui, Icon::ClockFading, "Aurora Weather Desk", here.name);
        spacer(ui);
        {
            Style strip;
            strip.width = 186.0f;
            strip.shrink = 0.0f;
            auto stripScope = ui.begin(strip);
            std::vector<TabItem> items;
            for (std::string_view label : kModels) items.push_back({.label = label});
            if (const auto chosen = tabs(ui, input, "weather.model", items, model_,
                                         {.thickness = 30.0f, .rule = false})) {
                model_ = *chosen;
            }
        }
        kit::pill(ui, "1 WARNING", {.tone = kit::Tone::Alarm});
        button(ui, input, "SYNOPTIC",
               {.variant = ButtonVariant::Ghost, .leading = Icon::Eye, .id = "weather.synoptic"});
    }
    kit::rule(ui, Direction::Column);

    // The warning banner. It is the one thing on this screen that is allowed
    // to be loud, so it sits above everything and takes the alarm colour.
    {
        Style banner;
        banner.direction = Direction::Row;
        banner.align = Align::Center;
        banner.gap = 10.0f;
        banner.height = 34.0f;
        banner.shrink = 0.0f;
        banner.padding = Edges::symmetric(0.0f, 16.0f);
        banner.background = Fill{Token::Removed, 0.16f};
        banner.radius = 0.0f;
        auto bannerScope = ui.begin(banner);
        icon(ui, Icon::CircleAlert, {.color = Token::Removed, .size = 15.0f});
        text(ui, "BIKF Keflavík — gale warning in force until 21:00 UTC",
             {.color = Token::Removed, .weight = FontWeight::Medium, .size = 12.0f, .grow = 1.0f});
        text(ui, "issued 09:14 UTC", {.color = Token::TextMuted, .size = 11.0f});
    }

    {
        Style body;
        body.direction = Direction::Column;
        body.grow = 1.0f;
        body.basis = 0.0f;
        auto bodyScope = ui.begin(body);
        auto page = beginScroll(ui, input, "weather.page", page_,
                                {.padding = Edges::all(14.0f), .gap = 12.0f});

        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.begin(row);
            current(ui);
            {
                Style middle;
                middle.direction = Direction::Column;
                middle.gap = 12.0f;
                middle.grow = 1.0f;
                middle.basis = 0.0f;
                middle.minWidth = 0.0f;
                auto middleScope = ui.begin(middle);
                observations(ui);
                {
                    Style charts;
                    charts.direction = Direction::Row;
                    charts.gap = 12.0f;
                    charts.grow = 1.0f;
                    charts.basis = 0.0f;
                    auto chartScope = ui.begin(charts);
                    trends(ui, input);
                }
            }
            forecast(ui, input);
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.begin(row);
            network(ui, input);
        }
    }

    kit::rule(ui, Direction::Column);
    {
        auto bar = kit::beginStatusBar(ui);
        kit::statusItem(ui, Icon::RefreshCw, "METAR 12:50Z · TAF 12:00Z", kit::Tone::Ok);
        kit::statusItem(ui, Icon::Download, std::string(kModels[model_]) + " run 06Z");
        spacer(ui);
        kit::statusItem(ui, Icon::Terminal, "WMO region VI");
        text(ui, "AURORA 2.9", {.color = Token::TextMuted, .size = 10.5f});
    }
    return root.id();
}

}  // namespace

DemoInfo weatherDemo() {
    return {.id = "weather",
            .title = "Aurora Weather Desk",
            .sector = "Meteorology · Forecasting",
            .summary =
                "A forecaster's workstation: one observation large enough to read across "
                "a room, model runs beside it, and the station network underneath.",
            .highlights = {"Dial gauges", "Wrapping field grid", "Rainfall bars",
                           "Selectable station list"},
            .design = {1280.0f, 764.0f},
            .palette = Palette::Follow,
            .create = [] { return std::unique_ptr<Demo>(new Weather()); }};
}

}  // namespace gbui::demos
