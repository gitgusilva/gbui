#include "gbui/widgets/select.hpp"

#include <string>
#include <vector>

#include "gbui/input/interaction.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/style/theme.hpp"
#include "gbui/widgets/checkbox.hpp"
#include <cstdlib>
#include <utility>
#include "gbui/widgets/controls.hpp"
#include "harness.hpp"

using namespace gbui;

namespace {

const Rect kWindow{0, 0, 400, 500};

/** A select with five options, and a checkbox after it so Tab has somewhere
 *  else to go. Driven the way the frame loop drives it: resolve, then build. */
struct Combo {
    Theme theme = Theme::dark();
    Arena arena;
    Interaction input;
    SelectState state;
    SelectOptions options{};
    std::vector<std::string> items{"main", "feat/a", "feat/b", "fix/c", "release/1"};
    std::optional<std::size_t> value = 0;
    /** What the last frame reported, so a test can assert on one press. */
    std::optional<std::size_t> chosen;

    void frame(const InputFrame& event = {}) {
        input.update(arena, arena.empty() ? NodeId{} : NodeId(0), event);

        arena.reset();
        Ui ui(arena);
        {
            auto root = ui.column({.gap = 6.0f, .padding = Edges::all(10.0f)});
            const SelectResult result =
                select(ui, input, "sel", items, value, state, options);
            chosen = result.chosen;
            if (result.chosen) value = result.chosen;
            (void)checkbox(ui, input, "after", false, {.label = "something else"});
            (void)root;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), kWindow, context);
    }

    void press(Key key, bool shift = false) {
        InputFrame event;
        event.keys.push_back({key, {.shift = shift}});
        frame(event);
    }

    /** Puts the keyboard on the select, the way Tab would. */
    void focus() {
        press(Key::Tab);
        frame();
    }

    void clickAt(Vec2 point) {
        InputFrame down;
        down.pointer = point;
        down.pointerDown = true;
        frame(down);

        InputFrame up;
        up.pointer = point;
        frame(up);
    }

    Vec2 centreOf(std::string_view tag) const {
        const Rect r = input.frameOf(tag);
        return {r.x + r.width / 2.0f, r.y + r.height / 2.0f};
    }
};

}  // namespace

TEST("opening a list highlights the value, so Return changes nothing") {
    Combo combo;
    combo.frame();
    combo.focus();
    CHECK(combo.input.isFocused("sel"));

    combo.press(Key::Return);
    CHECK(combo.state.open);
    CHECK_EQ(combo.state.highlighted.value_or(99), std::size_t{0});

    combo.press(Key::Return);
    CHECK(!combo.state.open);
    CHECK_EQ(combo.value.value_or(99), std::size_t{0});
    CHECK(!combo.state.highlighted.has_value());
}

TEST("Up and Down walk the open list without changing the value") {
    Combo combo;
    combo.frame();
    combo.focus();
    combo.press(Key::Return);

    combo.press(Key::Down);
    combo.press(Key::Down);
    CHECK_EQ(combo.state.highlighted.value_or(99), std::size_t{2});
    // Walking is not choosing: the value is still what it was.
    CHECK_EQ(combo.value.value_or(99), std::size_t{0});
    CHECK(!combo.chosen.has_value());

    combo.press(Key::Up);
    CHECK_EQ(combo.state.highlighted.value_or(99), std::size_t{1});

    // And it wraps at both ends rather than stopping dead.
    combo.press(Key::Up);
    combo.press(Key::Up);
    CHECK_EQ(combo.state.highlighted.value_or(99), std::size_t{4});
    combo.press(Key::Down);
    CHECK_EQ(combo.state.highlighted.value_or(99), std::size_t{0});
}

TEST("Return commits the highlighted row and closes the list") {
    Combo combo;
    combo.frame();
    combo.focus();
    combo.press(Key::Return);
    combo.press(Key::Down);
    combo.press(Key::Down);

    combo.press(Key::Return);
    CHECK_EQ(combo.chosen.value_or(99), std::size_t{2});
    CHECK_EQ(combo.value.value_or(99), std::size_t{2});
    CHECK(!combo.state.open);
}

TEST("Escape closes the list and leaves the value alone") {
    Combo combo;
    combo.frame();
    combo.focus();
    combo.press(Key::Return);
    combo.press(Key::Down);
    combo.press(Key::Down);

    combo.press(Key::Escape);
    CHECK(!combo.state.open);
    CHECK(!combo.chosen.has_value());
    CHECK_EQ(combo.value.value_or(99), std::size_t{0});
}

