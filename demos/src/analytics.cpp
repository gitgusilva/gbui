// Meridian — the revenue dashboard every SaaS company has a version of.
//
// The familiar one, and it goes first for that reason: a reader who has built
// this screen in React knows exactly what they are looking at and can spend
// their attention on how the toolkit says it instead of on what it says.
//
// Interactive: the range strip along the top, the chart's crosshair and its
// brush, the donut's wedges, and the table's sorting and selection.

#include <algorithm>
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

struct Account {
    std::string_view name;
    std::string_view owner;
    std::string_view plan;
    double revenue;
    double growth;
    int seats;
    std::string_view renews;
    kit::Tone health;
};

class Analytics final : public Demo {
public:
    NodeId build(Frame& frame) override;

private:
    void kpis(Ui& ui);
    void revenuePanel(Ui& ui, const Interaction& input);
    void mixPanel(Ui& ui, const Interaction& input);
    void accountsPanel(Ui& ui, const Interaction& input);

    // Four independent waves, so the tiles do not all move together — which is
    // the giveaway that a dashboard is a mock-up.
    kit::Rolling revenue_{48, 22'000.0, 47'000.0, 1.7f, 0.8f};
    kit::Rolling forecast_{48, 24'000.0, 44'000.0, 4.1f, 0.8f};
    kit::Rolling signups_{48, 180.0, 460.0, 9.3f, 0.7f};
    kit::Rolling churn_{48, 0.6, 2.4, 5.5f, 1.1f};
    kit::Rolling latency_{48, 120.0, 410.0, 2.9f, 0.5f};

