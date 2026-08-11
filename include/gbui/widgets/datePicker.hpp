// A calendar: a month of days, walked with the pointer or the keyboard.
#pragma once

#include <array>
#include <string>
#include <string_view>

#include "gbui/input/interaction.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui {

/**
 * A day, with no time and no zone.
 *
 * Deliberately not `std::chrono::year_month_day`: that type is excellent for
 * the arithmetic and is used for exactly that inside, but putting it in the
 * public surface would push a `<chrono>` include into every call site for the
 * sake of three integers. This converts to it and back where the work happens.
 */
struct Date {
    int year = 0;
    int month = 0;  ///< 1..12
    int day = 0;    ///< 1..31

    bool valid() const;
    /** Days since 1970-01-01, so two dates can be compared and stepped without
     *  the caller knowing how long a month is. */
    long long serial() const;
    static Date fromSerial(long long days);
    /** Today, from the system clock. */
    static Date today();

    friend bool operator==(const Date& a, const Date& b) {
        return a.year == b.year && a.month == b.month && a.day == b.day;
    }
    friend bool operator!=(const Date& a, const Date& b) { return !(a == b); }
    friend bool operator<(const Date& a, const Date& b) { return a.serial() < b.serial(); }
};

/**
 * The words a calendar needs, and the week's shape.
 *
 * These are options rather than a lookup, and that is the honest answer to the
 * locale problem: a first day of the week, twelve month names and seven day
 * initials are the whole of what a calendar grid needs, and shipping a locale
 * database to derive them would be a far larger dependency than the widget.
 * The defaults are English with Monday first; an application that knows its
 * user's locale passes its own.
 */
struct CalendarLocale {
    /** 0 is Sunday, 1 Monday — the ISO default here. */
    int firstDayOfWeek = 1;
    std::array<std::string_view, 12> months{"January", "February", "March",     "April",
                                            "May",     "June",     "July",      "August",
                                            "September", "October", "November", "December"};
    /** Indexed from Sunday, whatever `firstDayOfWeek` says. */
    std::array<std::string_view, 7> weekdays{"S", "M", "T", "W", "T", "F", "S"};
    /** Full weekday names, for `EEEE` in a pattern. Indexed from Sunday. */
    std::array<std::string_view, 7> weekdayNames{"Sunday",   "Monday", "Tuesday", "Wednesday",
                                                 "Thursday", "Friday", "Saturday"};
};

/**
 * Formats a date by a pattern, the way every date library spells it.
 *
 * The pattern decides the order, the separators and the words, so one function
 * covers `dd/MM/yyyy`, `MM/dd/yy` and `d 'de' MMMM 'de' yyyy` without the
 * caller assembling strings. Recognised, longest first:
 *
 * | | |
 * |---|---|
 * | `yyyy` `yy`   | year, four digits or two |
 * | `MMMM` `MMM`  | month by name, full or first three letters |
 * | `MM` `M`      | month by number, padded or not |
 * | `dd` `d`      | day, padded or not |
 * | `EEEE` `EEE`  | weekday by name, full or short |
 *
 * Anything else is copied through, and text inside single quotes is copied
 * verbatim — which is how a pattern says "de" in Portuguese without the `d`
 * being read as a day.
 *
 * The names come from `CalendarLocale`, so the pattern decides the *shape* and
 * the locale decides the *words*: `dd/MM/yyyy` with Portuguese names is
 * Brazilian, `MM/dd/yyyy` with English names is American, and neither needs a
 * locale database.
 */
std::string formatDate(const Date& date, std::string_view pattern,
                       const CalendarLocale& locale = {});

/** What the calendar remembers: which month is on screen, which is not the
 *  same as which day is chosen. Owned by the application. */
struct DatePickerState {
    /** The month being shown. Day is ignored. */
    Date visible{};
    /** Whether the popover form is showing. Ignored by the inline calendar,
     *  which is always open by definition. */
    bool open = false;
    /** Where the keyboard is, which moves without choosing — the same split a
     *  select makes between its highlight and its value. */
    Date focusedDay{};
};

struct DatePickerOptions {
    /** Nothing before this, or after that, can be chosen. An invalid date means
     *  no bound. */
    Date minimum{};
    Date maximum{};
    /** Drawn with a dot under it. Invalid draws none. */
    Date today = Date::today();
    CalendarLocale locale{};
    bool showToday = true;
    float cellSize = 30.0f;
    float gap = 2.0f;
};

struct DatePickerResult {
    bool chosen = false;   ///< a day was picked this frame
    Date date{};
};

/**
 * Draws a month and reports the day chosen, if any.
 *
 * Stateless like everything else: `selected` comes in, the state carries only
 * what is *being looked at*. The arrow keys move by a day and Page Up and Page
 * Down by a month, both skipping nothing — a day outside the bounds is drawn
 * and dimmed rather than hidden, because a calendar with holes in it is harder
 * to read than one with unavailable days in it.
 */
DatePickerResult datePicker(Ui& ui, const Interaction& input, std::string_view id,
                            const Date& selected, DatePickerState& state,
                            const DatePickerOptions& options = {});

struct DateFieldOptions : DatePickerOptions {
    /**
     * Shown when nothing is chosen.
     *
     * A field with no value has to say so. Falling back to today would be a
     * lie — "no date" and "today" are different answers, and a form that
     * silently fills one in gets the other submitted.
     */
    std::string_view placeholder = "Select a date…";
    /** How the chosen date is written on the trigger. The same grammar
     *  `formatDate` takes, so the field reads the way the rest of the
     *  application writes dates. */
    std::string_view pattern = "dd/MM/yyyy";
    /** An x on the trigger that puts it back to nothing. */
    bool clearable = true;
    bool disabled = false;
    float width = 200.0f;
    float height = 32.0f;
    bool dismissOnOutsideClick = true;
    bool dismissOnEscape = true;
};

struct DateFieldResult {
    /** The value changed — either a day was picked or it was cleared. */
    bool changed = false;
    /** The new value, which is invalid when it was cleared. */
    Date date{};
};

/**
 * A calendar behind a field, opened in a popover.
 *
 * The counterpart to `colorField`, and the same split: the inline calendar owns
 * its space, this one borrows it. They share the state and the grid.
 */
DateFieldResult dateField(Ui& ui, const Interaction& input, std::string_view id,
                          const Date& selected, DatePickerState& state,
                          const DateFieldOptions& options = {});

}  // namespace gbui
