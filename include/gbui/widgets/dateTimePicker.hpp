// A calendar and a clock, chosen together.
#pragma once

#include <string>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/datePicker.hpp"
#include "gbui/widgets/timePicker.hpp"

namespace gbui {

/** A day and a time of day. Two members rather than a count of seconds because
 *  the two are chosen separately and almost always displayed separately. */
struct DateTime {
    Date date{};
    Time time{};

    bool valid() const { return date.valid() && time.valid(); }

    friend bool operator==(const DateTime& a, const DateTime& b) {
        return a.date == b.date && a.time == b.time;
    }
    friend bool operator!=(const DateTime& a, const DateTime& b) { return !(a == b); }
    friend bool operator<(const DateTime& a, const DateTime& b) {
        if (a.date != b.date) return a.date < b.date;
        return a.time < b.time;
    }
};

/**
 * Formats a date and a time from one pattern.
 *
 * The same grammar as `formatDate` and `formatTime` — it is those two, over one
 * string — so `dd/MM/yyyy HH:mm` and `EEE, d MMM 'at' h:mm a` both work and
 * neither needs the caller to concatenate. The date letters come from the
 * calendar's locale and the clock letters from the clock's, which is why both
 * are taken.
 */
std::string formatDateTime(const DateTime& when, std::string_view pattern,
                           const CalendarLocale& calendar = {}, const ClockLocale& clock = {});

struct DateTimePickerState {
    DatePickerState calendar{};
    TimePickerState clock{};
    /** Where the open pair is scrolled to, when it opened somewhere too short
     *  for it — a calendar and a clock need more room than either alone. Its
     *  own rather than the calendar's, because the field embeds the inline
     *  calendar and never opens the calendar's own popover. */
    ScrollState popup;
};

struct DateTimePickerOptions {
    DatePickerOptions date{};
    TimePickerOptions time{};
    /** The clock beside the calendar, or under it. Beside is the default
     *  because a calendar is wider than it is tall and the pair balances. */
    bool sideBySide = true;
    float gap = 16.0f;
};

struct DateTimePickerResult {
    /** Either half changed this frame. */
    bool changed = false;
    DateTime when{};
};

/** The two pickers, sharing one value. */
DateTimePickerResult dateTimePicker(Ui& ui, const Interaction& input, std::string_view id,
                                    const DateTime& selected, DateTimePickerState& state,
                                    const DateTimePickerOptions& options = {});

struct DateTimeFieldOptions : DateTimePickerOptions {
    std::string_view placeholder = "Select a date and time…";
    /**
     * How the value is written on the trigger.
     *
     * One pattern for both halves, in the grammar `formatDate` and `formatTime`
     * share — the date tokens are read by one and the clock tokens by the
     * other, so `dd/MM/yyyy HH:mm` needs no separate spelling of where the date
     * ends and the time begins.
     */
    std::string_view pattern = "dd/MM/yyyy HH:mm";
    bool clearable = true;
    bool disabled = false;
    float width = 240.0f;
    float height = 0.0f;   ///< Zero takes the design's control height.
    bool dismissOnOutsideClick = true;
    bool dismissOnEscape = true;
};

struct DateTimeFieldResult {
    bool changed = false;
    DateTime when{};
};

/** What the field remembers: the pickers' state, and whether it is open. */
struct DateTimeFieldState : DateTimePickerState {
    bool open = false;
};

/**
 * A calendar and a clock behind one input.
 *
 * The pair belongs behind a single field far more often than beside each other
 * on a page — a due date, a scheduled run, a deadline are one value, and asking
 * for it in two controls invites half of it being filled in.
 */
DateTimeFieldResult dateTimeField(Ui& ui, const Interaction& input, std::string_view id,
                                  const DateTime& selected, DateTimeFieldState& state,
                                  const DateTimeFieldOptions& options = {});

}  // namespace gbui