TEST("Home and End jump to the ends of the open list") {
    Combo combo;
    combo.frame();
    combo.focus();
    combo.press(Key::Return);

    combo.press(Key::End);
    CHECK_EQ(combo.state.highlighted.value_or(99), std::size_t{4});
    combo.press(Key::Home);
    CHECK_EQ(combo.state.highlighted.value_or(99), std::size_t{0});
}

TEST("closed, the arrows step the value itself") {
    Combo combo;
    combo.frame();
    combo.focus();

    combo.press(Key::Down);
    CHECK(!combo.state.open);
    CHECK_EQ(combo.value.value_or(99), std::size_t{1});
    combo.press(Key::Up);
    CHECK_EQ(combo.value.value_or(99), std::size_t{0});
}

TEST("Tab leaves the control instead of walking into the open list") {
    Combo combo;
    combo.frame();
    combo.focus();
    combo.press(Key::Return);
    CHECK(combo.state.open);

    combo.press(Key::Tab);
    // The rows are drawn and clickable, but they are not places Tab can land:
    // the box keeps the keyboard while its list is open.
    CHECK(combo.input.isFocused("after"));
    CHECK(!combo.input.isFocused("sel.list.0"));
}

TEST("the list scrolls to keep the highlight in view") {
    Combo combo;
    combo.options.maxVisible = 2;   // two rows visible out of five
    combo.frame();
    combo.focus();
    combo.press(Key::Return);
    combo.frame();
    CHECK_NEAR(combo.state.list.offset, 0.0f);

    combo.press(Key::End);
    combo.frame();
    // The last row is below the fold, so the list scrolled to it — and no
    // further than that.
    CHECK(combo.state.list.offset > 0.0f);
    CHECK_NEAR(combo.state.list.offset, combo.state.list.maxOffset());

    combo.press(Key::Home);
    combo.frame();
    CHECK_NEAR(combo.state.list.offset, 0.0f);
}

TEST("clicking a row chooses it and closes the list") {
    Combo combo;
    combo.frame();
    combo.focus();
    combo.press(Key::Return);
    combo.frame();

    combo.clickAt(combo.centreOf("sel.list.3"));
    CHECK_EQ(combo.value.value_or(99), std::size_t{3});
    CHECK(!combo.state.open);
}

// ---- pickers ---------------------------------------------------------------

TEST("a date knows its own arithmetic") {
    // The parts a calendar grid leans on, which is the whole reason `Date`
    // converts to `std::chrono` rather than counting days itself.
    CHECK((Date{2026, 8, 11}.valid()));
    CHECK((!Date{2026, 2, 30}.valid()));
    CHECK((!Date{2026, 13, 1}.valid()));

    // A leap year, and the day after it.
    CHECK((Date{2024, 2, 29}.valid()));
    CHECK((!Date{2025, 2, 29}.valid()));
    CHECK((Date::fromSerial(Date{2024, 2, 28}.serial() + 1) == Date{2024, 2, 29}));
    CHECK((Date::fromSerial(Date{2026, 12, 31}.serial() + 1) == Date{2027, 1, 1}));

    CHECK((Date{2026, 1, 1} < Date{2026, 1, 2}));
    CHECK((Date{2025, 12, 31} < Date{2026, 1, 1}));
}

TEST("a date picker draws a whole month and reports the day clicked") {
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    Interaction input;
    DatePickerState state;
    state.visible = Date{2026, 8, 1};

    const auto frame = [&](InputFrame events) {
        arena.reset();
        Ui rebuilt(arena);
        DatePickerResult result;
        {
            auto column = rebuilt.column({.width = 300.0f});
            DatePickerOptions options;
            options.minimum = Date{2026, 8, 5};
            options.maximum = Date{2026, 8, 20};
            result = datePicker(rebuilt, input, "cal", Date{2026, 8, 11}, state, options);
            (void)column;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, rebuilt.root(), Rect{0, 0, 300, 400}, context);
        input.update(arena, rebuilt.root(), events);
        return result;
    };

    frame({});
    frame({});

    // Every day of August is on screen, keyed by its serial.
    const Rect eleventh = input.frameOf("cal." + std::to_string(Date{2026, 8, 11}.serial()));
    const Rect first = input.frameOf("cal." + std::to_string(Date{2026, 8, 1}.serial()));
    CHECK(!eleventh.empty());
    CHECK(!first.empty());
    // The 1st is a Saturday in August 2026, so it sits in the first row and the
    // grid begins with the tail of July rather than with a hole.
    CHECK_NEAR(first.y, eleventh.y - eleventh.height * 2.0f - 4.0f);

    // Clicking a day inside the bounds chooses it. The click lands on the
    // release and is *read* on the frame after, because the build runs before
    // the update — the same order a real loop uses.
    InputFrame press;
    press.pointer = {eleventh.x + 5.0f, eleventh.y + 5.0f};
    press.pointerDown = true;
    frame(press);
    InputFrame release;
    release.pointer = press.pointer;
    frame(release);
    CHECK(frame({}).chosen);

    // …and one outside them does not.
    const Rect outside = input.frameOf("cal." + std::to_string(Date{2026, 8, 25}.serial()));
    CHECK(!outside.empty());  // drawn, not hidden
    InputFrame pressOut;
    pressOut.pointer = {outside.x + 5.0f, outside.y + 5.0f};
    pressOut.pointerDown = true;
    frame(pressOut);
    InputFrame releaseOut;
    releaseOut.pointer = pressOut.pointer;
    frame(releaseOut);
    CHECK(!frame({}).chosen);
}

