// Helix — a plant supervisory screen, the kind that runs on a wall in a
// control room and is looked at for eight hours at a stretch.
//
// Three things make this screen different from the dashboard, and all three
// are why it is worth having in the set:
//
//   * it is **always dark**, whatever the reader's preference says. An
//     operator at three in the morning is not helped by a light theme, so this
//     demo declares `Palette::Dark` and the host honours it;
//   * everything on it is a *reading against a setpoint*, so almost nothing
//     here is a number on its own — it is a dial, a meter or a tone;
//   * it is genuinely operable. The pumps toggle, the setpoint drags, and the
//     alarms acknowledge, which is the whole difference between a control
//     screen and a picture of one.

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

struct Unit {
    std::string_view tag;
    std::string_view name;
    std::string_view state;
    kit::Tone tone;
};

struct Alarm {
    std::string_view time;
    std::string_view tag;
    std::string_view message;
    int priority;
    kit::Tone tone;
    bool acknowledged;
};

struct Pump {
    std::string_view tag;
    std::string_view name;
    double hours;
};

class Scada final : public Demo {
public:
    NodeId build(Frame& frame) override;

private:
    void units(Ui& ui, const Interaction& input);
    void dials(Ui& ui);
    void vesselsAndPumps(Ui& ui, const Interaction& input);
    void trend(Ui& ui, const Interaction& input);
    void commandLog(Ui& ui, const Interaction& input);
    void alarms(Ui& ui, const Interaction& input);

    kit::Rolling flow_{60, 380.0, 520.0, 1.1f, 0.4f};
    kit::Rolling pressure_{60, 2.4, 4.6, 3.7f, 0.4f};
    kit::Rolling turbidity_{60, 0.08, 0.42, 5.2f, 0.6f};
    kit::Rolling ph_{60, 6.7, 7.9, 8.1f, 0.8f};
    kit::Rolling chlorine_{60, 0.4, 1.6, 2.6f, 0.7f};
    kit::Rolling clearwell_{60, 52.0, 94.0, 4.4f, 1.2f};
    kit::Rolling backwash_{60, 12.0, 68.0, 6.9f, 1.0f};
    kit::Rolling sludge_{60, 30.0, 88.0, 9.6f, 1.5f};
    kit::Rolling chemical_{60, 8.0, 42.0, 0.9f, 1.3f};

    ChartView view_{};
    ScrollState alarmScroll_{};
    ScrollState logScroll_{};
    std::size_t unit_ = 2;
    double setpoint_ = 460.0;
    std::array<bool, 4> pumps_ = {true, true, false, true};
    bool autoMode_ = true;

    std::array<Unit, 6> units_ = {{
        {"U-100", "Raw water intake", "RUNNING", kit::Tone::Ok},
        {"U-200", "Coagulation", "RUNNING", kit::Tone::Ok},
        {"U-300", "Clarifier train", "HIGH TURBIDITY", kit::Tone::Warn},
        {"U-400", "Rapid sand filters", "RUNNING", kit::Tone::Ok},
        {"U-500", "UV disinfection", "BYPASS", kit::Tone::Alarm},
        {"U-600", "Distribution pumping", "RUNNING", kit::Tone::Ok},
    }};

