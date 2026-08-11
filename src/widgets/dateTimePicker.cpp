#include "gbui/widgets/dateTimePicker.hpp"

#include <string>

#include "detail.hpp"
#include "gbui/widgets/icon.hpp"
#include "gbui/widgets/popover.hpp"
#include "gbui/widgets/text.hpp"

namespace gbui {

using namespace detail;
namespace {

/** Which of the two formatters owns a pattern letter. Anything neither claims
 *  is copied through, which is what makes separators and words work. */
bool isDateLetter(char c) { return c == 'y' || c == 'M' || c == 'd' || c == 'E'; }
bool isTimeLetter(char c) { return c == 'H' || c == 'h' || c == 'm' || c == 's' || c == 'a'; }

}  // namespace

std::string formatDateTime(const DateTime& when, std::string_view pattern,
                           const CalendarLocale& calendar, const ClockLocale& clock) {
    std::string out;
    if (!when.valid()) return out;

    // Walked once, handing each run of letters to whichever formatter owns it.
    // Splitting the pattern in two and joining the results would need a rule for
    // where to cut, and there is no such rule: `HH'h' dd/MM` interleaves them.
    for (std::size_t i = 0; i < pattern.size();) {
        const char c = pattern[i];
        if (c == '\'') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '\'') {
                out.push_back('\'');
                i += 2;
                continue;
            }
            ++i;
            while (i < pattern.size() && pattern[i] != '\'') out.push_back(pattern[i++]);
            if (i < pattern.size()) ++i;
            continue;
        }

        std::size_t count = 1;
        while (i + count < pattern.size() && pattern[i + count] == c) ++count;
        const std::string_view run = pattern.substr(i, count);
        if (isDateLetter(c)) out += formatDate(when.date, run, calendar);
        else if (isTimeLetter(c)) out += formatTime(when.time, run, clock);
        else out.append(count, c);
        i += count;
    }
    return out;
}

DateTimePickerResult dateTimePicker(Ui& ui, const Interaction& input, std::string_view id,
                                    const DateTime& selected, DateTimePickerState& state,
                                    const DateTimePickerOptions& options) {
    DateTimePickerResult result;
    result.when = selected;

    Style frame;
    frame.direction = options.sideBySide ? Direction::Row : Direction::Column;
    frame.gap = options.gap;
    frame.align = Align::Start;
    auto scope = ui.begin(frame);
    ui.tag(id);

    const DatePickerResult day =
        datePicker(ui, input, std::string(id) + ".date", selected.date, state.calendar,
                   options.date);
    if (day.chosen) {
        result.when.date = day.date;
        result.changed = true;
    }

    // The clock takes the calendar's height when the two stand side by side.
    // A calendar's height is decided by how many weeks the month spans, so it
    // is not a number anyone can write down in advance — it has to be read back
    // from last frame, like every other piece of geometry here. Without it the
    // columns kept their own fixed height and the pair came out ragged.
    TimePickerOptions clock = options.time;
    if (options.sideBySide) {
        const Rect calendar = input.frameOf(std::string(id) + ".date");
        if (calendar.height > 0.0f) clock.height = calendar.height;
    }

    const TimePickerResult picked =
        timePicker(ui, input, std::string(id) + ".time", selected.time, state.clock, clock);
    if (picked.changed) {
        result.when.time = picked.time;
        result.changed = true;
    }
    (void)scope;

    return result;
}