TEST("hsv survives the trip a picker actually makes") {
    // Round-tripping a saturated colour is exact enough to edit with.
    const Color samples[] = {Color{37, 99, 235}, Color{34, 197, 94}, Color{255, 171, 0}};
    for (const Color original : samples) {
        const Color back = Hsv::fromColor(original).toColor();
        CHECK((std::abs(int{back.r} - int{original.r}) <= 1));
        CHECK((std::abs(int{back.g} - int{original.g}) <= 1));
        CHECK((std::abs(int{back.b} - int{original.b}) <= 1));
    }
    // …and the reason the picker keeps `Hsv` rather than a `Color`: a grey has
    // no hue to come back as, so a round trip through RGB would lose it.
    const Hsv grey = Hsv::fromColor(Color{128, 128, 128});
    CHECK_NEAR(grey.saturation, 0.0f);
    CHECK_NEAR(grey.hue, 0.0f);
}

TEST("a colour picker's rails can be turned off one at a time") {
    const auto build = [&](const ColorPickerOptions& options) {
        Arena arena;
        Ui ui(arena);
        Theme theme = Theme::dark();
        Interaction input;
        ColorPickerState state;
        state.set(Color{37, 99, 235});
        {
            auto column = ui.column({.width = 300.0f});
            colorPicker(ui, input, "pick", state, options);
            (void)column;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, ui.root(), Rect{0, 0, 300, 400}, context);
        input.update(arena, ui.root(), InputFrame{});
        return std::pair{!input.frameOf("pick.hue").empty(),
                         !input.frameOf("pick.alpha").empty()};
    };

    CHECK(build({}).first);
    CHECK(build({}).second);
    CHECK((!build({.alpha = false}).second));
    CHECK((build({.alpha = false}).first));
    CHECK((!build({.hue = false}).first));
}

TEST("a date pattern decides the shape and the locale decides the words") {
    const Date day{2026, 8, 11};  // a Tuesday

    // The same date, three ways, from three patterns — which is the whole point
    // of a pattern rather than a fixed set of styles.
    CHECK_EQ(formatDate(day, "dd/MM/yyyy"), std::string("11/08/2026"));
    CHECK_EQ(formatDate(day, "MM/dd/yy"), std::string("08/11/26"));
    CHECK_EQ(formatDate(day, "yyyy-MM-dd"), std::string("2026-08-11"));
    CHECK_EQ(formatDate(day, "d/M/yy"), std::string("11/8/26"));

    // Names come from the locale, so the pattern is language-independent.
    CHECK_EQ(formatDate(day, "d MMMM yyyy"), std::string("11 August 2026"));
    CHECK_EQ(formatDate(day, "EEE, d MMM"), std::string("Tue, 11 Aug"));

    CalendarLocale ptBr;
    ptBr.months = {"janeiro", "fevereiro", "março",    "abril",   "maio",     "junho",
                   "julho",   "agosto",    "setembro", "outubro", "novembro", "dezembro"};
    ptBr.weekdayNames = {"domingo", "segunda-feira", "terça-feira", "quarta-feira",
                         "quinta-feira", "sexta-feira", "sábado"};
    // Quoted text is copied out, which is how "de" survives without its `d`
    // being read as a day.
    CHECK_EQ(formatDate(day, "d 'de' MMMM 'de' yyyy", ptBr),
             std::string("11 de agosto de 2026"));
    CHECK_EQ(formatDate(day, "dd/MM/yyyy", ptBr), std::string("11/08/2026"));

    // An invalid date formats to nothing rather than to a wrong date.
    CHECK(formatDate(Date{}, "dd/MM/yyyy").empty());
}