    std::size_t range_ = 2;
    ChartView view_{};
    DonutState channels_{};
    TableState accounts_{};
    ScrollState page_{};
    std::vector<Account> rows_ = {
        {"Halden Robotics", "T. Okonkwo", "Enterprise", 184'200.0, 12.4, 1420, "12 Mar",
         kit::Tone::Ok},
        {"Ostara Freight", "M. Halvorsen", "Enterprise", 152'800.0, 4.1, 980, "03 Apr",
         kit::Tone::Ok},
        {"Verity Health", "R. Duarte", "Business", 98'400.0, -2.7, 610, "28 Feb", kit::Tone::Warn},
        {"Kestrel Energy", "T. Okonkwo", "Enterprise", 91'050.0, 18.9, 540, "19 Jun",
         kit::Tone::Ok},
        {"Northline Media", "S. Aterno", "Business", 74'300.0, 0.4, 388, "07 May",
         kit::Tone::Neutral},
        {"Arbor Foods", "R. Duarte", "Business", 61'700.0, -9.2, 295, "14 Feb", kit::Tone::Alarm},
        {"Quill & Co", "S. Aterno", "Team", 44'900.0, 6.8, 176, "22 Sep", kit::Tone::Ok},
        {"Sable Logistics", "M. Halvorsen", "Business", 39'250.0, 1.9, 154, "30 Jul",
         kit::Tone::Neutral},
        {"Mercer Bank", "T. Okonkwo", "Enterprise", 210'600.0, 7.3, 1810, "01 Dec", kit::Tone::Ok},
        {"Ironwood Retail", "S. Aterno", "Team", 28'400.0, -4.5, 96, "16 Nov", kit::Tone::Warn},
    };
};

constexpr std::array<std::string_view, 4> kRanges = {"24H", "7D", "30D", "12M"};

/** The categories under the revenue chart, which change with the range — the
 *  one thing about a range selector that is actually work. */
std::vector<std::string> categoriesFor(std::size_t range, std::size_t count) {
    std::vector<std::string> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t back = count - i - 1;
        switch (range) {
            case 0:
                out.push_back(kit::format("%.0fh ago", static_cast<double>(back)));
                break;
            case 1:
                out.push_back(kit::format("D-%.0f", static_cast<double>(back)));
                break;
            case 3:
                out.push_back(kit::format("M-%.0f", static_cast<double>(back)));
                break;
            default:
                out.push_back(kit::format("day %.0f", static_cast<double>(i + 1)));
                break;
        }
    }
    return out;
}

void Analytics::kpis(Ui& ui) {
    Style row;
    row.direction = Direction::Row;
    row.gap = 12.0f;
    row.shrink = 0.0f;
    auto scope = ui.begin(row);

    // The headline tile carries a wash of its own tone; the three beside it do
    // not. A row where every tile is tinted has no headline.
    kit::statTile(ui, {.label = "MRR",
                       .value = "$" + kit::compact(kit::eased(ui, "analytics.mrr", "v",
                                                              revenue_.latest() * 30.0)),
                       .tint = true,
                       .trend = kit::signedPercent(revenue_.trend()),
                       .trendTone = revenue_.trend() >= 0.0 ? kit::Tone::Ok : kit::Tone::Alarm,
                       .tone = kit::Tone::Info,
                       .history = &revenue_.values()});
    kit::statTile(ui, {.label = "NEW SIGNUPS",
                       .value = kit::format("%.0f", signups_.latest()),
                       .unit = "today",
                       .trend = kit::signedPercent(signups_.trend()),
                       .trendTone = signups_.trend() >= 0.0 ? kit::Tone::Ok : kit::Tone::Alarm,
                       .tone = kit::Tone::Ok,
                       .history = &signups_.values()});
    kit::statTile(ui, {.label = "CHURN",
                       .value = kit::format("%.2f%%", churn_.latest()),
                       .trend = kit::signedPercent(churn_.trend()),
                       // A rising churn is bad news, so the arrow is inverted
                       // against every other tile here. Getting this backwards
                       // is the most common bug in a dashboard.
                       .trendTone = churn_.trend() <= 0.0 ? kit::Tone::Ok : kit::Tone::Alarm,
                       .tone = kit::toneFor(churn_.latest(), 1.6, 2.1),
                       .history = &churn_.values()});
    kit::statTile(ui, {.label = "API P95",
                       .value = kit::format(
                           "%.0f", kit::eased(ui, "analytics.latency", "v", latency_.latest())),
                       .unit = "ms",
                       .trend = kit::signedPercent(latency_.trend()),
                       .trendTone = latency_.trend() <= 0.0 ? kit::Tone::Ok : kit::Tone::Alarm,
                       .tone = kit::toneFor(latency_.latest(), 280.0, 360.0),
                       .history = &latency_.values()});
}

void Analytics::revenuePanel(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(ui, {.title = "REVENUE VS FORECAST",
                                    .note = "drag to pan · ctrl+wheel to zoom",
                                    .gap = 6.0f,
                                    .grow = 1.6f,
                                    .minWidth = 320.0f});

    const std::vector<Series> series = {
        {.name = "Booked", .values = revenue_.values(), .color = Token::Accent},
        {.name = "Forecast",
         .values = forecast_.values(),
         .color = Token::Graph3,
         .fillAlpha = 0.0f},
    };
    const auto categories = categoriesFor(range_, revenue_.values().size());

    lineChart(ui, input, "analytics.revenue", series, view_,
              {.height = 190.0f, .valueFormat = "$%.0f", .categories = categories},
              {.wheel = true, .drag = true});
    chartBrush(ui, input, "analytics.revenue.brush", series, view_, {.axisWidth = 46.0f});

    {
        Style legend;
        legend.direction = Direction::Row;
        legend.align = Align::Center;
        legend.gap = 16.0f;
        legend.shrink = 0.0f;
        auto legendScope = ui.begin(legend);
        kit::statusDot(ui, kit::Tone::Info, 8.0f);
        text(ui, "Booked", {.color = Token::TextMuted, .size = 11.0f});
        {
            Style swatch;
            swatch.width = 8.0f;
            swatch.height = 8.0f;
            swatch.radius = 4.0f;
            swatch.shrink = 0.0f;
            swatch.alignSelf = Align::Center;
            swatch.background = Fill{Token::Graph3};
            ui.add(swatch);
        }
        text(ui, "Forecast", {.color = Token::TextMuted, .size = 11.0f});
        spacer(ui);
        text(ui, "peak " + kit::format("$%.0f", revenue_.peak()),
             {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.0f});
    }
}

void Analytics::mixPanel(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(ui, {.title = "CHANNEL MIX", .grow = 1.0f, .minWidth = 240.0f});

    const std::vector<Slice> slices = {
        {.name = "Direct", .value = 38.0},     {.name = "Partner", .value = 24.0},
        {.name = "Self-serve", .value = 21.0}, {.name = "Marketplace", .value = 11.0},
        {.name = "Referral", .value = 6.0},
    };
    donutChart(ui, input, "analytics.mix", slices, channels_,
               {.size = 176.0f, .legendMaxHeight = 176.0f});
}

void Analytics::accountsPanel(Ui& ui, const Interaction& input) {
    auto card =
        kit::beginCard(ui, {.title = "TOP ACCOUNTS",
                            .note = kit::format("%.0f of 1 284", static_cast<double>(rows_.size())),
                            .gap = 0.0f,
                            .grow = 1.0f,
                            .height = 296.0f});

    // `fitStyle` is what the cell actually draws in. Without it the sample is
    // measured in the UI face and the column comes out a glyph short of the
    // mono numbers it was sized for — which is a column that ellipsises the
    // one value it exists to show.
    const std::vector<Column> columns = {
        {.title = "Account", .width = 1.6f, .sortable = true},
        {.title = "Owner", .width = 1.0f},
        {.title = "Plan", .sizing = ColumnSize::FitContent, .fitSample = "Enterprise"},
        {.title = "ARR",
         .sizing = ColumnSize::FitContent,
         .fitSample = "$9 999.9k",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End,
         .sortable = true},
        {.title = "Growth",
         .sizing = ColumnSize::FitContent,
         .fitSample = "-99.9%",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End,
         .sortable = true},
        {.title = "Seats",
         .sizing = ColumnSize::FitContent,
         .fitSample = "9 999",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End},
        {.title = "Renews",
         .sizing = ColumnSize::FitContent,
         .fitSample = "99 Mmm",
         .fitStyle = {.role = FontRole::Mono}},
    };

    const TableResult result = table(
        ui, input, "analytics.accounts", columns, rows_.size(), accounts_,
        [&](Ui& cellUi, std::size_t row, std::size_t column) {
            const Account& account = rows_[row];
            switch (column) {
                case 0: {
                    kit::statusDot(cellUi, account.health);
                    kit::hspace(cellUi, 8.0f);
                    text(cellUi, account.name,
                         {.color = Token::TextStrong, .weight = FontWeight::Medium, .grow = 1.0f});
                    break;
                }
                case 1:
                    text(cellUi, account.owner, {.color = Token::Text, .grow = 1.0f});
                    break;
                case 2:
                    badge(cellUi, account.plan);
                    break;
                case 3:
                    text(cellUi, "$" + kit::compact(account.revenue),
                         {.color = Token::Text,
                          .role = FontRole::Mono,
                          .align = TextAlign::End,
                          .grow = 1.0f});
                    break;
                case 4:
                    text(cellUi, kit::signedPercent(account.growth),
                         {.color = account.growth >= 0.0 ? Token::Added : Token::Removed,
                          .role = FontRole::Mono,
                          .align = TextAlign::End,
                          .grow = 1.0f});
                    break;
                case 5:
                    text(cellUi, kit::format("%.0f", static_cast<double>(account.seats)),
                         {.color = Token::TextMuted,
                          .role = FontRole::Mono,
                          .align = TextAlign::End,
                          .grow = 1.0f});
                    break;
                default:
                    text(cellUi, account.renews,
                         {.color = Token::TextMuted, .role = FontRole::Mono, .grow = 1.0f});
                    break;
            }
        },
        {.rowHeight = 32.0f, .zebra = true});

    // The table reports that the reader asked for a different order; reordering
    // the data is this file's job, because only it knows how to compare two of
    // its own rows. That division is the whole contract — see table.hpp.
    if (result.sortChanged && accounts_.sortColumn >= 0) {
        const int column = accounts_.sortColumn;
        const bool ascending = accounts_.ascending;
        std::stable_sort(rows_.begin(), rows_.end(), [&](const Account& a, const Account& b) {
            bool less = false;
            switch (column) {
                case 0:
                    less = a.name < b.name;
                    break;
                case 3:
                    less = a.revenue < b.revenue;
                    break;
                case 4:
                    less = a.growth < b.growth;
                    break;
                default:
                    less = a.seats < b.seats;
                    break;
            }
            return ascending ? less : !less;
        });
    }
}

NodeId Analytics::build(Frame& frame) {
    Ui& ui = frame.ui;
    const Interaction& input = frame.input;

    revenue_.advance(frame.time);
    forecast_.advance(frame.time);
    signups_.advance(frame.time);
    churn_.advance(frame.time);
    latency_.advance(frame.time);

    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.begin(window);

    {
        auto header = kit::beginHeader(ui, Icon::ChartPie, "Meridian",
                                       "Revenue intelligence · workspace: acme-eu");
        spacer(ui);
        {
            Style strip;
            strip.width = 240.0f;
            strip.shrink = 0.0f;
            auto stripScope = ui.begin(strip);
            std::vector<TabItem> items;
            for (std::string_view label : kRanges) items.push_back({.label = label});
            if (const auto chosen = tabs(ui, input, "analytics.range", items, range_,
                                         {.thickness = 30.0f, .rule = false})) {
                range_ = *chosen;
                // A new range is a new window on the data, so the pan resets —
                // otherwise the reader lands somewhere they did not choose.
                view_.reset();
            }
        }
        button(
            ui, input, "EXPORT",
            {.variant = ButtonVariant::Ghost, .leading = Icon::Download, .id = "analytics.export"});
        button(ui, input, "REFRESH",
               {.variant = ButtonVariant::Primary,
                .leading = Icon::RefreshCw,
                .id = "analytics.refresh"});
    }
    kit::rule(ui, Direction::Column);

    {
        Style body;
        body.direction = Direction::Column;
        body.grow = 1.0f;
        body.basis = 0.0f;
        auto bodyScope = ui.begin(body);
        auto page = beginScroll(ui, input, "analytics.page", page_,
                                {.padding = Edges::all(14.0f), .gap = 12.0f});

        kpis(ui);
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.height = 300.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.begin(row);
            revenuePanel(ui, input);
            mixPanel(ui, input);
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.begin(row);
            accountsPanel(ui, input);
        }
    }

    kit::rule(ui, Direction::Column);
    {
        auto bar = kit::beginStatusBar(ui);
        kit::statusItem(ui, Icon::RefreshCw, "synced 12s ago", kit::Tone::Ok);
        kit::statusItem(ui, Icon::Terminal, "warehouse: snowflake-eu-1");
        spacer(ui);
        kit::statusItem(ui, Icon::ClockFading, kRanges[range_]);
        text(ui, "MERIDIAN 4.2", {.color = Token::TextMuted, .size = 10.5f});
    }
    return root.id();
}

}  // namespace

DemoInfo analyticsDemo() {
    return {
        .id = "analytics",
        .title = "Meridian Analytics",
        .sector = "SaaS · Business intelligence",
        .summary =
            "A revenue dashboard: KPI tiles with sparklines, a pannable line chart "
            "over a brush, a channel donut and a sortable account table.",
        .highlights = {"Pan and zoom", "Sortable table", "Donut with legend", "Live sparklines"},
        .tryThis =
            "Drag inside the revenue chart to pan it, hold Ctrl and scroll to zoom, "
            "drag the window along the strip underneath, and click ARR twice to sort by "
            "it.",
        .design = {1280.0f, 824.0f},
        .palette = Palette::Follow,
        .create = [] { return std::unique_ptr<Demo>(new Analytics()); }};
}

}  // namespace gbui::demos