DateTimeFieldResult dateTimeField(Ui& ui, const Interaction& input, std::string_view id,
                                  const DateTime& selected, DateTimeFieldState& state,
                                  const DateTimeFieldOptions& options) {
    DateTimeFieldResult result;
    result.when = selected;

    const std::string triggerId = std::string(id) + ".trigger";
    const std::string clearId = std::string(id) + ".clear";
    const bool hasValue = selected.valid();

    Style trigger;
    trigger.direction = Direction::Row;
    trigger.align = Align::Center;
    trigger.gap = 8.0f;
    trigger.width = options.width;
    trigger.minHeight = options.height > 0.0f ? options.height : ui.design().controlHeight;
    trigger.padding = Edges::symmetric(0.0f, 10.0f);
    trigger.radius = ui.design().controlRadius;
    const FieldPalette palette =
        paletteForField(options.disabled, false, input.isHovered(triggerId));
    trigger.background = palette.background;
    trigger.border = Border{1.0f, Fill{palette.border}};
    if (input.isFocusVisible(triggerId)) trigger.outline = Outline{2.0f, 2.0f, Fill{Token::Accent}};
    trigger.opacity = opacityFor(options.disabled);
    trigger.cursorHint = options.disabled ? Cursor::NotAllowed : Cursor::Pointer;

    {
        auto scope = ui.begin(trigger);
        ui.tag(triggerId).focusable(!options.disabled).cursor(trigger.cursorHint);
        icon(ui, Icon::ClockFading, {.color = Token::TextMuted, .size = 14.0f});
        // Muted for the placeholder and not for the value: that contrast is the
        // whole of how a reader tells "nothing chosen" from "chosen".
        text(ui,
             hasValue ? formatDateTime(selected, options.pattern, options.date.locale,
                                       options.time.locale)
                      : std::string(options.placeholder),
             {.color = hasValue ? Token::Text : Token::TextMuted, .size = 12.0f, .grow = 1.0f});
        if (options.clearable && hasValue && !options.disabled) {
            Style clear;
            clear.width = 18.0f;
            clear.height = 18.0f;
            clear.shrink = 0.0f;
            clear.radius = 4.0f;
            clear.justify = Justify::Center;
            clear.align = Align::Center;
            if (input.isHovered(clearId)) clear.background = Fill{Token::SurfaceHover};
            clear.cursorHint = Cursor::Pointer;
            auto clearScope = ui.begin(clear);
            ui.tag(clearId).cursor(Cursor::Pointer);
            icon(ui, Icon::X, {.color = Token::TextMuted, .size = 11.0f});
            (void)clearScope;
        } else {
            icon(ui, Icon::ChevronDown, {.color = Token::TextMuted, .size = 13.0f});
        }
        (void)scope;
    }

    if (options.disabled) return result;
    if (input.clicked(clearId)) {
        result.when = DateTime{};
        result.changed = true;
        return result;
    }
    if (activated(input, triggerId, false)) state.open = !state.open;
    if (!state.open) return result;

    PopoverOptions popover;
    popover.placement = Placement::Bottom;
    // Wider than the date field's: this holds a calendar *and* the clock
    // columns beside it, and squeezing them would stack the pair vertically
    // just as the reader opened it.
    popover.minWidth = 420.0f;
    popover.maxWidth = 560.0f;
    popover.padding = Edges::all(10.0f);
    popover.scroll = ScrollAxis::Vertical;

    auto surface = beginPopover(ui, input, std::string(id) + ".popover", triggerId, popover);
    const DateTimePickerResult picked = dateTimePicker(
        ui, input, std::string(id) + ".picker", result.when.valid() ? result.when : selected,
        state, options);
    if (picked.changed) {
        result.when = picked.when;
        result.changed = true;
    }
    (void)surface;

    // Not closed on picking, unlike the date field: a date *and* a time is two
    // choices, and shutting the popover after the first one means reopening it
    // to make the second.
    // "Inside" is a prefix test on the hovered tag, because the popover's
    // children are named after it — a click on the calendar, on a clock column
    // or on the trigger is not an outside click.
    const std::string surfaceId = std::string(id) + ".popover";
    const std::string_view hovered = input.hovered();
    const bool inside = (hovered.size() >= surfaceId.size() &&
                         hovered.substr(0, surfaceId.size()) == surfaceId) ||
                        (hovered.size() >= triggerId.size() &&
                         hovered.substr(0, triggerId.size()) == triggerId);
    if (options.dismissOnOutsideClick && input.pointerDown() && input.dragging().empty() &&
        !inside) {
        state.open = false;
    }
    if (options.dismissOnEscape) {
        for (const KeyEvent& event : input.keys()) {
            if (event.key == Key::Escape) state.open = false;
        }
    }
    return result;
}

}  // namespace gbui