TEST("a time pattern spells the same instant several ways") {
    const Time afternoon{14, 30, 5};

    CHECK_EQ(formatTime(afternoon, "HH:mm"), std::string("14:30"));
    CHECK_EQ(formatTime(afternoon, "HH:mm:ss"), std::string("14:30:05"));
    CHECK_EQ(formatTime(afternoon, "h:mm a"), std::string("2:30 PM"));
    CHECK_EQ(formatTime(afternoon, "hh:mm a"), std::string("02:30 PM"));
    // Quoted text survives, so a French-style "14h30" needs no concatenation.
    CHECK_EQ(formatTime(afternoon, "HH'h'mm"), std::string("14h30"));

    // Midnight and noon are the case a plain modulo gets wrong: both are 12.
    CHECK_EQ(formatTime(Time{0, 5, 0}, "h:mm a"), std::string("12:05 AM"));
    CHECK_EQ(formatTime(Time{12, 5, 0}, "h:mm a"), std::string("12:05 PM"));
    CHECK_EQ(formatTime(Time{0, 5, 0}, "HH:mm"), std::string("00:05"));

    ClockLocale ptBr{"da manhã", "da tarde"};
    CHECK_EQ(formatTime(afternoon, "h:mm a", ptBr), std::string("2:30 da tarde"));
}

TEST("seconds of the day round-trip") {
    CHECK((Time::fromSeconds(Time{23, 59, 59}.secondsOfDay()) == Time{23, 59, 59}));
    CHECK((Time::fromSeconds(0) == Time{0, 0, 0}));
    // Wrapping rather than clamping: a step past midnight is the next day's
    // start, which is what stepping a clock means.
    CHECK((Time::fromSeconds(Time{23, 59, 59}.secondsOfDay() + 1) == Time{0, 0, 0}));
    CHECK((Time::fromSeconds(-1) == Time{23, 59, 59}));
}

TEST("one pattern spans a date and a time") {
    const DateTime when{Date{2026, 8, 11}, Time{14, 30, 0}};

    CHECK_EQ(formatDateTime(when, "dd/MM/yyyy HH:mm"), std::string("11/08/2026 14:30"));
    CHECK_EQ(formatDateTime(when, "yyyy-MM-dd'T'HH:mm:ss"), std::string("2026-08-11T14:30:00"));
    // Interleaved, which is why the pattern is walked once rather than split in
    // two and joined: there is no place to cut this one.
    CHECK_EQ(formatDateTime(when, "HH'h' dd/MM"), std::string("14h 11/08"));
    CHECK_EQ(formatDateTime(when, "EEE, d MMM 'at' h:mm a"),
             std::string("Tue, 11 Aug at 2:30 PM"));

    // An incomplete value formats to nothing rather than to a plausible lie.
    CHECK(formatDateTime(DateTime{Date{}, Time{14, 0, 0}}, "dd/MM/yyyy HH:mm").empty());
}

TEST("twelve-hour is a display, so the stored hour does not move") {
    Arena arena;
    Ui ui(arena);
    Theme theme = Theme::dark();
    Interaction input;
    TimePickerState state;

    const auto build = [&](const Time& value, bool use24Hour) {
        arena.reset();
        Ui rebuilt(arena);
        TimePickerResult result;
        {
            auto column = rebuilt.column({.width = 320.0f});
            TimePickerOptions options;
            options.use24Hour = use24Hour;
            result = timePicker(rebuilt, input, "clock", value, state, options);
            (void)column;
        }
        LayoutContext context;
        context.theme = &theme;
        layout(arena, rebuilt.root(), Rect{0, 0, 320, 200}, context);
        input.update(arena, rebuilt.root(), InputFrame{});
        return result;
    };

    // 14:30 shown on a twelve-hour picker highlights "2" and PM, and reports
    // back the very same time — the display does not round-trip through 2:00.
    const TimePickerResult shown = build(Time{14, 30, 0}, false);
    CHECK((shown.time == Time{14, 30, 0}));
    CHECK(!shown.changed);
    CHECK(!input.frameOf("clock.pm").empty());
    CHECK(input.frameOf("clock.am").width > 0.0f);

    // The AM/PM column only exists on a twelve-hour picker.
    build(Time{14, 30, 0}, true);
    CHECK(input.frameOf("clock.pm").empty());
}