    std::array<Pump, 4> pumpList_ = {{
        {"P-401A", "Filter feed A", 12'480.0},
        {"P-401B", "Filter feed B", 11'905.0},
        {"P-402A", "Backwash A", 3'140.0},
        {"P-601", "High lift", 22'610.0},
    }};

    std::vector<Alarm> alarmList_ = {
        {"12:48:02", "AIT-301", "Turbidity above 0.30 NTU", 1, kit::Tone::Alarm, false},
        {"12:41:55", "UV-501", "Lamp 3 end of life", 2, kit::Tone::Warn, false},
        {"12:33:10", "P-402A", "Motor winding temperature high", 2, kit::Tone::Warn, false},
        {"12:04:37", "LIT-410", "Backwash tank low level", 3, kit::Tone::Warn, true},
        {"11:52:19", "FIT-101", "Intake flow deviation", 3, kit::Tone::Neutral, true},
        {"11:20:44", "AIT-604", "Residual chlorine low", 2, kit::Tone::Warn, true},
        {"10:58:03", "PLC-2", "Redundant link restored", 4, kit::Tone::Ok, true},
    };
};

void Scada::units(Ui& ui, const Interaction& input) {
    Style rail;
    rail.direction = Direction::Column;
    rail.width = 196.0f;
    rail.shrink = 0.0f;
    rail.gap = 2.0f;
    rail.padding = Edges::symmetric(10.0f, 0.0f);
    rail.background = Fill{Token::Bg};
    rail.radius = 0.0f;
    auto scope = ui.begin(rail);

    {
        Style heading;
        heading.padding = Edges::symmetric(0.0f, 12.0f);
        heading.shrink = 0.0f;
        auto headingScope = ui.begin(heading);
        sectionHeading(ui, "PROCESS UNITS");
    }

    for (std::size_t i = 0; i < units_.size(); ++i) {
        const Unit& unit = units_[i];
        const std::string tag = "scada.unit." + std::string(unit.tag);
        {
            auto row = kit::beginEntry(ui, {.id = tag,
                                            .selected = i == unit_,
                                            .hovered = input.isHovered(tag),
                                            .height = 42.0f});
            kit::statusDot(ui, unit.tone);
            {
                Style column;
                column.direction = Direction::Column;
                column.gap = 1.0f;
                column.grow = 1.0f;
                column.minWidth = 0.0f;
                auto columnScope = ui.begin(column);
                text(ui, unit.name,
                     {.color = i == unit_ ? Token::TextStrong : Token::Text,
                      .weight = i == unit_ ? FontWeight::Medium : FontWeight::Regular,
                      .size = 12.0f});
                text(ui, unit.state, {.color = kit::toneToken(unit.tone), .size = 10.0f});
            }
            text(ui, unit.tag, {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.5f});
        }
        if (input.clicked(tag)) unit_ = i;
    }

    spacer(ui);
    kit::rule(ui, Direction::Column);
    {
        Style footer;
        footer.direction = Direction::Column;
        footer.gap = 8.0f;
        footer.padding = Edges::all(12.0f);
        footer.shrink = 0.0f;
        auto footerScope = ui.begin(footer);
        kit::field(ui, "PLC", "Rockwell L83E · slot 0", Token::Text, FontRole::Mono);
        kit::field(ui, "SCAN TIME", "6.2 ms", Token::Text, FontRole::Mono);
        kit::field(ui, "OPC UA", "opc.tcp://helix:4840", Token::TextMuted, FontRole::Mono);
    }
}

void Scada::dials(Ui& ui) {
    auto card = kit::beginCard(ui, {.title = "PROCESS VARIABLES",
                                    .note = "1 s scan · engineering units",
                                    .direction = Direction::Row,
                                    .gap = 6.0f});

    const auto dial = [&](const kit::GaugeOptions& options) {
        Style cell;
        cell.direction = Direction::Column;
        cell.align = Align::Center;
        cell.gap = 4.0f;
        cell.grow = 1.0f;
        cell.basis = 0.0f;
        cell.minWidth = 0.0f;
        auto cellScope = ui.begin(cell);
        kit::gauge(ui, options);
    };

    dial({.value = flow_.latest(),
          .maximum = 600.0,
          .label = "FIT-101 FLOW",
          .unit = "m³/h",
          .tone = kit::toneBelow(flow_.latest(), 400.0, 380.0),
          .id = "scada.dial.flow",
          .size = 118.0f});
    dial({.value = pressure_.latest(),
          .maximum = 6.0,
          .label = "PIT-204 PRESSURE",
          .unit = "bar",
          .tone = kit::toneFor(pressure_.latest(), 4.0, 4.5),
          .id = "scada.dial.pressure",
          .size = 118.0f,
          .valueFormat = "%.2f"});
    dial({.value = turbidity_.latest(),
          .maximum = 0.5,
          .label = "AIT-301 TURBIDITY",
          .unit = "NTU",
          .tone = kit::toneFor(turbidity_.latest(), 0.25, 0.30),
          .id = "scada.dial.turbidity",
          .size = 118.0f,
          .valueFormat = "%.2f"});
    dial({.value = ph_.latest(),
          .minimum = 6.0,
          .maximum = 9.0,
          .label = "AIT-302 pH",
          .tone = kit::Tone::Ok,
          .id = "scada.dial.ph",
          .size = 118.0f,
          .valueFormat = "%.2f"});
    dial({.value = chlorine_.latest(),
          .maximum = 2.0,
          .label = "AIT-604 FREE CL",
          .unit = "mg/L",
          .tone = kit::toneBelow(chlorine_.latest(), 0.7, 0.5),
          .id = "scada.dial.chlorine",
          .size = 118.0f,
          .valueFormat = "%.2f"});
}

void Scada::vesselsAndPumps(Ui& ui, const Interaction& input) {
    {
        auto card = kit::beginCard(ui, {.title = "VESSEL LEVELS",
                                        .note = "% of working volume",
                                        .gap = 14.0f,
                                        .grow = 1.0f,
                                        .minWidth = 210.0f});

        kit::meter(ui, {.label = "TK-410 Clearwell",
                        .value = clearwell_.latest(),
                        .unit = "%",
                        .tone = kit::toneFor(clearwell_.latest(), 85.0, 92.0),
                        .id = "scada.tank.clearwell"});
        kit::meter(ui, {.label = "TK-420 Backwash",
                        .value = backwash_.latest(),
                        .unit = "%",
                        .tone = kit::toneBelow(backwash_.latest(), 25.0, 15.0),
                        .id = "scada.tank.backwash"});
        kit::meter(ui, {.label = "TK-320 Sludge",
                        .value = sludge_.latest(),
                        .unit = "%",
                        .tone = kit::toneFor(sludge_.latest(), 75.0, 85.0),
                        .id = "scada.tank.sludge"});
        kit::meter(ui, {.label = "TK-210 Coagulant",
                        .value = chemical_.latest(),
                        .unit = "%",
                        .tone = kit::toneBelow(chemical_.latest(), 20.0, 12.0),
                        .id = "scada.tank.chemical"});
    }
    {
        auto card =
            kit::beginCard(ui, {.title = "PUMPS AND SETPOINT", .gap = 8.0f, .width = 244.0f});

        for (std::size_t i = 0; i < pumpList_.size(); ++i) {
            const Pump& pump = pumpList_[i];
            const std::string tag = "scada.pump." + std::string(pump.tag);
            Style row;
            row.direction = Direction::Row;
            row.align = Align::Center;
            row.gap = 8.0f;
            row.height = 26.0f;
            auto rowScope = ui.begin(row);

            kit::statusDot(ui, pumps_[i] ? kit::Tone::Ok : kit::Tone::Neutral);
            {
                Style names;
                names.direction = Direction::Column;
                names.gap = 0.0f;
                names.grow = 1.0f;
                names.minWidth = 0.0f;
                auto namesScope = ui.begin(names);
                text(ui, pump.name, {.color = Token::Text, .size = 11.5f});
                text(ui, kit::format("%.0f h", pump.hours),
                     {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.0f});
            }
            if (switchToggle(ui, input, tag, pumps_[i])) pumps_[i] = !pumps_[i];
        }

        kit::rule(ui, Direction::Column);
        {
            Style row;
            row.direction = Direction::Row;
            row.align = Align::Center;
            row.gap = 8.0f;
            auto rowScope = ui.begin(row);
            text(ui, "FLOW SETPOINT", {.color = Token::TextMuted, .size = 10.0f, .grow = 1.0f});
            text(ui, kit::format("%.0f m³/h", setpoint_),
                 {.color = Token::TextStrong,
                  .weight = FontWeight::Medium,
                  .role = FontRole::Mono,
                  .size = 11.5f});
        }
        // `grow` is off, and it has to be: a slider defaults to taking the
        // free space on the main axis, which is width in a row and *height* in
        // a column — where it would push everything above it to the top of the
        // card and sit alone at the bottom.
        if (const SliderResult result = slider(ui, input, "scada.setpoint", setpoint_,
                                               {.minimum = 300.0,
                                                .maximum = 600.0,
                                                .step = 5.0,
                                                .disabled = autoMode_,
                                                .grow = 0.0f});
            result.changed) {
            setpoint_ = result.value;
        }
        spacer(ui);
    }
}

void Scada::trend(Ui& ui, const Interaction& input) {
    auto card = kit::beginCard(ui, {.title = "FIT-101 TREND · LAST 30 MINUTES",
                                    .note = "m³/h · drag to pan",
                                    .grow = 1.2f,
                                    .minWidth = 260.0f});
    // One quantity and the setpoint it is chasing. Three tags on one axis is
    // the usual mistake: a flow of 450 and a level of 22 % share a scale only
    // in the sense that both are numbers, and the level ends up a flat line
    // along the bottom.
    const std::vector<double> target(flow_.values().size(), setpoint_);
    const std::vector<Series> series = {
        {.name = "Flow", .values = flow_.values(), .color = Token::Graph1},
        {.name = "Setpoint",
         .values = target,
         .color = Token::Graph2,
         .fillAlpha = 0.0f,
         .thickness = 1.0f},
    };
    lineChart(ui, input, "scada.trend", series, view_, {.height = 168.0f, .valueFormat = "%.0f"},
              {.drag = true});
}

void Scada::commandLog(Ui& ui, const Interaction& input) {
    // Every HMI worth the name keeps one of these, and it is the part an
    // auditor asks for first: who changed what, when, and from which station.
    struct Entry {
        std::string_view time;
        std::string_view who;
        std::string_view what;
        kit::Tone tone;
    };
    static constexpr std::array<Entry, 8> entries = {{
        {"12:49:11", "r.almeida", "P-402A commanded STOP from HMI-2", kit::Tone::Warn},
        {"12:47:58", "system", "AIT-301 alarm raised · turbidity 0.31 NTU", kit::Tone::Alarm},
        {"12:44:02", "r.almeida", "Flow setpoint 440 → 460 m³/h", kit::Tone::Info},
        {"12:31:40", "system", "U-500 placed in BYPASS by interlock ILK-12", kit::Tone::Alarm},
        {"12:18:07", "j.moreira", "Backwash sequence started on filter 3", kit::Tone::Neutral},
        {"11:58:33", "system", "Historian checkpoint written · 14 d retained", kit::Tone::Ok},
        {"11:44:21", "r.almeida", "Coagulant dose 2.4 → 2.6 mg/L", kit::Tone::Info},
        {"11:20:44", "system", "AIT-604 alarm raised · free chlorine 0.42 mg/L", kit::Tone::Warn},
    }};

    auto card = kit::beginCard(ui, {.title = "COMMAND AND EVENT LOG",
                                    .note = "operator actions and system events",
                                    .gap = 0.0f,
                                    .grow = 1.0f});

    auto list = beginScroll(ui, input, "scada.log", logScroll_, {.gap = 0.0f});
    for (const Entry& entry : entries) {
        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 10.0f;
        row.height = 24.0f;
        row.shrink = 0.0f;
        auto rowScope = ui.begin(row);
        text(ui, entry.time, {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.5f});
        {
            Style who;
            who.width = 78.0f;
            who.shrink = 0.0f;
            auto whoScope = ui.begin(who);
            text(ui, entry.who,
                 {.color = entry.who == "system" ? Token::TextMuted : Token::Accent,
                  .role = FontRole::Mono,
                  .size = 10.5f});
        }
        kit::statusDot(ui, entry.tone, 6.0f);
        text(ui, entry.what, {.color = Token::Text, .size = 11.0f, .grow = 1.0f});
    }
}

void Scada::alarms(Ui& ui, const Interaction& input) {
    std::size_t unacknowledged = 0;
    for (const Alarm& alarm : alarmList_) {
        if (!alarm.acknowledged) ++unacknowledged;
    }

    auto card = kit::beginCard(
        ui, {.title = "ALARM SUMMARY",
             .note = kit::format("%.0f unacknowledged", static_cast<double>(unacknowledged)),
             .gap = 0.0f,
             .grow = 1.0f,
             .width = 320.0f});

    auto list = beginScroll(ui, input, "scada.alarms", alarmScroll_, {.gap = 2.0f});
    for (std::size_t i = 0; i < alarmList_.size(); ++i) {
        Alarm& alarm = alarmList_[i];
        const std::string tag = "scada.alarm." + std::to_string(i);
        const std::string ackTag = tag + ".ack";

        Style row;
        row.direction = Direction::Row;
        row.align = Align::Center;
        row.gap = 8.0f;
        row.height = 40.0f;
        row.shrink = 0.0f;
        row.padding = Edges::symmetric(0.0f, 8.0f);
        row.radius = 4.0f;
        // An unacknowledged alarm is washed in its own colour; an acknowledged
        // one keeps the colour only in its priority chip. That is the whole
        // visual grammar of an alarm list, and it is one `if`.
        if (!alarm.acknowledged) row.background = Fill{kit::toneToken(alarm.tone), 0.14f};
        auto rowScope = ui.begin(row);

        kit::beacon(ui, alarm.tone, !alarm.acknowledged, 7.0f);
        text(ui, kit::format("P%.0f", static_cast<double>(alarm.priority)),
             {.color = kit::toneToken(alarm.tone),
              .weight = FontWeight::SemiBold,
              .role = FontRole::Mono,
              .size = 10.5f});
        {
            Style column;
            column.direction = Direction::Column;
            column.gap = 1.0f;
            column.grow = 1.0f;
            column.minWidth = 0.0f;
            auto columnScope = ui.begin(column);
            text(ui, alarm.message,
                 {.color = alarm.acknowledged ? Token::Text : Token::TextStrong,
                  .weight = alarm.acknowledged ? FontWeight::Regular : FontWeight::Medium,
                  .size = 11.5f});
            {
                Style meta;
                meta.direction = Direction::Row;
                meta.gap = 8.0f;
                auto metaScope = ui.begin(meta);
                text(ui, alarm.time,
                     {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.0f});
                text(ui, alarm.tag,
                     {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.0f});
            }
        }
        if (alarm.acknowledged) {
            icon(ui, Icon::Check, {.color = Token::TextMuted, .size = 14.0f});
        } else {
            button(ui, input, "ACK",
                   {.variant = ButtonVariant::Secondary, .height = 24.0f, .id = ackTag});
        }
        rowScope.close();
        if (input.clicked(ackTag)) alarm.acknowledged = true;
    }
}

NodeId Scada::build(Frame& frame) {
    Ui& ui = frame.ui;
    const Interaction& input = frame.input;

    flow_.advance(frame.time);
    pressure_.advance(frame.time);
    turbidity_.advance(frame.time);
    ph_.advance(frame.time);
    chlorine_.advance(frame.time);
    clearwell_.advance(frame.time);
    backwash_.advance(frame.time);
    sludge_.advance(frame.time);
    chemical_.advance(frame.time);

    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.begin(window);

    {
        auto header = kit::beginHeader(ui, Icon::Settings, "Helix Process Control",
                                       "Marecchia WTP · shift B · operator: r.almeida");
        spacer(ui);
        kit::pill(ui, "3 UNACK", {.tone = kit::Tone::Alarm});
        {
            Style toggle;
            toggle.direction = Direction::Row;
            toggle.align = Align::Center;
            toggle.gap = 8.0f;
            toggle.shrink = 0.0f;
            auto toggleScope = ui.begin(toggle);
            text(ui, autoMode_ ? "AUTO" : "MANUAL",
                 {.color = autoMode_ ? Token::Added : Token::Modified,
                  .weight = FontWeight::SemiBold,
                  .size = 10.5f});
            if (switchToggle(ui, input, "scada.auto", autoMode_)) autoMode_ = !autoMode_;
        }
        button(
            ui, input, "HOLD",
            {.variant = ButtonVariant::Danger, .leading = Icon::CircleAlert, .id = "scada.hold"});
    }
    kit::rule(ui, Direction::Column);

    {
        Style body;
        body.direction = Direction::Row;
        body.grow = 1.0f;
        body.basis = 0.0f;
        auto bodyScope = ui.begin(body);

        units(ui, input);
        kit::rule(ui, Direction::Row);

        {
            Style centre;
            centre.direction = Direction::Column;
            centre.gap = 12.0f;
            centre.padding = Edges::all(12.0f);
            centre.grow = 1.0f;
            centre.basis = 0.0f;
            centre.minWidth = 0.0f;
            auto centreScope = ui.begin(centre);
            dials(ui);
            {
                Style row;
                row.direction = Direction::Row;
                row.gap = 12.0f;
                row.height = 262.0f;
                row.shrink = 0.0f;
                auto rowScope = ui.begin(row);
                vesselsAndPumps(ui, input);
                trend(ui, input);
            }
            commandLog(ui, input);
        }

        {
            Style right;
            right.direction = Direction::Column;
            right.padding = Edges{12.0f, 12.0f, 12.0f, 0.0f};
            right.shrink = 0.0f;
            auto rightScope = ui.begin(right);
            alarms(ui, input);
        }
    }

    kit::rule(ui, Direction::Column);
    {
        auto bar = kit::beginStatusBar(ui);
        kit::statusItem(ui, Icon::Terminal, "PLC link OK · redundancy A", kit::Tone::Ok);
        kit::statusItem(ui, Icon::ClockFading, "scan 6.2 ms");
        kit::statusItem(ui, Icon::Archive, "historian 14 d buffered");
        spacer(ui);
        kit::statusItem(ui, Icon::CircleAlert, "3 unacknowledged", kit::Tone::Alarm);
        text(ui, "HELIX 7.4 · ISA-101", {.color = Token::TextMuted, .size = 10.5f});
    }
    return root.id();
}

}  // namespace

DemoInfo scadaDemo() {
    return {
        .id = "scada",
        .title = "Helix Process Control",
        .sector = "Water treatment · SCADA / HMI",
        .summary =
            "A plant supervisory screen: process dials against their setpoints, tank "
            "meters, pump switches and an alarm list you can acknowledge.",
        .highlights = {"Always dark", "Live dials", "Working switches", "Acknowledgeable alarms"},
        .tryThis =
            "Switch a pump off, drag the flow setpoint and watch the orange line in the "
            "trend follow it, then acknowledge one of the alarms on the right.",
        .design = {1360.0f, 800.0f},
        .palette = Palette::Dark,
        .create = [] { return std::unique_ptr<Demo>(new Scada()); }};
}

}  // namespace gbui::demos
